"""Extract an SDK and check independent consumers using only installed libraries."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import zipfile

from package import check_contents, check_runtime_imports, system_runtime


def check_relocated_runtimes(sdk, metadata):
    if metadata["system"] != "Linux":
        return
    libraries = {path.resolve() for path in (sdk / "lib").glob("*.so*") if path.is_file()}
    for library in sorted(libraries):
        dynamic = subprocess.run(["readelf", "-d", library], check=True, capture_output=True, text=True).stdout
        paths = re.findall(r"\((?:RUNPATH|RPATH)\).*?\[(.*?)\]", dynamic)
        if paths != ["$ORIGIN"]:
            raise ValueError(f"SDK library requires a relative runtime search path: {library.name}: {paths}")
        loaded = subprocess.run(["ldd", library], check=True, capture_output=True, text=True).stdout
        for line in loaded.splitlines():
            match = re.match(r"\s*(\S+)\s+=>\s+(.+)", line)
            if not match:
                continue
            name, location = match.groups()
            location = re.sub(r"\s+\(0x[0-9a-fA-F]+\)\s*$", "", location).strip()
            if location == "not found":
                raise ValueError(f"Relocated SDK cannot load {name} required by {library.name}")
            if not system_runtime(name, "Linux") and not Path(location).resolve().is_relative_to(sdk.resolve()):
                raise ValueError(f"Relocated SDK loads {name} outside its package: {location}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--producer-build", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--phase", choices=("all", "build", "test"), default="all")
    args = parser.parse_args()
    source = args.source.resolve()
    packages = args.packages.resolve()
    build = args.build_dir.resolve()
    hermetic_build = build.parent / (build.name + "-hermetic")
    hermetic_source = build.parent / (build.name + "-standalone-source")
    extracted = build.parent / (build.name + "-relocated-sdk")
    if args.phase != "test":
        digest, name = (packages / "SHA256SUMS").read_text(encoding="utf-8").strip().split("  ")
        archive_path = packages / name
        if archive_path.parent != packages:
            raise ValueError("Invalid archive filename")
        with archive_path.open("rb") as contents:
            if hashlib.file_digest(contents, "sha256").hexdigest() != digest:
                raise ValueError("SDK checksum mismatch")
        extracted.mkdir(parents=True)
        if archive_path.suffix == ".zip":
            with zipfile.ZipFile(archive_path) as archive:
                for name in archive.namelist():
                    if not (extracted / name).resolve().is_relative_to(extracted.resolve()):
                        raise ValueError("Invalid SDK archive path")
                archive.extractall(extracted)
        else:
            with tarfile.open(archive_path) as archive:
                archive.extractall(extracted, filter="data")
    roots = list(extracted.iterdir())
    if len(roots) != 1:
        raise ValueError("Expected one SDK root")
    sdk = roots[0]
    metadata = json.loads((sdk / "SDK-MANIFEST.json").read_text(encoding="utf-8"))
    if args.phase != "test":
        check_contents(sdk, metadata)
        check_runtime_imports(sdk, metadata)
        check_relocated_runtimes(sdk, metadata)
        # Source and producer build paths must never be required by the import files.
        for config in sdk.rglob("*.cmake"):
            contents = config.read_text(encoding="utf-8")
            if any(path.resolve().as_posix() in contents for path in (source, args.producer_build)):
                raise ValueError(f"SDK leaks a producer path: {config}")
    environment = os.environ.copy()
    if metadata["system"] == "Windows":
        environment["PATH"] = os.pathsep.join((str(sdk / "bin"), environment.get("PATH", "")))
    commands = []
    if args.phase != "test":
        configure = ["cmake", "-S", source / ".github/ci/sdk-consumer", "-B", build, "-G", "Ninja",
                     f"-DCMAKE_PREFIX_PATH={sdk}", f"-DCHERYL_REPOSITORY_ROOT={source.as_posix()}",
                     f"-DCMAKE_BUILD_TYPE={metadata['configuration']}"]
        if metadata["system"] == "Windows" and environment.get("CMAKE_CXX_COMPILER_LAUNCHER") == "ccache":
            configure += ["-DCMAKE_POLICY_DEFAULT_CMP0141=NEW",
                          "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"]
        # Copy a complete tiny project outside the repository. It has no source
        # header paths, test fixtures, toolchain file, or producer target access.
        shutil.copytree(source / ".github/ci/sdk-consumer/standalone", hermetic_source)
        hermetic_configure = ["cmake", "-S", hermetic_source, "-B", hermetic_build, "-G", "Ninja",
                              f"-DCMAKE_PREFIX_PATH={sdk}", f"-DCMAKE_BUILD_TYPE={metadata['configuration']}",
                              "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]
        # Native implementations come from the SDK for either Engine linkage.
        for dependency in ("Freetype", "harfbuzz", "ICU", "hwloc", "hidapi"):
            hermetic_configure.append(f"-DCMAKE_DISABLE_FIND_PACKAGE_{dependency}=TRUE")
        if metadata.get("linkage") == "shared":
            for dependency in ("PkgConfig", "OpenGL", "X11"):
                hermetic_configure.append(f"-DCMAKE_DISABLE_FIND_PACKAGE_{dependency}=TRUE")
        if metadata["system"] == "Windows":
            hermetic_configure.append("-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        commands.extend([configure, hermetic_configure,
                         ["cmake", "--build", build, "--parallel", str(args.jobs)],
                         ["cmake", "--build", hermetic_build, "--parallel", str(args.jobs)]])
    if args.phase != "build":
        commands.append(["ctest", "--test-dir", build, "--parallel", str(args.jobs), "--no-tests=error", "--timeout", "120",
                         "--output-on-failure", "--output-junit", build / "junit.xml"])
        commands.append(["ctest", "--test-dir", hermetic_build, "--parallel", str(args.jobs), "--no-tests=error", "--timeout", "120",
                         "--output-on-failure", "--output-junit", hermetic_build / "junit.xml"])
    for command in commands:
        print("+ " + subprocess.list2cmdline(list(map(str, command))), flush=True)
        subprocess.run(command, env=environment, check=True)


if __name__ == "__main__":
    main()
