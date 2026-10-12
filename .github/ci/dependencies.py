"""Prepare CI dependencies and describe reusable dependency caches."""

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
CCACHE_VERSION = "4.14.0"
APT_COMMON = ["ccache", "ninja-build", "pkg-config", "g++-14", "libfreetype-dev", "libharfbuzz-dev",
              "libicu-dev", "libhwloc-dev", "libdw-dev", "libelf-dev", "fonts-urw-base35"]
APT_EXTRA = {
    "headless": ["clang-18", "clang-tools-18"],
    "modules": ["libgl-dev", "libx11-dev", "libxrandr-dev", "libxinerama-dev", "libxcursor-dev",
                "libxi-dev", "libxext-dev", "python3-jinja2"],
}


def run(command, *, capture=False, cwd=ROOT, environment=None):
    print("+ " + subprocess.list2cmdline(list(map(str, command))), flush=True)
    result = subprocess.run(command, cwd=cwd, env=environment, check=False,
                            capture_output=capture, text=capture)
    if capture and result.returncode:
        print(result.stdout, end="", flush=True)
        print(result.stderr, end="", file=sys.stderr, flush=True)
    result.check_returncode()
    return result.stdout if capture else None


def emit(variable, values):
    with Path(os.environ[variable]).open("a", encoding="utf-8") as output:
        for name, value in values.items():
            output.write(f"{name}={value}\n")


def digest(value):
    return hashlib.sha256(value).hexdigest()


def metadata(profile):
    runner = f"{os.environ['RUNNER_OS']}-{os.environ['RUNNER_ARCH']}"
    manifest = (ROOT / ".github/ci/vcpkg.json").read_bytes()
    baseline = json.loads(manifest)["builtin-baseline"]
    requirements = (ROOT / ".github/ci/requirements-modules.txt").read_bytes()
    entries = subprocess.check_output(["git", "ls-tree", "-rz", "HEAD"], cwd=ROOT).split(b"\0")
    links = sorted(entry for entry in entries if entry.startswith(b"160000 "))
    submodules = digest((ROOT / ".gitmodules").read_bytes() + b"\0" + b"\0".join(links))
    modules_path = Path(run(["git", "rev-parse", "--git-path", "modules"], capture=True).strip())
    if not modules_path.is_absolute():
        modules_path = ROOT / modules_path
    cache = ROOT / "build/cache/dependencies"
    vcpkg = Path(os.environ["RUNNER_TEMP"]).resolve() / "vcpkg"
    ccache = ROOT / "build/cache/tools/ccache"
    dependency_prefix = f"dependencies-v1-{runner}-{profile}-"
    dependency_tag = digest(manifest + b"\0" + requirements)
    run_id = f"{os.environ['GITHUB_RUN_ID']}-{os.environ['GITHUB_RUN_ATTEMPT']}"
    dependency_key = f"{dependency_prefix}{dependency_tag}-{run_id}"
    pip_prefix = f"pip-v1-{runner}-{sys.implementation.cache_tag}-"
    pip = ROOT / "build/cache/pip"
    emit("GITHUB_OUTPUT", {
        "submodule-key": f"submodule-git-v1-{runner}-{submodules}",
        "submodule-path": modules_path.resolve().as_posix(),
        "tools-key": f"windows-tools-v1-{runner}-{baseline}-ccache-{CCACHE_VERSION}",
        "vcpkg-root": vcpkg.as_posix(),
        "ccache-tools": ccache.as_posix(),
        "dependency-path": cache.as_posix(),
        "dependency-key": dependency_key,
        "dependency-exact-prefix": f"{dependency_prefix}{dependency_tag}-",
        "dependency-profile-prefix": dependency_prefix,
        "dependency-runner-prefix": f"dependencies-v1-{runner}-",
        "pip-path": pip.as_posix(),
        "pip-key": f"{pip_prefix}{digest(requirements)}-{run_id}",
        "pip-exact-prefix": f"{pip_prefix}{digest(requirements)}-",
        "pip-prefix": pip_prefix,
    })
    environment = {
        "CHERYL_CI_DEPENDENCY_CACHE_DIR": cache.as_posix(),
        "CHERYL_CI_DEPENDENCY_CACHE_KEY": dependency_key,
        "CHERYL_CI_VCPKG_ROOT": vcpkg.as_posix(),
        "CHERYL_CI_CCACHE_TOOLS": ccache.as_posix(),
    }
    if os.environ["RUNNER_OS"] == "Windows":
        archives = cache / "vcpkg/archives"
        downloads = cache / "vcpkg/downloads"
        for directory in (archives, downloads, pip):
            directory.mkdir(parents=True, exist_ok=True)
        environment.update({
            "VCPKG_BINARY_SOURCES": f"clear;files,{archives.as_posix()},readwrite",
            "VCPKG_DOWNLOADS": downloads.as_posix(),
            "PIP_CACHE_DIR": pip.as_posix(),
        })
    else:
        (cache / "apt").mkdir(parents=True, exist_ok=True)
    emit("GITHUB_ENV", environment)


def submodules():
    environment = os.environ.copy()
    token = environment.pop("CHERYL_SUBMODULE_TOKEN", "")
    if token:
        server = environment["GITHUB_SERVER_URL"].rstrip("/")
        credentials = base64.b64encode(f"x-access-token:{token}".encode()).decode()
        index = int(environment.get("GIT_CONFIG_COUNT", "0"))
        # Supply checkout credentials to Git without storing them in cached configs.
        for name, value in [(f"http.{server}/.extraheader", f"AUTHORIZATION: basic {credentials}"),
                            (f"url.{server}/.insteadOf", "git@github.com:")]:
            environment[f"GIT_CONFIG_KEY_{index}"] = name
            environment[f"GIT_CONFIG_VALUE_{index}"] = value
            index += 1
        environment["GIT_CONFIG_COUNT"] = str(index)
    run(["git", "submodule", "sync", "--recursive"], environment=environment)
    run(["git", "submodule", "update", "--init", "--recursive", "--checkout", "--depth=1"], environment=environment)


def windows_tools():
    baseline = json.loads((ROOT / ".github/ci/vcpkg.json").read_text(encoding="utf-8"))["builtin-baseline"]
    vcpkg = Path(os.environ["CHERYL_CI_VCPKG_ROOT"])
    if not (vcpkg / ".git").exists():
        run(["git", "clone", "--filter=blob:none", "--no-checkout",
             "https://github.com/microsoft/vcpkg.git", str(vcpkg)])
    run(["git", "-C", str(vcpkg), "checkout", "--detach", baseline])
    if not (vcpkg / "vcpkg.exe").is_file():
        run(["cmd.exe", "/d", "/c", "bootstrap-vcpkg.bat", "-disableMetrics"], cwd=vcpkg)
    run([str(vcpkg / "vcpkg.exe"), "version"])

    tools = Path(os.environ["CHERYL_CI_CCACHE_TOOLS"])
    executable = tools / "ccache.exe"
    if not executable.is_file():
        run(["choco", "install", "ccache", f"--version={CCACHE_VERSION}", "--yes", "--no-progress"])
        installed = Path(os.environ["ChocolateyInstall"]) / "lib/ccache/tools"
        candidates = sorted(installed.rglob("ccache.exe"))
        if not candidates:
            raise RuntimeError("ccache executable is unavailable")
        shutil.copytree(candidates[0].parent, tools, dirs_exist_ok=True)
    version = run([str(executable), "--version"], capture=True).splitlines()[0]
    if version != f"ccache version {CCACHE_VERSION}":
        raise RuntimeError(f"Unexpected ccache version: {version}")
    print(version, flush=True)
    with Path(os.environ["GITHUB_PATH"]).open("a", encoding="utf-8") as output:
        output.write(f"{tools}\n")


def apt_command(profile):
    cache = Path(os.environ["CHERYL_CI_DEPENDENCY_CACHE_DIR"]) / "apt"
    return ["sudo", "apt-get", "-o", f"Dir::Cache::archives={cache}",
            "-o", "APT::Keep-Downloaded-Packages=true", "--yes", "install",
            *APT_COMMON, *APT_EXTRA[profile]]


def apt_plan(profile):
    environment = os.environ.copy()
    environment["LC_ALL"] = "C"
    command = apt_command(profile)
    command.insert(2, "--print-uris")
    plan = run(command, capture=True, environment=environment)
    uris = sorted(line for line in plan.splitlines() if line.startswith("'"))
    downloads = digest("\n".join(uris).encode())
    emit("GITHUB_OUTPUT", {
        "needed": str(bool(uris)).lower(),
        "key": f"apt-debs-v1-{os.environ['RUNNER_OS']}-{os.environ['RUNNER_ARCH']}-{downloads}",
    })


def cache_state(name, snapshot):
    cache = Path(os.environ["PIP_CACHE_DIR" if name == "pip" else "CHERYL_CI_DEPENDENCY_CACHE_DIR"])
    files = []
    for path in sorted(cache.rglob("*")):
        if path.is_file():
            stat = path.stat()
            files.append([path.relative_to(cache).as_posix(), stat.st_size, stat.st_mtime_ns])
    current = digest(json.dumps(files).encode())
    state = Path(os.environ["RUNNER_TEMP"]) / f"cheryl-{name}-cache.json"
    if snapshot:
        state.write_text(current, encoding="utf-8")
    else:
        changed = bool(files) and current != state.read_text(encoding="utf-8")
        emit("GITHUB_OUTPUT", {"changed": str(changed).lower()})
        print(f"{name} cache: {len(files)} files; changed: {changed}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("metadata", "submodules", "windows-tools", "apt-plan", "apt-install", "cache-state"))
    parser.add_argument("--profile", choices=("headless", "modules"), default="headless")
    parser.add_argument("--cache", choices=("vcpkg", "pip"), default="vcpkg")
    parser.add_argument("--snapshot", action="store_true")
    args = parser.parse_args()
    if args.operation == "metadata":
        metadata(args.profile)
    elif args.operation == "submodules":
        submodules()
    elif args.operation == "windows-tools":
        windows_tools()
    elif args.operation == "apt-plan":
        apt_plan(args.profile)
    elif args.operation == "apt-install":
        run(apt_command(args.profile))
    else:
        cache_state(args.cache, args.snapshot)


if __name__ == "__main__":
    main()
