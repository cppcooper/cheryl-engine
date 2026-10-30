"""Prepare shared native dependencies for Cheryl source and SDK builds."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


ROOT = Path(__file__).resolve().parents[2]


def run(command, *, cwd=ROOT, environment=None, capture=False):
    print("+ " + subprocess.list2cmdline(list(map(str, command))), flush=True)
    return subprocess.run(command, cwd=cwd, env=environment, check=True,
                          text=True, capture_output=capture)


def dependency_triplet(system, machine):
    architecture = {"AMD64": "x64", "x86_64": "x64", "aarch64": "arm64", "ARM64": "arm64"}.get(machine)
    if not architecture or system not in {"Linux", "Windows"}:
        raise ValueError("Dependency preparation supports Linux and Windows on x64/arm64")
    return architecture + ("-windows" if system == "Windows" else "-linux-dynamic")


def prepare(vcpkg, installed, *, hwloc=True, native=False, jobs=None):
    manifest_path = ROOT / ".github/ci/vcpkg.json"
    manifest_bytes = manifest_path.read_bytes()
    baseline = json.loads(manifest_bytes)["builtin-baseline"]
    if not vcpkg.exists():
        vcpkg.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "init", str(vcpkg)])
        run(["git", "remote", "add", "origin", "https://github.com/microsoft/vcpkg.git"], cwd=vcpkg)
        run(["git", "fetch", "--depth=1", "origin", baseline], cwd=vcpkg)
        run(["git", "checkout", "--detach", baseline], cwd=vcpkg)
    revision = run(["git", "rev-parse", "HEAD"], cwd=vcpkg, capture=True).stdout.strip()
    if revision != baseline:
        raise ValueError(f"Supply vcpkg at pinned revision {baseline}; existing checkout is unchanged")
    executable = vcpkg / ("vcpkg.exe" if os.name == "nt" else "vcpkg")
    environment = os.environ.copy()
    if jobs is not None:
        environment["VCPKG_MAX_CONCURRENCY"] = str(jobs)
    if not executable.is_file():
        command = (["cmd.exe", "/d", "/c", "bootstrap-vcpkg.bat", "-disableMetrics"] if os.name == "nt"
                   else ["bash", "bootstrap-vcpkg.sh", "-disableMetrics"])
        run(command, cwd=vcpkg, environment=environment)
    triplet = dependency_triplet(platform.system(), platform.machine())
    features = (["hwloc"] if hwloc else []) + (["native"] if native else [])
    run([str(executable), "install", f"--triplet={triplet}",
         f"--x-manifest-root={manifest_path.parent}", f"--x-install-root={installed}",
         "--x-no-default-features", *[f"--x-feature={feature}" for feature in features]], environment=environment)
    stamp = {"baseline": baseline, "manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
             "target_triplet": triplet, "features": features}
    (installed / ".cheryl-dependencies.json").write_text(json.dumps(stamp, indent=2) + "\n", encoding="utf-8")
    print(f"Shared dependency prefix: {installed / triplet}", flush=True)
    print(f"CMake toolchain: {vcpkg / 'scripts/buildsystems/vcpkg.cmake'}", flush=True)
    return stamp


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcpkg-root", type=Path, default=ROOT / "build/tools/vcpkg")
    parser.add_argument("--installed-dir", type=Path, default=ROOT / "build/dependencies")
    parser.add_argument("--without-hwloc", action="store_true")
    parser.add_argument("--native", action="store_true", help="Prepare HIDAPI for the optional native HID implementation")
    parser.add_argument("--jobs", type=int)
    args = parser.parse_args()
    if args.jobs is not None and args.jobs < 1:
        parser.error("--jobs must be positive")
    prepare(args.vcpkg_root.resolve(), args.installed_dir.resolve(), hwloc=not args.without_hwloc,
            native=args.native, jobs=args.jobs)


if __name__ == "__main__":
    main()
