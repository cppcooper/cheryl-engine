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


def dependency_scope(*, environment=None):
    environment = os.environ if environment is None else environment
    ref = environment.get("GITHUB_REF")
    event = environment.get("GITHUB_EVENT_NAME")
    branch = environment.get("GITHUB_BASE_REF") if event == "pull_request" else {
        "refs/heads/main": "main", "refs/heads/develop": "develop"
    }.get(ref)
    writer = (environment.get("GITHUB_REPOSITORY") == "cppcooper/cheryl-engine"
              and ref in {"refs/heads/main", "refs/heads/develop"}
              and event in {"push", "workflow_dispatch"})
    return {"dependency-role": "develop" if branch == "develop" else "main",
            "dependency-cache-writer": str(writer).lower()}


def environment_configuration():
    with (ROOT / ".github/ci/environment.json").open(encoding="utf-8") as source:
        return json.load(source)


def environment_metadata(profile, *, environment=None, configuration=None):
    """Describe immutable environment caches without inspecting Git or source revisions."""
    environment = os.environ if environment is None else environment
    configuration = environment_configuration() if configuration is None else configuration
    if profile not in {"headless", "modules", "docs"}:
        raise ValueError("Unknown environment cache profile")
    revision = configuration.get("cache_version") if isinstance(configuration, dict) else None
    if type(revision) is not int or revision <= 0:
        raise ValueError("The environment cache version must be a positive integer")
    runner = [environment.get(name) for name in ("RUNNER_OS", "RUNNER_ARCH")]
    if any(not isinstance(value, str) or not value or any(
            not (character.isascii() and (character.isalnum() or character in "_.-"))
            for character in value) for value in runner):
        raise ValueError("Environment cache keys require a runner OS and architecture")
    image = {name: environment.get(name, "") for name in ("ImageOS", "ImageVersion")}
    if any(not isinstance(value, str) for value in image.values()):
        raise ValueError("Runner image identities must be strings")
    identity = digest(json.dumps({"configuration": configuration, "image": image},
                                 sort_keys=True, separators=(",", ":")).encode())
    writer = (environment.get("GITHUB_REPOSITORY") == "cppcooper/cheryl-engine"
              and environment.get("GITHUB_REF") == "refs/heads/main"
              and environment.get("GITHUB_EVENT_NAME") in {"push", "workflow_dispatch"})
    return {"environment-prefix": f"actions-env-cache-v{revision}-{'-'.join(runner)}-{profile}-{identity}",
            "environment-cache-writer": str(writer).lower()}


def environment_keys(profile, baseline, requirements, *, python_tag, environment=None, configuration=None):
    if not isinstance(baseline, str) or not baseline:
        raise ValueError("The vcpkg tool baseline must be nonempty")
    if not isinstance(requirements, bytes) or not isinstance(python_tag, str) or not python_tag or any(
            not (character.isascii() and (character.isalnum() or character in "_.-")) for character in python_tag):
        raise ValueError("Python cache keys require requirement bytes and an interpreter ABI tag")
    values = environment_metadata(profile, environment=environment, configuration=configuration)
    prefix = values["environment-prefix"]
    values.update({"tools-key": f"{prefix}-tools-{digest(baseline.encode())}",
                   "pip-key": f"{prefix}-pip-{python_tag}-{digest(requirements)}"})
    return values


def apt_cache_key(profile, uris, *, environment=None, configuration=None):
    if isinstance(uris, str):
        raise ValueError("APT cache keys require a sequence of resolved package URI lines")
    uris = tuple(uris)
    if any(not isinstance(value, str) for value in uris):
        raise ValueError("APT cache keys require a sequence of resolved package URI lines")
    prefix = environment_metadata(profile, environment=environment, configuration=configuration)["environment-prefix"]
    downloads = digest("\n".join(sorted(uris)).encode())
    return f"{prefix}-apt-{downloads}"


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
    if profile not in {"headless", "modules"}:
        raise ValueError("Dependency preparation requires the headless or modules profile")
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
    scope = dependency_scope()
    role = scope["dependency-role"]
    source_prefix = f"build-deps-v2-{runner}-{role}-sources-"
    installed = ROOT / "build/ci/vcpkg_installed"
    pip = ROOT / "build/cache/pip"
    emit("GITHUB_OUTPUT", {
        **environment_keys(profile, baseline, requirements, python_tag=sys.implementation.cache_tag),
        **scope,
        "submodule-key": source_prefix + submodules,
        "submodule-prefix": source_prefix,
        "submodule-main-prefix": f"build-deps-v2-{runner}-main-sources-",
        "submodule-path": modules_path.resolve().as_posix(),
        "vcpkg-root": vcpkg.as_posix(),
        "vcpkg-installed-dir": installed.as_posix(),
        "ccache-tools": ccache.as_posix(),
        "dependency-path": cache.as_posix(),
        "pip-path": pip.as_posix(),
    })
    environment = {
        "CHERYL_CI_DEPENDENCY_CACHE_DIR": cache.as_posix(),
        "CHERYL_CI_VCPKG_ROOT": vcpkg.as_posix(),
        "CHERYL_CI_VCPKG_INSTALLED_DIR": installed.as_posix(),
        "CHERYL_CI_CCACHE_TOOLS": ccache.as_posix(),
    }
    if os.environ["RUNNER_OS"] == "Windows":
        pool = ROOT / "build/cache/vcpkg"
        archives = pool / "archives"
        downloads = pool / "downloads"
        for directory in (archives, downloads, pip):
            directory.mkdir(parents=True, exist_ok=True)
        environment.update({
            "CHERYL_CI_VCPKG_CACHE_DIR": pool.as_posix(),
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
    versions = environment_configuration()["windows"]
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
        run(["choco", "install", "ccache", f"--version={versions['ccache_package_version']}", "--yes", "--no-progress"])
        installed = Path(os.environ["ChocolateyInstall"]) / "lib/ccache/tools"
        candidates = sorted(installed.rglob("ccache.exe"))
        if not candidates:
            raise RuntimeError("ccache executable is unavailable")
        shutil.copytree(candidates[0].parent, tools, dirs_exist_ok=True)
    version = run([str(executable), "--version"], capture=True).splitlines()[0]
    if version != f"ccache version {versions['ccache_version']}":
        raise RuntimeError(f"Unexpected ccache version: {version}")
    print(version, flush=True)
    with Path(os.environ["GITHUB_PATH"]).open("a", encoding="utf-8") as output:
        output.write(f"{tools}\n")


def windows_install(profile):
    if profile not in {"headless", "modules"}:
        raise ValueError("Windows dependency installation requires the headless or modules profile")
    vcpkg = Path(os.environ["CHERYL_CI_VCPKG_ROOT"]).resolve() / "vcpkg.exe"
    command = [str(vcpkg), "install", "--triplet=x64-windows", "--host-triplet=x64-windows",
               f"--x-manifest-root={(ROOT / '.github/ci').as_posix()}",
               f"--x-install-root={(ROOT / 'build/ci/vcpkg_installed').as_posix()}",
               "--x-no-default-features", "--x-feature=hwloc"]
    if profile == "modules":
        command.append("--x-feature=native")
    run(command, cwd=ROOT)


def apt_command(profile):
    packages = environment_configuration()["linux"]
    cache = Path(os.environ["CHERYL_CI_DEPENDENCY_CACHE_DIR"]) / "apt"
    return ["sudo", "apt-get", "-o", f"Dir::Cache::archives={cache}",
            "-o", "APT::Keep-Downloaded-Packages=true", "--yes", "install",
            *packages["apt_common"], *packages["apt_extra"][profile]]


def apt_plan(profile):
    environment = os.environ.copy()
    environment["LC_ALL"] = "C"
    command = apt_command(profile)
    command.insert(2, "--print-uris")
    plan = run(command, capture=True, environment=environment)
    uris = sorted(line for line in plan.splitlines() if line.startswith("'"))
    emit("GITHUB_OUTPUT", {
        "needed": str(bool(uris)).lower(),
        "key": apt_cache_key(profile, uris),
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
    parser.add_argument("operation", choices=("metadata", "environment-prefix", "submodules", "windows-tools", "windows-install", "apt-plan", "apt-install", "cache-state"))
    parser.add_argument("--profile", choices=("headless", "modules", "docs"), default="headless")
    parser.add_argument("--cache", choices=("vcpkg", "pip"), default="vcpkg")
    parser.add_argument("--snapshot", action="store_true")
    args = parser.parse_args()
    if args.profile == "docs" and args.operation != "environment-prefix":
        parser.error("The docs profile only describes environment cache prefixes")
    if args.operation == "environment-prefix":
        emit("GITHUB_OUTPUT", environment_metadata(args.profile))
    elif args.operation == "metadata":
        metadata(args.profile)
    elif args.operation == "submodules":
        submodules()
    elif args.operation == "windows-tools":
        windows_tools()
    elif args.operation == "windows-install":
        windows_install(args.profile)
    elif args.operation == "apt-plan":
        apt_plan(args.profile)
    elif args.operation == "apt-install":
        run(apt_command(args.profile))
    else:
        cache_state(args.cache, args.snapshot)


if __name__ == "__main__":
    main()
