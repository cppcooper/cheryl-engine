"""Select installed documentation tools or provision a shared, pinned toolchain."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.request
import venv
import zipfile

ROOT = Path(__file__).resolve().parents[2]
REQUIREMENTS = ROOT / "docs/requirements.txt"
NATIVE_TOOLS = Path(__file__).with_name("native-tools.json")
FORMAT_VERSION = 1
MINIMUM_PACKAGES = {
    "sphinx": "8.2.3",
    "breathe": "4.36.0",
    "exhale": "0.3.7",
    "myst-parser": "5.1.0",
    "sphinx-rtd-theme": "3.1.0",
}


class ToolError(RuntimeError):
    pass


def default_cache() -> Path:
    if os.environ.get("CHERYL_DOCS_CACHE_DIR"):
        return Path(os.environ["CHERYL_DOCS_CACHE_DIR"]).expanduser().resolve()
    if sys.platform == "win32":
        base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData/Local"))
    elif sys.platform == "darwin":
        base = Path.home() / "Library/Caches"
    else:
        base = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
    return (base / "cheryl-engine/docs-tools").resolve()


@contextmanager
def file_lock(path: Path, timeout: float = 600.0):
    """Serialize independent build trees using an OS-owned advisory lock."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as handle:
        if os.name == "nt" and handle.seek(0, os.SEEK_END) == 0:
            handle.write(b"\0")
            handle.flush()
        started = time.monotonic()
        next_message = started + 5
        acquired = False
        try:
            while not acquired:
                try:
                    if os.name == "nt":
                        import msvcrt

                        handle.seek(0)
                        msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
                    else:
                        import fcntl

                        fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                    acquired = True
                except (BlockingIOError, PermissionError):
                    now = time.monotonic()
                    if now - started >= timeout:
                        raise ToolError(f"Timed out waiting for documentation tools: {path}")
                    if now >= next_message:
                        print(f"Waiting for documentation tool lock: {path}", file=sys.stderr)
                        next_message = now + 5
                    time.sleep(0.1)
            yield
        finally:
            if acquired:
                if os.name == "nt":
                    import msvcrt

                    handle.seek(0)
                    msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
                else:
                    import fcntl

                    fcntl.flock(handle.fileno(), fcntl.LOCK_UN)


def atomic_json(path: Path, value: dict):
    temporary = path.with_name(f"{path.name}.{os.getpid()}.tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def read_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def locked_versions() -> dict[str, str]:
    versions = {}
    for line in REQUIREMENTS.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^([A-Za-z0-9_.-]+)==([^\s\\;]+)", line)
        if match:
            versions[re.sub(r"[-_.]+", "-", match[1]).lower()] = match[2]
    if not versions or not all(name in versions for name in MINIMUM_PACKAGES):
        raise ToolError("The documentation requirements lock is incomplete")
    return versions


def environment_identity() -> str:
    identity = {
        "format": FORMAT_VERSION,
        "python": str(Path(sys.executable).resolve()),
        "version": sys.version,
        "implementation": sys.implementation.name,
        "abi": sys.implementation.cache_tag,
        "platform": sys.platform,
        "machine": platform.machine().lower(),
        "requirements": hashlib.sha256(REQUIREMENTS.read_bytes()).hexdigest(),
    }
    return hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()[:24]


PROBE = r'''
import importlib
from importlib import metadata
import json
import re
import sys
try:
    from packaging.requirements import Requirement
    from packaging.specifiers import SpecifierSet
    from packaging.version import Version
    settings = json.loads(sys.argv[1])
    versions = {}
    packages = {re.sub(r"[-_.]+", "-", name).lower(): version
                for name, version in settings["packages"].items()}
    pending = [(name, frozenset()) for name in packages]
    visited = set()
    while pending:
        name, extras = pending.pop()
        key = re.sub(r"[-_.]+", "-", name).lower()
        visit = (key, extras)
        if visit in visited:
            continue
        visited.add(visit)
        distribution = metadata.distribution(name)
        version = distribution.version
        requires_python = distribution.metadata.get("Requires-Python")
        python_version = ".".join(map(str, sys.version_info[:3]))
        if requires_python and not SpecifierSet(requires_python).contains(python_version, prereleases=True):
            raise RuntimeError(f"{name} requires Python {requires_python}, found {python_version}")
        wanted = packages.get(key)
        if wanted and (Version(version) != Version(wanted) if settings["exact"]
                       else Version(version) < Version(wanted)):
            raise RuntimeError(f"{name} {version} does not meet {wanted}")
        versions[distribution.metadata["Name"]] = version
        for item in distribution.requires or ():
            requirement = Requirement(item)
            if requirement.marker and not any(
                requirement.marker.evaluate({"extra": extra}) for extra in {"", *extras}
            ):
                continue
            dependency = metadata.version(requirement.name)
            if requirement.specifier and not requirement.specifier.contains(dependency, prereleases=True):
                raise RuntimeError(f"{name} requires {requirement}, found {dependency}")
            pending.append((requirement.name, frozenset(getattr(requirement, "extras", ()))))
    for name in ("sphinx", "breathe", "exhale", "myst_parser", "sphinx_rtd_theme"):
        importlib.import_module(name)
    print(json.dumps({"versions": versions}))
except Exception as error:
    print(json.dumps({"error": str(error)}))
    sys.exit(1)
'''


def probe_python(python: Path, packages: dict[str, str], exact: bool):
    try:
        result = subprocess.run(
            [str(python), "-c", PROBE, json.dumps({"packages": packages, "exact": exact})],
            capture_output=True, text=True, timeout=30,
            env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
        )
        payload = json.loads(result.stdout)
        if result.returncode == 0:
            return payload["versions"]
    except (OSError, ValueError, subprocess.SubprocessError, KeyError):
        pass
    return None


def cached_python(cache: Path, install: bool):
    identity = environment_identity()
    prefix = cache / "python" / identity
    python = prefix / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    marker = prefix / "ready.json"
    expected = locked_versions()

    def ready():
        record = read_json(marker)
        if isinstance(record, dict) and record.get("identity") == identity:
            return probe_python(python, expected, exact=True)
        return None

    versions = ready()
    if versions:
        return python, versions
    if not install:
        raise ToolError("Pinned Python tools need setup; run tools/docs/bootstrap.py setup")
    with file_lock(cache / "locks" / f"python-{identity}.lock"):
        versions = ready()
        if not versions:
            prefix.mkdir(parents=True, exist_ok=True)
            marker.unlink(missing_ok=True)
            print(f"Preparing pinned Python documentation tools: {prefix}", file=sys.stderr)
            installer = pip_installer(cache, expected)
            venv.EnvBuilder(with_pip=False, clear=False).create(prefix)
            subprocess.run(
                [str(python), "-c",
                 "import runpy,sys; sys.path.insert(0,sys.argv.pop(1)); runpy.run_module('pip',run_name='__main__')",
                 str(installer), "--isolated", "install", "--disable-pip-version-check",
                 "--requirement", str(REQUIREMENTS), "--require-hashes", "--only-binary=:all:",
                 "--index-url", "https://pypi.org/simple", "--cache-dir", str(cache / "downloads/pip"),
                 "--force-reinstall"],
                check=True,
            )
            versions = probe_python(python, expected, exact=True)
            if not versions:
                raise ToolError("Installed documentation packages failed version/dependency/import checks")
            atomic_json(marker, {"identity": identity, "versions": versions})
    return python, versions


def native_platform() -> str:
    machine = platform.machine().lower()
    machine = {"amd64": "x86_64", "aarch64": "arm64"}.get(machine, machine)
    system = {"linux": "linux", "win32": "windows", "darwin": "macos"}.get(sys.platform, sys.platform)
    return f"{system}-{machine}"


def doxygen_version(executable: Path):
    try:
        result = subprocess.run([str(executable), "--version"], capture_output=True, text=True, timeout=15)
        match = re.match(r"^(\d+\.\d+\.\d+)(?:\s|$)", result.stdout.strip())
        if result.returncode == 0 and match:
            return match[1]
    except (OSError, subprocess.SubprocessError):
        pass
    return None


def version_tuple(version: str):
    return tuple(int(part) for part in version.split("."))


def validate_doxygen(executable: Path) -> str:
    manifest = read_json(NATIVE_TOOLS)
    if not isinstance(manifest, dict):
        raise ToolError("Missing native documentation tool manifest")
    version = doxygen_version(executable)
    if not version or version_tuple(version) < version_tuple(manifest["minimum_version"]):
        raise ToolError(f"Doxygen {manifest['minimum_version']} or newer is required: {executable}")
    return version


def archive_path(root: Path, name: str) -> Path:
    normalized = PurePosixPath(name.replace("\\", "/"))
    if normalized.is_absolute() or ".." in normalized.parts or any(":" in part for part in normalized.parts):
        raise ToolError(f"Unsafe archive path: {name}")
    target = root.joinpath(*normalized.parts)
    if not target.resolve().is_relative_to(root.resolve()):
        raise ToolError(f"Archive path escapes destination: {name}")
    return target


def extract_archive(archive: Path, destination: Path):
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as bundle:
            for member in bundle.infolist():
                target = archive_path(destination, member.filename)
                mode = member.external_attr >> 16
                if mode & 0o170000 == 0o120000:
                    raise ToolError(f"Archive symbolic link is unsupported: {member.filename}")
                if member.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with bundle.open(member) as source, target.open("wb") as output:
                        shutil.copyfileobj(source, output)
                    if mode & 0o111:
                        target.chmod(0o755)
    else:
        with tarfile.open(archive) as bundle:
            for member in bundle:
                target = archive_path(destination, member.name)
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                elif member.isfile():
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with bundle.extractfile(member) as source, target.open("wb") as output:
                        shutil.copyfileobj(source, output)
                    if member.mode & 0o111:
                        target.chmod(0o755)
                else:
                    raise ToolError(f"Archive link or special file is unsupported: {member.name}")


def download_archive(url: str, destination: Path, expected_hash: str):
    partial = destination.with_name(f"{destination.name}.{os.getpid()}.tmp")
    try:
        with urllib.request.urlopen(url, timeout=60) as source, partial.open("wb") as output:
            shutil.copyfileobj(source, output)
        if hashlib.sha256(partial.read_bytes()).hexdigest() != expected_hash:
            raise ToolError(f"Documentation tool archive failed SHA256 verification: {url}")
        partial.replace(destination)
    finally:
        partial.unlink(missing_ok=True)


def pip_installer(cache: Path, expected: dict[str, str]) -> Path:
    manifest = read_json(NATIVE_TOOLS)
    entry = manifest["python_installer"]
    if entry["version"] != expected.get("pip"):
        raise ToolError("Pinned installer version disagrees with documentation requirements")
    destination = cache / "downloads" / f"pip-{entry['version']}-{entry['sha256'][:16]}.whl"
    with file_lock(cache / "locks" / f"pip-{entry['sha256']}.lock"):
        if not destination.exists() or hashlib.sha256(destination.read_bytes()).hexdigest() != entry["sha256"]:
            destination.parent.mkdir(parents=True, exist_ok=True)
            download_archive(entry["url"], destination, entry["sha256"])
    return destination


def select_doxygen(cache: Path, force_pinned: bool, install: bool):
    manifest = read_json(NATIVE_TOOLS)
    if not isinstance(manifest, dict):
        raise ToolError("Missing native documentation tool manifest")
    if not force_pinned:
        installed = shutil.which("doxygen")
        version = doxygen_version(Path(installed)) if installed else None
        if version and version_tuple(version) >= version_tuple(manifest["minimum_version"]):
            return Path(installed), version
    host = native_platform()
    entry = manifest["platforms"].get(host)
    if not entry:
        raise ToolError(f"No pinned Doxygen binary for {host}; supply a supported system Doxygen")
    identity = f"{host}-{manifest['version']}-{entry['sha256'][:16]}"
    prefix = cache / "native" / identity
    marker = prefix / "ready.json"

    def ready():
        record = read_json(marker)
        if isinstance(record, dict) and record.get("sha256") == entry["sha256"]:
            try:
                if not isinstance(record.get("executable"), str):
                    return None
                executable = archive_path(prefix, record["executable"])
                version = doxygen_version(executable)
                if version == manifest["version"]:
                    return executable, version
            except (KeyError, TypeError, ToolError):
                pass
        return None

    selected = ready()
    if selected:
        return selected
    if not install:
        raise ToolError("Pinned Doxygen needs setup; run tools/docs/bootstrap.py setup")
    with file_lock(cache / "locks" / f"doxygen-{identity}.lock"):
        selected = ready()
        if not selected:
            prefix.mkdir(parents=True, exist_ok=True)
            marker.unlink(missing_ok=True)
            archive = prefix / "download"
            if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != entry["sha256"]:
                print(f"Downloading pinned Doxygen {manifest['version']} for {host}", file=sys.stderr)
                download_archive(entry["url"], archive, entry["sha256"])
            extract_archive(archive, prefix / "files")
            basename = "doxygen.exe" if os.name == "nt" else "doxygen"
            candidates = [path for path in (prefix / "files").rglob(basename) if path.is_file()]
            if len(candidates) != 1:
                raise ToolError(f"Doxygen archive contains {len(candidates)} candidate executables")
            executable = candidates[0]
            if os.name != "nt":
                executable.chmod(0o755)
            version = doxygen_version(executable)
            if version != manifest["version"]:
                raise ToolError("Pinned Doxygen cannot run or has an unexpected version")
            atomic_json(marker, {"sha256": entry["sha256"], "executable": str(executable.relative_to(prefix))})
            selected = executable, version
    return selected


def select_tools(cache: Path, force_pinned: bool = False, install: bool = False):
    doxygen, native_version = select_doxygen(cache, force_pinned, install)
    versions = None if force_pinned else probe_python(Path(sys.executable), MINIMUM_PACKAGES, exact=False)
    if versions:
        python = Path(sys.executable)
        source = "system"
    else:
        python, versions = cached_python(cache, install)
        source = "pinned"
    return {"python": str(python), "python_source": source, "packages": versions,
            "doxygen": str(doxygen), "doxygen_version": native_version}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("setup", "status", "exec"))
    parser.add_argument("--cache-dir", type=Path, default=default_cache())
    parser.add_argument("--force-pinned", action="store_true", help="Use the locked fallback even if system tools qualify")
    arguments, command = parser.parse_known_args(argv)
    if command and command[0] == "--":
        command = command[1:]
    if command and arguments.operation != "exec":
        parser.error("Only exec accepts a command")
    if arguments.operation == "exec" and not command:
        parser.error("exec requires a command after --")
    if sys.version_info < (3, 11):
        raise ToolError("Documentation setup requires Python 3.11 or newer")
    tools = select_tools(arguments.cache_dir.expanduser().resolve(), arguments.force_pinned,
                         install=arguments.operation != "status")
    if arguments.operation != "exec":
        print(json.dumps(tools, indent=2))
        return 0
    if command[0] == "python":
        command[0] = tools["python"]
    python_bin = str(Path(tools["python"]).parent)
    native_bin = str(Path(tools["doxygen"]).parent)
    environment = {**os.environ, "CHERYL_DOCS_TOOLS_ACTIVE": "1",
                   "CHERYL_DOCS_DOXYGEN": tools["doxygen"],
                   "CHERYL_DOCS_TOOLCHAIN": json.dumps(tools),
                   "CHERYL_DOCS_CACHE_DIR": str(arguments.cache_dir.expanduser().resolve()),
                   "PATH": os.pathsep.join((python_bin, native_bin, os.environ.get("PATH", "")))}
    return subprocess.run(command, env=environment).returncode


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ToolError, OSError, subprocess.SubprocessError) as error:
        print(f"Documentation tools: {error}", file=sys.stderr)
        raise SystemExit(1)
