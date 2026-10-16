"""Share native ccache layers across compatible jobs and publish their union."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path, PurePath
import re
import shutil
import stat
import tarfile
import tempfile


ROOT = Path(__file__).resolve().parents[2]
PREFIX = re.compile(r"ccache-v3-(?:Linux-[A-Za-z0-9_]+-linux-(?:gcc14|clang18)|Windows-[A-Za-z0-9_]+-windows-msvc)")
LEGACY_PREFIXES = {
    "linux-gcc14": ("ubuntu-24.04", ("linux-gcc-hwloc-Release", "linux-gcc-neutral-Release",
                                    "linux-modules-Release", "linux-modules-shared-Release")),
    "linux-clang18": ("ubuntu-24.04", ("linux-clang-debug-Debug",)),
    "windows-msvc": ("windows-2025", ("windows-msvc-hwloc-RelWithDebInfo",
                                      "windows-modules-RelWithDebInfo", "windows-modules-shared-RelWithDebInfo")),
}
PULL_REF = re.compile(r"refs/pull/([1-9][0-9]*)/merge")
# Native file-backend keys and CacheEntry v1 result headers in ccache 4.9/4.14.
NATIVE_KEY = re.compile(r"[0-9a-f]{2}/[0-9a-f]{38}")
RESULT_HEADER = b"\xcc\xac\x01\x00"
EMPTY_MARKER = ".cheryl-empty-layer"
METADATA_FILES = {EMPTY_MARKER, "CACHEDIR.TAG"}
LAYER_LIMIT = 1024 ** 3
ARCHIVE_ENTRY_LIMIT = 200_000


def _valid_ref(ref):
    return (isinstance(ref, str) and ref.startswith("refs/") and not ref.endswith(("/", "."))
            and not any(char.isspace() or ord(char) < 32 or ord(char) == 127 or char in "~^:?*[\\" for char in ref)
            and not any(part in ref for part in ("..", "//", "@{"))
            and not any(part.startswith(".") or part.endswith(".lock") for part in ref.split("/")))


def select_role(environment, event):
    ref = environment.get("GITHUB_REF", "")
    if not _valid_ref(ref):
        raise ValueError("GITHUB_REF must be a valid full Git reference")
    if environment.get("GITHUB_EVENT_NAME") == "pull_request":
        match = PULL_REF.fullmatch(ref)
        number = event.get("number") if isinstance(event, dict) else None
        pull = event.get("pull_request") if isinstance(event, dict) else None
        base_info = pull.get("base") if isinstance(pull, dict) else None
        base = base_info.get("ref") if isinstance(base_info, dict) else None
        if not match or type(number) is not int or number != int(match.group(1)) or not isinstance(base, str) or not _valid_ref("refs/heads/" + base):
            raise ValueError("Pull request cache scope must match the event number and base branch")
        return {"role": "pr", "restore-main": True, "restore-develop": base == "develop"}
    if not ref.startswith(("refs/heads/", "refs/tags/")) or not ref.rsplit("/", 1)[-1]:
        raise ValueError("Compiler caches require a branch, tag or pull request merge reference")
    if ref == "refs/heads/main":
        return {"role": "main", "restore-main": False, "restore-develop": False}
    if ref == "refs/heads/develop":
        return {"role": "develop", "restore-main": True, "restore-develop": False}
    return {"role": "branch", "restore-main": True, "restore-develop": False}


def _context(environment, event):
    environment = os.environ if environment is None else environment
    if event is None:
        event = {}
        if environment.get("GITHUB_EVENT_NAME") == "pull_request":
            with Path(environment["GITHUB_EVENT_PATH"]).open(encoding="utf-8") as source:
                event = json.load(source)
    return environment, event


def metadata(prefix, *, environment=None, event=None, root=ROOT):
    environment, event = _context(environment, event)
    if not isinstance(prefix, str) or not PREFIX.fullmatch(prefix):
        raise ValueError("Compiler cache prefix must identify a supported ccache-v3 runner, architecture and compiler family")
    for name in ("GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT"):
        value = environment.get(name)
        if not isinstance(value, str) or not re.fullmatch(r"[1-9][0-9]*", value):
            raise ValueError(f"{name} must be a positive integer")
    selected = select_role(environment, event)
    directory = Path(root).absolute() / "build" / "cache" / "compiler"
    role = selected["role"]
    output = {"role": role, "main-path": str(directory / "main"),
              "develop-path": str(directory / "develop"), "current-path": str(directory / role),
              "delta-path": str(directory / "delta"),
              "delta-archive": str(directory.parent / "compiler-artifacts" / "delta.tar"),
              "main-prefix": f"{prefix}-main-", "develop-prefix": f"{prefix}-develop-",
              "current-prefix": f"{prefix}-{role}-"}
    output["current-key"] = output["current-prefix"] + environment["GITHUB_RUN_ID"] + "-" + environment["GITHUB_RUN_ATTEMPT"]
    output.update({name: str(selected[name]).lower() for name in ("restore-main", "restore-develop")})
    return output


def file_url(path):
    return (path if isinstance(path, PurePath) else Path(path)).as_uri()


def storage_configuration(current, parents):
    return " ".join([file_url(current) + "|update-mtime=true",
                     *(file_url(parent) + "|read-only" for parent in parents)])


def _files(directory):
    directory = Path(directory).absolute()
    for ancestor in (directory, *directory.parents):
        if ancestor.is_symlink() or (hasattr(ancestor, "is_junction") and ancestor.is_junction()):
            raise ValueError(f"Compiler cache paths must not contain symbolic links: {ancestor}")
    if not directory.exists():
        return {}
    if not directory.is_dir():
        raise ValueError(f"Compiler cache path must be a directory: {directory}")
    files = {}
    for parent, directories, names in os.walk(directory, followlinks=False):
        for name in sorted(directories + names):
            path = Path(parent) / name
            mode = path.lstat().st_mode
            if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
                raise ValueError(f"Compiler cache paths must not contain symbolic links: {path}")
            if stat.S_ISREG(mode):
                files[path.relative_to(directory).as_posix()] = path
            elif not stat.S_ISDIR(mode):
                raise ValueError(f"Compiler cache contains a non-regular file: {path}")
    return files


def _digest(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.digest()


def _entries(directory):
    return {name: path for name, path in _files(directory).items() if name not in METADATA_FILES}


def fingerprint(directory):
    digest = hashlib.sha256()
    for name, path in sorted(_entries(directory).items()):
        encoded = name.encode("utf-8")
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)
        digest.update(_digest(path))
    return digest.hexdigest()


def _is_result(path):
    with path.open("rb") as source:
        return source.read(len(RESULT_HEADER)) == RESULT_HEADER


def deduplicate(current, parents):
    current_files = _entries(current)
    parent_files = [_entries(parent) for parent in parents]
    removed = 0
    for name, path in current_files.items():
        parent = next((files[name] for files in parent_files if name in files), None)
        if parent is None:
            continue
        # Result keys identify compiler inputs; serialized headers can differ by creation time.
        same_result = NATIVE_KEY.fullmatch(name) and _is_result(path) and _is_result(parent)
        if same_result or (path.stat().st_size == parent.stat().st_size and _digest(path) == _digest(parent)):
            path.unlink()
            removed += 1
    return removed


def _layers(environment, event, root):
    info = metadata(environment.get("CHERYL_CCACHE_PREFIX"), environment=environment, event=event, root=root)
    parents = []
    if info["restore-develop"] == "true":
        parents.append(Path(info["develop-path"]))
    if info["restore-main"] == "true":
        parents.append(Path(info["main-path"]))
    return info, Path(info["current-path"]), parents


def configure(*, environment=None, event=None, root=ROOT):
    environment, event = _context(environment, event)
    info, current, parents = _layers(environment, event, root)
    delta = Path(info["delta-path"])
    for directory in [delta, current, *parents]:
        _files(directory)
        directory.mkdir(parents=True, exist_ok=True)
    if _entries(delta):
        raise ValueError("A compiler worker must start with a fresh writable delta")
    stats = Path(root).absolute() / "build" / "ccache"
    _files(stats)
    stats.mkdir(parents=True, exist_ok=True)
    return {"CCACHE_DIR": str(stats), "CCACHE_REMOTE_ONLY": "1",
            "CCACHE_REMOTE_STORAGE": storage_configuration(delta, [current, *parents]),
            "CCACHE_NOFILECLONE": "1", "CCACHE_NOHARDLINK": "1"}


def _native_entries(directory):
    entries = _entries(directory)
    if any(not NATIVE_KEY.fullmatch(name) for name in entries):
        raise ValueError("Compiler archives must contain only native file-backend cache entries")
    return entries


def trim(directory, *, limit=LAYER_LIMIT):
    entries = _native_entries(directory)
    sizes = {name: path.stat().st_size for name, path in entries.items()}
    total = sum(sizes.values())
    removed = 0
    for name, path in sorted(entries.items(), key=lambda item: (item[1].stat().st_mtime_ns, item[0])):
        if total <= limit:
            break
        path.unlink()
        total -= sizes[name]
        removed += 1
    return removed


def merge(destination, source):
    """Union results by native key; keep one complete, newest manifest per key."""
    destination_files = _native_entries(destination)
    source_files = _native_entries(source)
    for name, path in sorted(source_files.items()):
        target = destination_files.get(name)
        if target is not None:
            existing, incoming = target.stat(), path.stat()
            if _is_result(target) and _is_result(path):
                # The same result key can have different serialization timestamps.
                if incoming.st_mtime_ns > existing.st_mtime_ns:
                    os.utime(target, ns=(existing.st_atime_ns, incoming.st_mtime_ns))
                continue
            if incoming.st_mtime_ns < existing.st_mtime_ns:
                continue
        target = Path(destination) / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)


def pack(output, *, environment=None, event=None, root=ROOT):
    environment, event = _context(environment, event)
    info, current, parents = _layers(environment, event, root)
    delta = Path(info["delta-path"])
    _native_entries(delta)
    removed = deduplicate(delta, [current, *parents])
    trimmed = trim(delta)
    entries = _native_entries(delta)
    output = Path(output).absolute()
    if output != Path(info["delta-archive"]):
        raise ValueError("Compiler delta archive must use the planned output path")
    _files(output.parent)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)
    if entries:
        # Cache entries are already compressed; tar preserves their LRU timestamps.
        with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
            for name, path in sorted(entries.items()):
                member = tarfile.TarInfo(name)
                status = path.stat()
                member.size = status.st_size
                member.mtime = status.st_mtime
                member.mode = 0o644
                with path.open("rb") as source:
                    archive.addfile(member, source)
    return {"has-content": str(bool(entries)).lower(), "removed": str(removed), "trimmed": str(trimmed)}


def _archive_members(archive):
    members = []
    seen = set()
    total = 0
    for member in archive:
        if (not member.isfile() or not NATIVE_KEY.fullmatch(member.name) or member.name in seen
                or member.size < 0 or not math.isfinite(member.mtime) or member.mtime < 0):
            raise ValueError("Compiler delta archive contains an invalid native entry")
        total += member.size
        if total > LAYER_LIMIT or len(members) >= ARCHIVE_ENTRY_LIMIT:
            raise ValueError("Compiler delta archive exceeds the layer limits")
        seen.add(member.name)
        members.append(member)
    return members


def _merge_archives(current, directory):
    archives = sorted(_files(directory).values())
    # Validate every contribution before changing the restored snapshot.
    for path in archives:
        if path.name != "delta.tar":
            raise ValueError("Expected a compiler delta.tar artifact")
        with tarfile.open(path, "r:") as archive:
            _archive_members(archive)
    for path in archives:
        with tarfile.open(path, "r:") as archive:
            members = _archive_members(archive)
            with tempfile.TemporaryDirectory(prefix="compiler-delta-", dir=current.parent) as temporary:
                incoming = Path(temporary)
                for member in members:
                    target = incoming / member.name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with archive.extractfile(member) as source, target.open("wb") as output:
                        shutil.copyfileobj(source, output)
                    os.utime(target, (member.mtime, member.mtime))
                merge(current, incoming)
    return len(archives)


def migration_plan(prefix, *, environment=None, event=None, root=ROOT):
    environment, event = _context(environment, event)
    info, current, _ = _layers(environment, event, root)
    selected = environment["CHERYL_CCACHE_PREFIX"]
    family = next(family for family in LEGACY_PREFIXES if selected.endswith("-" + family))
    runner, profiles = LEGACY_PREFIXES[family]
    architecture = selected.split("-")[3]
    allowed = {f"ccache-v2-{runner}-{architecture}-{profile}" for profile in profiles}
    if prefix not in allowed:
        raise ValueError("Legacy compiler cache must belong to the selected runner and compiler family")
    legacy_prefix = f"{prefix}-{info['role']}-"
    return {"path": str(current), "prefix": legacy_prefix,
            "key": legacy_prefix + environment["GITHUB_RUN_ID"] + "-" + environment["GITHUB_RUN_ATTEMPT"]}


def import_legacy(*, environment=None, event=None, root=ROOT):
    environment, event = _context(environment, event)
    _, current, _ = _layers(environment, event, root)
    staging = current.parent / "migration"
    merge(staging, current)
    entries = _native_entries(current)
    for path in entries.values():
        path.unlink()
    for name in METADATA_FILES:
        (current / name).unlink(missing_ok=True)
    return {"imported": str(len(entries))}


def publish(deltas, *, environment=None, event=None, root=ROOT):
    environment, event = _context(environment, event)
    _, current, parents = _layers(environment, event, root)
    _native_entries(current)
    current.mkdir(parents=True, exist_ok=True)
    before = fingerprint(current)
    initial_snapshot = bool(_native_entries(current)) or (current / EMPTY_MARKER).exists()
    merge(current, current.parent / "migration")
    archives = _merge_archives(current, deltas)
    removed = deduplicate(current, parents)
    trimmed = trim(current)
    entries = _native_entries(current)
    marker = current / EMPTY_MARKER
    if entries:
        marker.unlink(missing_ok=True)
    else:
        # Remember an initialized empty layer so legacy imports are not repeated.
        marker.write_text("Current compiler cache layer is empty.\n", encoding="utf-8")
    return {"changed": str(fingerprint(current) != before or not initial_snapshot).lower(),
            "has-content": str(bool(entries) or marker.exists()).lower(),
            "removed": str(removed), "trimmed": str(trimmed), "archives": str(archives)}


def _emit(values, destination):
    for name, value in values.items():
        if any(char in str(value) for char in "\r\n"):
            raise ValueError("Compiler cache output values must be single-line")
    with Path(os.environ[destination]).open("a", encoding="utf-8") as output:
        for name, value in values.items():
            output.write(f"{name}={value}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    describe = commands.add_parser("plan")
    describe.add_argument("--prefix", required=True)
    commands.add_parser("attach")
    package = commands.add_parser("pack")
    package.add_argument("--output", required=True)
    publication = commands.add_parser("publish")
    publication.add_argument("--deltas", required=True)
    migration = commands.add_parser("migration-plan")
    migration.add_argument("--prefix", required=True)
    commands.add_parser("import")
    args = parser.parse_args()
    if args.command == "plan":
        _emit(metadata(args.prefix), "GITHUB_OUTPUT")
        _emit({"CHERYL_CCACHE_PREFIX": args.prefix}, "GITHUB_ENV")
    elif args.command == "attach":
        _emit(configure(), "GITHUB_ENV")
    else:
        if args.command == "pack":
            result = pack(args.output)
        elif args.command == "publish":
            result = publish(args.deltas)
        elif args.command == "migration-plan":
            result = migration_plan(args.prefix)
        else:
            result = import_legacy()
        _emit(result, "GITHUB_OUTPUT")
        print("Compiler cache " + args.command + ": " + json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
