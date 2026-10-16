"""Pack Windows vcpkg additions and maintain one bounded shared package pool."""

import argparse
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import shutil
import stat
import tarfile
import tempfile
import time


ROOT = Path(__file__).resolve().parents[2]
CACHE_PREFIX = "vcpkg-v1-Windows-X64-msvc-"
GIB = 1024 ** 3
DEFAULT_LIMIT = 2 * GIB
CONTRIBUTION_LIMIT = 2 * GIB
METADATA_LIMIT = 16 * 1024 ** 2
FILE_LIMIT = 200_000
TAR_LIMIT = CONTRIBUTION_LIMIT + METADATA_LIMIT + FILE_LIMIT * 1024 + 10240
CONTRIBUTION_COUNT = 32
AGGREGATE_LIMIT = 3 * GIB
INDEX_NAME = "pool-index.json"
MANIFEST_NAME = "contribution.json"
ABI = re.compile(r"[0-9a-f]{64}")
# Pinned vcpkg-tool 2026-09-26: binarycaching.cpp files_archive_subpath().
ARCHIVE = re.compile(r"archives/([0-9a-f]{2})/([0-9a-f]{64})\.zip")
SEGMENT = re.compile(r"[A-Za-z0-9_.+@=(){}\[\],~-]+")
RESERVED = re.compile(r"(?:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?", re.IGNORECASE)
CREDENTIAL_NAMES = {".git", ".ssh", ".aws", ".azure", ".config", ".netrc", "_netrc", ".npmrc",
                    ".pypirc", ".git-credentials", "credentials", "credentials.json", "nuget.config",
                    "id_rsa", "id_ed25519", "hosts.yml", "access_token", "token", "tokens.json"}


class _BoundedTarInfo(tarfile.TarInfo):
    def _check_extension(self):
        if not 0 <= self.size <= METADATA_LIMIT:
            raise ValueError("vcpkg tar extended header exceeds its metadata limit")

    def _proc_pax(self, archive):
        self._check_extension()
        return super()._proc_pax(archive)

    def _proc_gnulong(self, archive):
        self._check_extension()
        return super()._proc_gnulong(archive)


def describe(*, environment=None, root=ROOT):
    environment = os.environ if environment is None else environment
    identity = []
    for name in ("GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT"):
        value = environment.get(name)
        if not isinstance(value, str) or not re.fullmatch(r"[1-9][0-9]*", value):
            raise ValueError(f"{name} must be a positive integer")
        identity.append(value)
    directory = Path(root).absolute() / "build/cache"
    return {"cache-path": str(directory / "vcpkg"), "cache-prefix": CACHE_PREFIX,
            "cache-key": CACHE_PREFIX + "-".join(identity),
            "contribution-path": str(directory / "vcpkg-artifacts/contribution.tar")}


def _pool(root):
    return Path(root).absolute() / "build/cache/vcpkg"


def _no_links(path):
    path = Path(path).absolute()
    for ancestor in (path, *path.parents):
        if ancestor.is_symlink() or (hasattr(ancestor, "is_junction") and ancestor.is_junction()):
            raise ValueError(f"vcpkg cache paths must not contain links: {ancestor}")


def _path(name):
    if not isinstance(name, str) or not name or len(name) > 1024:
        raise ValueError("Invalid vcpkg cache relative path")
    parts = name.split("/")
    for part in parts:
        if (part in {"", ".", ".."} or not SEGMENT.fullmatch(part) or part.endswith(".")
                or RESERVED.fullmatch(part) or part.casefold() in CREDENTIAL_NAMES):
            raise ValueError(f"Invalid vcpkg cache relative path: {name!r}")
    return parts


def _payload_path(name):
    parts = _path(name)
    if parts[0] == "archives":
        match = ARCHIVE.fullmatch(name)
        if not match or match[1] != match[2][:2]:
            raise ValueError(f"Invalid native vcpkg binary archive path: {name!r}")
    elif parts[0] != "downloads" or len(parts) < 2:
        raise ValueError(f"Unexpected vcpkg cache payload path: {name!r}")
    return name


def _register(name, names):
    parts = _path(name)
    for length in range(1, len(parts) + 1):
        prefix = "/".join(parts[:length])
        folded = prefix.casefold()
        if folded in names and names[folded] != prefix:
            raise ValueError(f"vcpkg cache has a case-insensitive path collision: {prefix!r}")
        names[folded] = prefix


def _files(directory):
    directory = Path(directory).absolute()
    _no_links(directory)
    if not directory.exists():
        return {}
    if not directory.is_dir():
        raise ValueError(f"Expected a vcpkg cache directory: {directory}")
    files = {}
    names = {}
    for parent, directories, entries in os.walk(directory, followlinks=False):
        directories.sort()
        for name in sorted(directories + entries):
            path = Path(parent) / name
            _no_links(path)
            relative = path.relative_to(directory).as_posix()
            _register(relative, names)
            mode = path.lstat().st_mode
            if stat.S_ISREG(mode):
                files[relative] = path
                if len(files) > FILE_LIMIT:
                    raise ValueError("vcpkg cache exceeds the file-count limit")
            elif not stat.S_ISDIR(mode):
                raise ValueError(f"vcpkg cache contains a non-regular file: {relative!r}")
    return files


def _digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def _encode(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def _pairs(items):
    result = {}
    for name, value in items:
        if name in result:
            raise ValueError(f"Duplicate vcpkg metadata key: {name!r}")
        result[name] = value
    return result


def _decode(data):
    if len(data) > METADATA_LIMIT:
        raise ValueError("vcpkg metadata exceeds its size limit")
    try:
        return json.loads(data.decode("utf-8"), object_pairs_hook=_pairs)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("Invalid vcpkg metadata JSON") from error


def _records(value, *, indexed):
    if not isinstance(value, dict) or len(value) > FILE_LIMIT:
        raise ValueError("Invalid vcpkg file metadata")
    names = {}
    for name, record in value.items():
        _payload_path(name)
        _register(name, names)
        fields = {"sha256", "size", "last_used"} if indexed else {"sha256", "size"}
        if (not isinstance(record, dict) or set(record) != fields
                or not isinstance(record["sha256"], str) or not ABI.fullmatch(record["sha256"])
                or type(record["size"]) is not int or not 0 <= record["size"] <= CONTRIBUTION_LIMIT):
            raise ValueError(f"Invalid vcpkg file metadata: {name!r}")
        if indexed and (type(record["last_used"]) is not int or not 0 <= record["last_used"] <= 1_000_000_000):
            raise ValueError(f"Invalid vcpkg last-use checkpoint: {name!r}")
    for name in value:
        parts = name.split("/")
        if any("/".join(parts[:length]) in value for length in range(1, len(parts))):
            raise ValueError(f"vcpkg file is also used as a directory: {name!r}")
    return value


def inventory(directory):
    result = {}
    for name, path in _files(directory).items():
        if name == INDEX_NAME:
            continue
        _payload_path(name)
        size = path.stat().st_size
        if size > CONTRIBUTION_LIMIT:
            raise ValueError(f"vcpkg cache file exceeds the contribution limit: {name!r}")
        result[name] = {"sha256": _digest(path), "size": size}
    return result


def read_index(directory):
    path = Path(directory) / INDEX_NAME
    _no_links(path)
    if not path.exists():
        return {"schema": 1, "files": {}}
    if not path.is_file() or path.stat().st_size > METADATA_LIMIT:
        raise ValueError("Invalid vcpkg pool index")
    value = _decode(path.read_bytes())
    if not isinstance(value, dict) or set(value) != {"schema", "files"} or type(value["schema"]) is not int or value["schema"] != 1:
        raise ValueError("Unsupported vcpkg pool index schema")
    _records(value["files"], indexed=True)
    return value


def parse_status(text):
    """Read core-package ABI identities from vcpkg's Debian-style status file."""
    if not isinstance(text, str) or "\0" in text or len(text.encode("utf-8")) > METADATA_LIMIT:
        raise ValueError("Invalid vcpkg installed status")
    paragraphs = []
    fields = {}
    previous = None
    for line in text.splitlines() + [""]:
        if not line:
            if fields:
                paragraphs.append(fields)
            fields, previous = {}, None
        elif line[0].isspace():
            if previous is None:
                raise ValueError("Invalid vcpkg status continuation")
            fields[previous] += "\n" + line
        else:
            name, separator, value = line.partition(":")
            if not separator or not re.fullmatch(r"[A-Za-z][A-Za-z0-9-]*", name) or name in fields:
                raise ValueError("Invalid vcpkg status field")
            fields[name], previous = value.strip(), name
    used = set()
    for fields in paragraphs:
        if fields.get("Status") not in {"install ok installed", "hold ok installed"} or fields.get("Feature"):
            continue
        abi = fields.get("Abi", "")
        if abi:
            if not ABI.fullmatch(abi):
                raise ValueError("Invalid installed vcpkg package ABI")
            used.add(f"archives/{abi[:2]}/{abi}.zip")
    return sorted(used)


def used_archives(installed):
    path = Path(installed).absolute() / "vcpkg/status"
    _no_links(path)
    if not path.is_file() or path.stat().st_size > METADATA_LIMIT:
        raise ValueError("Expected a bounded installed vcpkg status file")
    return parse_status(path.read_text(encoding="utf-8"))


def pack(installed, *, root=ROOT):
    pool = _pool(root)
    previous = read_index(pool)["files"]
    current = inventory(pool)
    changed = {name: record for name, record in current.items()
               if any(previous.get(name, {}).get(field) != record[field] for field in ("size", "sha256"))}
    if sum(record["size"] for record in changed.values()) > CONTRIBUTION_LIMIT:
        raise ValueError("vcpkg contribution exceeds its payload limit")
    used = used_archives(installed)
    manifest = {"schema": 1, "files": changed, "used": used}
    encoded = _encode(manifest)
    if len(encoded) > METADATA_LIMIT:
        raise ValueError("vcpkg contribution metadata exceeds its size limit")
    output = Path(root).absolute() / "build/cache/vcpkg-artifacts/contribution.tar"
    _no_links(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)
    if changed or used:
        with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
            member = tarfile.TarInfo(MANIFEST_NAME)
            member.size, member.mode = len(encoded), 0o644
            archive.addfile(member, io.BytesIO(encoded))
            for name, record in sorted(changed.items()):
                member = tarfile.TarInfo(name)
                member.size, member.mode = record["size"], 0o644
                with (pool / name).open("rb") as source:
                    archive.addfile(member, source)
    return {"has-content": str(bool(changed or used)).lower(), "files": str(len(changed)), "used": str(len(used))}


def _manifest(value):
    if (not isinstance(value, dict) or set(value) != {"schema", "files", "used"}
            or type(value["schema"]) is not int or value["schema"] != 1):
        raise ValueError("Unsupported vcpkg contribution schema")
    _records(value["files"], indexed=False)
    used = value["used"]
    if not isinstance(used, list) or len(used) > FILE_LIMIT:
        raise ValueError("Invalid vcpkg contribution usage metadata")
    seen = set()
    for name in used:
        _payload_path(name)
        if not ARCHIVE.fullmatch(name) or name in seen:
            raise ValueError("Invalid or duplicate vcpkg contribution archive usage")
        seen.add(name)
    return value


def _contribution(path, destination):
    if path.stat().st_size > TAR_LIMIT:
        raise ValueError("vcpkg contribution tar exceeds its size limit")
    with tarfile.open(path, "r:", tarinfo=_BoundedTarInfo) as archive:
        members, names = {}, {}
        size = 0
        for member in archive:
            _register(member.name, names)
            if (not member.isfile() or member.name in members or member.size < 0
                    or not math.isfinite(member.mtime) or member.mtime < 0):
                raise ValueError(f"Invalid vcpkg contribution member: {member.name!r}")
            if member.name == MANIFEST_NAME:
                if member.size > METADATA_LIMIT:
                    raise ValueError("vcpkg contribution metadata exceeds its size limit")
            else:
                _payload_path(member.name)
                size += member.size
            if len(members) >= FILE_LIMIT + 1 or size > CONTRIBUTION_LIMIT:
                raise ValueError("vcpkg contribution exceeds its payload or file-count limit")
            members[member.name] = member
        if MANIFEST_NAME not in members:
            raise ValueError("vcpkg contribution is missing its manifest")
        with archive.extractfile(members[MANIFEST_NAME]) as source:
            manifest = _manifest(_decode(source.read()))
        if set(members) - {MANIFEST_NAME} != set(manifest["files"]):
            raise ValueError("vcpkg contribution files do not match its manifest")
        for name, record in sorted(manifest["files"].items()):
            member = members[name]
            if member.size != record["size"]:
                raise ValueError(f"vcpkg contribution size mismatch: {name!r}")
            target = destination / name
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.extractfile(member) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output)
            if _digest(target) != record["sha256"]:
                raise ValueError(f"vcpkg contribution digest mismatch: {name!r}")
    return manifest


def trim(index, *, limit_bytes=DEFAULT_LIMIT, used=frozenset()):
    """Return an index within the byte cap, preferring the observed working set."""
    if type(limit_bytes) is not int or limit_bytes <= 0:
        raise ValueError("The vcpkg pool byte limit must be positive")
    files = {name: dict(record) for name, record in index["files"].items()}
    result = {"schema": 1, "files": files}
    oversized = {name for name, record in files.items()
                 if record["size"] + len(_encode({"schema": 1, "files": {name: record}})) > limit_bytes}
    if files and len(oversized) == len(files):
        raise ValueError("The vcpkg pool limit cannot retain even one entry")
    for name in oversized:
        del files[name]
    total = sum(record["size"] for record in files.values())
    candidates = sorted(files, key=lambda name: (name in used, files[name]["last_used"], name))
    for name in candidates:
        if total + len(_encode(result)) <= limit_bytes:
            break
        if len(files) == 1:
            raise ValueError("The vcpkg pool limit cannot retain even one remaining entry")
        total -= files.pop(name)["size"]
    if total + len(_encode(result)) > limit_bytes:
        raise ValueError("The vcpkg pool metadata exceeds the byte limit")
    return result


def merge(contributions, *, root=ROOT, limit_bytes=DEFAULT_LIMIT, now=None):
    pool = _pool(root)
    original = read_index(pool)
    current = inventory(pool)
    indexed = original["files"]
    if (pool / INDEX_NAME).exists() and (set(current) != set(indexed) or any(
            any(record[field] != indexed[name][field] for field in ("sha256", "size"))
            for name, record in current.items())):
        raise ValueError("The restored vcpkg pool does not match its index")
    timestamp = time.time() if now is None else now
    if type(timestamp) not in {int, float} or not math.isfinite(timestamp) or timestamp < 0:
        raise ValueError("The vcpkg publisher time must be finite and nonnegative")
    day = int(timestamp // 86400)
    result = {"schema": 1, "files": {name: {**record, "last_used": indexed.get(name, {}).get("last_used", 0)}
                                      for name, record in current.items()}}
    archives = _files(contributions)
    if len(archives) > CONTRIBUTION_COUNT or sum(path.stat().st_size for path in archives.values()) > AGGREGATE_LIMIT:
        raise ValueError("vcpkg contributions exceed their aggregate count or size limit")
    if any(path.name != "contribution.tar" for path in archives.values()):
        raise ValueError("Expected only contribution.tar vcpkg artifacts")
    used = set()
    sources = {}
    names = {}
    for name in current:
        _register(name, names)
    # Validate every artifact, including hashes, before changing the restored pool.
    with tempfile.TemporaryDirectory(prefix="cheryl-vcpkg-contributions-") as temporary:
        for number, path in enumerate(sorted(archives.values())):
            staging = Path(temporary) / str(number)
            manifest = _contribution(path, staging)
            used.update(manifest["used"])
            for name, record in sorted(manifest["files"].items()):
                _register(name, names)
                if name.startswith("archives/") and name in result["files"]:
                    # ABI identity is stable even when independent ZIP serializations differ.
                    result["files"][name]["last_used"] = day
                    continue
                result["files"][name] = {**record, "last_used": day}
                if current.get(name) != record:
                    sources[name] = staging / name
                else:
                    sources.pop(name, None)
        for name in used:
            if name in result["files"]:
                result["files"][name]["last_used"] = day
        _records(result["files"], indexed=True)
        retained_before_trim = len(result["files"])
        result = trim(result, limit_bytes=limit_bytes, used=used)
        encoded = _encode(result)
        if len(encoded) > METADATA_LIMIT:
            raise ValueError("vcpkg pool index exceeds its size limit")
        pool.mkdir(parents=True, exist_ok=True)
        for name, source in sorted(sources.items()):
            if name in result["files"]:
                target = pool / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
        removed = set(current) - set(result["files"])
        for name in sorted(removed):
            (pool / name).unlink()
        changed = original != result or not (pool / INDEX_NAME).exists()
        if changed:
            temporary_index = pool / "pool-index.tmp"
            temporary_index.write_bytes(encoded)
            temporary_index.replace(pool / INDEX_NAME)
    return {"changed": str(changed).lower(), "files": str(len(result["files"])),
            "removed": str(retained_before_trim - len(result["files"])),
            "bytes": str(sum(record["size"] for record in result["files"].values()) + len(encoded))}


def _emit(values, variable):
    if any("\r" in str(value) or "\n" in str(value) for value in values.values()):
        raise ValueError("vcpkg cache outputs must be single-line")
    with Path(os.environ[variable]).open("a", encoding="utf-8") as output:
        for name, value in values.items():
            output.write(f"{name}={value}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("describe")
    package = commands.add_parser("pack")
    package.add_argument("--installed", type=Path, required=True)
    publication = commands.add_parser("merge")
    publication.add_argument("--contributions", type=Path, required=True)
    publication.add_argument("--limit-gib", type=float, default=2)
    args = parser.parse_args()
    if args.command == "describe":
        result = describe()
        _emit({"CHERYL_CI_VCPKG_CACHE_DIR": result["cache-path"]}, "GITHUB_ENV")
    elif args.command == "pack":
        result = pack(args.installed)
    else:
        if not math.isfinite(args.limit_gib) or args.limit_gib <= 0:
            parser.error("--limit-gib must be finite and positive")
        result = merge(args.contributions, limit_bytes=int(args.limit_gib * GIB))
    _emit(result, "GITHUB_OUTPUT")
    print("vcpkg cache " + args.command + ": " + json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
