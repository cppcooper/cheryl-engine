"""Configure ordered native ccache layers and save only current-layer additions."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePath
import re
import stat
import subprocess


ROOT = Path(__file__).resolve().parents[2]
PREFIX = re.compile(r"ccache-v2-[A-Za-z0-9][A-Za-z0-9_.-]*")
PULL_REF = re.compile(r"refs/pull/([1-9][0-9]*)/merge")
# Native file-backend keys and CacheEntry v1 result headers in ccache 4.9/4.14.
NATIVE_KEY = re.compile(r"[0-9a-f]{2}/[0-9a-f]{38}")
RESULT_HEADER = b"\xcc\xac\x01\x00"
EMPTY_MARKER = ".cheryl-empty-layer"
METADATA_FILES = {EMPTY_MARKER, "CACHEDIR.TAG"}


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
        raise ValueError("Compiler cache prefix must start with ccache-v2- and contain only letters, numbers, dots, underscores and hyphens")
    for name in ("GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT"):
        value = environment.get(name)
        if not isinstance(value, str) or not re.fullmatch(r"[1-9][0-9]*", value):
            raise ValueError(f"{name} must be a positive integer")
    selected = select_role(environment, event)
    directory = Path(root).absolute() / "build" / "cache" / "compiler"
    role = selected["role"]
    output = {"role": role, "main-path": str(directory / "main"),
              "develop-path": str(directory / "develop"), "current-path": str(directory / role),
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
    for directory in [current, *parents]:
        _files(directory)
        directory.mkdir(parents=True, exist_ok=True)
    stats = Path(root).absolute() / "build" / "ccache"
    _files(stats)
    stats.mkdir(parents=True, exist_ok=True)
    state = {"key": info["current-key"], "path": str(current), "before": fingerprint(current),
             "entries": len(_entries(current))}
    (Path(root) / "build" / "cache" / "compiler-state.json").write_text(json.dumps(state), encoding="utf-8")
    return {"CCACHE_DIR": str(stats), "CCACHE_REMOTE_ONLY": "1",
            "CCACHE_REMOTE_STORAGE": storage_configuration(current, parents),
            "CCACHE_NOFILECLONE": "1", "CCACHE_NOHARDLINK": "1"}


def prepare_save(*, environment=None, event=None, root=ROOT):
    environment, event = _context(environment, event)
    info, current, parents = _layers(environment, event, root)
    state = json.loads((Path(root) / "build" / "cache" / "compiler-state.json").read_text(encoding="utf-8"))
    if not isinstance(state, dict) or state.get("key") != info["current-key"] or state.get("path") != str(current):
        raise ValueError("Compiler cache snapshot does not match this run and layer")
    if not isinstance(state.get("before"), str) or not re.fullmatch(r"[a-f0-9]{64}", state["before"]) or type(state.get("entries")) is not int or state["entries"] < 0:
        raise ValueError("Compiler cache snapshot does not match this run and layer")
    removed = deduplicate(current, parents)
    subprocess.run(["ccache", "--trim-dir", str(current), "--trim-max-size", "1GiB",
                    "--trim-method", "mtime"], check=True)
    after = fingerprint(current)
    entries = _entries(current)
    marker = current / EMPTY_MARKER
    if entries:
        marker.unlink(missing_ok=True)
    elif state["entries"]:
        # Supersede a restored nonempty archive even when all entries moved to a parent.
        marker.write_text("Current compiler cache layer is empty.\n", encoding="utf-8")
    changed = after != state["before"]
    return {"changed": str(changed).lower(), "removed": str(removed)}


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
    describe = commands.add_parser("metadata")
    describe.add_argument("--prefix", required=True)
    commands.add_parser("configure")
    commands.add_parser("prepare-save")
    args = parser.parse_args()
    if args.command == "metadata":
        _emit(metadata(args.prefix), "GITHUB_OUTPUT")
        _emit({"CHERYL_CCACHE_PREFIX": args.prefix}, "GITHUB_ENV")
    elif args.command == "configure":
        _emit(configure(), "GITHUB_ENV")
    else:
        result = prepare_save()
        _emit(result, "GITHUB_OUTPUT")
        print(f"Compiler cache additions: changed={result['changed']}, identical parent entries removed={result['removed']}")


if __name__ == "__main__":
    main()
