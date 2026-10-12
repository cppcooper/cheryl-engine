"""Prepare a prebuilt development SDK from the explicit CherylSDK component."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


def source_identity(source):
    snapshot = source / "SOURCE-MANIFEST.json"
    if snapshot.exists():
        return json.loads(snapshot.read_text(encoding="utf-8"))
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    pins = {}

    def collect(repo, commit, prefix=""):
        entries = subprocess.check_output(["git", "-C", str(repo), "ls-tree", "-rz", commit]).split(b"\0")
        for entry in filter(None, entries):
            metadata, name = entry.split(b"\t", 1)
            mode, kind, sha = metadata.decode().split()
            if mode == "160000":
                path = name.decode()
                actual = subprocess.check_output(["git", "-C", str(repo / path), "rev-parse", "HEAD"], text=True).strip()
                if actual != sha:
                    raise ValueError(f"Submodule does not match the recorded revision: {prefix + path}")
                pins[prefix + path] = sha
                collect(repo / path, sha, prefix + path + "/")

    collect(source, revision)
    return {"revision": revision, "submodules": pins}


def copy_native_dependencies(prefix, stage):
    if not (prefix / "include").is_dir() or not (prefix / "lib").is_dir():
        raise ValueError("Expected an installed native dependency prefix")
    destination = stage / "dependencies"
    for directory in ("include", "lib", "bin", "share"):
        for path in (prefix / directory).rglob("*"):
            if not path.is_file():
                continue
            relative = path.relative_to(prefix)
            if directory == "include":
                selected = True
            elif directory == "lib":
                selected = path.suffix in (".lib", ".cmake", ".pdb")
            elif directory == "bin":
                selected = path.suffix == ".dll"
            else:
                selected = path.name == "copyright" or path.suffix == ".cmake"
            if selected and not path.name.endswith("-debug.cmake"):
                target = destination / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
                if path.suffix == ".cmake":
                    # Some vcpkg configs use toolchain variables instead of their own prefix.
                    original = target.read_text(encoding="utf-8")
                    contents = original
                    for variable in ("VCPKG_INSTALLED_DIR", "_VCPKG_INSTALLED_DIR"):
                        contents = contents.replace("${" + variable + "}/${VCPKG_TARGET_TRIPLET}",
                                                    "${cheryl_sdk_native_prefix}")
                    if contents != original:
                        parent = "/".join(".." for _ in relative.parent.parts)
                        contents = ('get_filename_component(cheryl_sdk_native_prefix '
                                    '"${CMAKE_CURRENT_LIST_DIR}/' + parent + '" ABSOLUTE)\n' + contents)
                        target.write_text(contents, encoding="utf-8")


def check_contents(stage):
    for path in stage.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"SDK files must be self-contained: {path}")
        if path.suffix.lower() in (".c", ".cc", ".cpp", ".cxx", ".m", ".mm", ".exe"):
            raise ValueError(f"SDK must not include application or implementation files: {path}")
    if not (stage / "include/cheryl/core.h").is_file():
        raise ValueError("The SDK is missing public Engine headers")
    if not any((stage / "lib").glob("*cheryl-engine.*")):
        raise ValueError("The SDK is missing the compiled Engine library")
    if not (stage / "lib/cmake/Cheryl/CherylConfig.cmake").is_file():
        raise ValueError("The SDK is missing CMake integration")
    if not (stage / "share/licenses/cheryl/LICENSE").is_file():
        raise ValueError("The SDK is missing the Cheryl license")


def archive_sdk(stage, destination):
    if destination.suffix == ".zip":
        with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    info = zipfile.ZipInfo(path.relative_to(stage.parent).as_posix(), (1980, 1, 1, 0, 0, 0))
                    info.create_system = 3
                    info.external_attr = path.stat().st_mode << 16
                    info.compress_type = zipfile.ZIP_DEFLATED
                    archive.writestr(info, path.read_bytes())
    else:
        with destination.open("wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w") as archive:
                for path in [stage, *sorted(stage.rglob("*"))]:
                    info = archive.gettarinfo(str(path), arcname=path.relative_to(stage.parent).as_posix())
                    info.uid = info.gid = info.mtime = 0
                    info.uname = info.gname = ""
                    if info.isfile():
                        with path.open("rb") as contents:
                            archive.addfile(info, contents)
                    else:
                        archive.addfile(info)


def bundle(source, build, output, native_prefix=None):
    metadata = json.loads((build / "cheryl-sdk.json").read_text(encoding="utf-8"))
    metadata.update(source_identity(source))
    architecture = {"AMD64": "x64", "x86_64": "x64"}.get(metadata["architecture"], metadata["architecture"])
    platform_name = metadata["system"].lower()
    if metadata["system"] == "Linux":
        try:
            release = platform.freedesktop_os_release()
        except OSError:
            release = {}
        metadata["distribution"] = {key: release[key] for key in ("ID", "VERSION_ID") if key in release}
        if "ID" in release:
            platform_name += "-" + release["ID"] + release.get("VERSION_ID", "")
    name = (f"cheryl-{metadata['version']}-sdk-{platform_name}-{architecture}-"
            f"{metadata['compiler'].lower()}{metadata['compiler_version']}-{metadata['configuration'].lower()}-"
            f"{metadata['revision'][:12]}")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        stage = Path(temporary) / name
        subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage), "--component", "CherylSDK"], check=True)
        if metadata["system"] == "Windows":
            copy_native_dependencies(native_prefix or build / "vcpkg_installed/x64-windows", stage)
            manifest = json.loads((source / ".github/ci/vcpkg.json").read_text(encoding="utf-8"))
            metadata["vcpkg_baseline"] = manifest["builtin-baseline"]
            metadata["runtime_directory"] = "dependencies/bin"
        else:
            metadata["system_dependencies"] = ["FreeType", "HarfBuzz", "ICU", "hwloc (when selected)",
                                               "libdw and its dependencies", "X11/OpenGL (when selected)"]
        check_contents(stage)
        text = json.dumps(metadata, indent=2, sort_keys=True) + "\n"
        (stage / "SDK-MANIFEST.json").write_text(text, encoding="utf-8")
        destination = output / (name + (".zip" if metadata["system"] == "Windows" else ".tar.gz"))
        archive_sdk(stage, destination)
    with destination.open("rb") as contents:
        digest = hashlib.file_digest(contents, "sha256").hexdigest()
    (output / "SHA256SUMS").write_text(f"{digest}  {destination.name}\n", encoding="utf-8")
    (output / "SDK-MANIFEST.json").write_text(text, encoding="utf-8")
    print(destination)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-prefix", type=Path)
    args = parser.parse_args()
    bundle(args.source.resolve(), args.build_dir.resolve(), args.output.resolve(), args.native_prefix)


if __name__ == "__main__":
    main()
