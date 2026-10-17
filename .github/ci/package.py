"""Prepare a prebuilt development SDK from the explicit CherylSDK component."""

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


def sdk_name(metadata):
    platform_name = {"Linux": "linux", "Windows": "windows"}[metadata["system"]]
    suffix = "-static" if metadata["linkage"] == "static" else ""
    return f"cheryl-SDK-v{metadata['version']}-{platform_name}{suffix}"


def system_runtime(name, system):
    if system == "Windows":
        name = name.lower()
        if (name.startswith(("api-ms-", "ext-ms-", "vcruntime", "msvcp", "msvcr", "concrt", "ucrtbase"))
                or name in {"icu.dll", "opengl32.dll", "kernel32.dll", "kernelbase.dll", "ntdll.dll", "user32.dll",
                            "gdi32.dll", "advapi32.dll", "shell32.dll", "ole32.dll", "oleaut32.dll", "comdlg32.dll",
                            "comctl32.dll", "shlwapi.dll", "ws2_32.dll", "winmm.dll", "imm32.dll", "dwmapi.dll",
                            "setupapi.dll", "hid.dll", "cfgmgr32.dll", "bcrypt.dll", "version.dll", "normaliz.dll",
                            "dbghelp.dll", "psapi.dll"}):
            return True
        root = os.environ.get("SystemRoot")
        return bool(root and any((Path(root) / directory / name).is_file() for directory in ("System32", "SysWOW64")))
    return bool(re.match(
        r"^(?:ld-linux|ld64|ld-musl)|^lib(?:c|m|mvec|dl|pthread|rt|resolv|util|anl|BrokenLocale|nss_[A-Za-z0-9_]+|stdc\+\+|gcc_s|"
        r"GL|GLX|EGL|OpenGL|GLdispatch|GLESv[0-9]+|X[A-Za-z0-9_]*|xcb[^/]*|wayland[^/]*|xkbcommon[^/]*|decor[^/]*|"
        r"drm[^/]*|gbm|asound|pulse[^/]*|jack[^/]*|udev)\.so(?:\.|$)", name))


def cheryl_runtime(name):
    return bool(re.match(r"^(?:lib)?cheryl(?:[-.]|$)", name, re.IGNORECASE))


def embedded_runtime(name):
    return not cheryl_runtime(name) and bool(re.search(r"(?:spdlog|gainput|glad|(?:^|lib)gl46|miniaudio)", name, re.IGNORECASE))


def runtime_key(name, system):
    return name.lower() if system == "Windows" else name


def runtime_files(stage, system):
    directory, pattern = ("bin", "*.dll") if system == "Windows" else ("lib", "*.so*")
    return {runtime_key(path.name, system): path for path in sorted((stage / directory).glob(pattern)) if path.is_file()}


def check_runtime_name(name):
    if PurePosixPath(name).name != name or "\\" in name:
        raise ValueError(f"SDK imports a non-relocatable runtime path: {name}")


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


def check_symlinks(stage, *, allow_symlinks=True):
    root = stage.resolve()
    for path in stage.rglob("*"):
        if path.is_symlink():
            target = path.readlink()
            if not allow_symlinks or target.is_absolute():
                raise ValueError(f"SDK symlinks must be relative and supported by the archive: {path}")
            try:
                resolved = path.resolve(strict=True)
            except (OSError, RuntimeError) as error:
                raise ValueError(f"SDK symlink is broken or cyclic: {path}") from error
            if not resolved.is_relative_to(root):
                raise ValueError(f"SDK symlink escapes the SDK prefix: {path}")


def check_contents(stage, metadata=None):
    check_symlinks(stage)
    metadata = json.loads((stage / "SDK-MANIFEST.json").read_text(encoding="utf-8")) if metadata is None else metadata
    if (not isinstance(metadata, dict) or metadata.get("linkage") not in {"static", "shared"}
            or not isinstance(metadata.get("component_versions"), dict) or not metadata["component_versions"].get("engine")):
        raise ValueError("SDK metadata must declare linkage and the Engine component version")
    for path in stage.rglob("*"):
        if path.suffix.lower() in (".c", ".cc", ".cpp", ".cxx", ".m", ".mm", ".exe"):
            raise ValueError(f"SDK must not include application or implementation files: {path}")
    if not (stage / "include/cheryl/core.h").is_file():
        raise ValueError("The SDK is missing public Engine headers")
    artifacts = ["engine_library"]
    if metadata["linkage"] == "shared" and metadata.get("system") == "Windows":
        artifacts.append("engine_import_library")
    for field in artifacts:
        name = metadata.get(field)
        path = PurePosixPath(name) if isinstance(name, str) else None
        if not path or path.is_absolute() or ".." in path.parts or ":" in name or "\\" in name or not path.parts:
            raise ValueError("SDK metadata needs a relative compiled Engine library path")
        artifact = stage / path
        if not artifact.is_file() or not artifact.resolve().is_relative_to(stage.resolve()):
            raise ValueError(f"The SDK is missing the compiled Engine library: {name}")
    if not (stage / "lib/cmake/Cheryl/CherylConfig.cmake").is_file():
        raise ValueError("The SDK is missing CMake integration")
    if not (stage / "share/licenses/cheryl/LICENSE").is_file():
        raise ValueError("The SDK is missing the Cheryl license")
    if metadata["linkage"] == "shared":
        private_headers = ("spdlog", "GLFW", "gainput", "glad", "miniaudio.h", "freetype2", "harfbuzz", "unicode", "hwloc.h")
        for name in private_headers:
            if (stage / "include" / name).exists():
                raise ValueError(f"Shared SDK contains private dependency headers: {name}")
        for config in (stage / "lib/cmake/Cheryl").glob("CherylTargets*.cmake"):
            contents = config.read_text(encoding="utf-8")
            shared_targets = {entry[field] for entry in metadata.get("shared_runtime_dependencies", [])
                              for field in ("dependency", "sdk_target") if entry.get(field)}
            component_targets = {"engine": "Cheryl::Engine", "startup": "Cheryl::Startup", "workers": "Cheryl::Workers",
                                 "native_glfw": "Cheryl::NativeGLFW", "opengl": "Cheryl::OpenGL",
                                 "ui_tgui": "Cheryl::UI::TGUI", "ui_rmlui": "Cheryl::UI::RmlUi",
                                 "audio_miniaudio": "Cheryl::Audio::Miniaudio"}
            shared_targets.update(target for component, target in component_targets.items()
                                  if component in metadata["component_versions"])

            def runtime_hints(match):
                hints = [name for name in match[1].split(";") if name]
                if any(name not in shared_targets or name.startswith("Cheryl::Private_")
                       or (name not in component_targets.values()
                           and re.search(r"(?:spdlog|gainput|glad|(?:^|lib)gl46|miniaudio)", name, re.IGNORECASE))
                       for name in hints):
                    raise ValueError(f"Shared SDK exports undeclared dependent runtime hints: {config.name}: {hints}")
                return ""

            # CMake records private shared dependencies as optional runtime path
            # hints. They do not introduce public link or compile requirements.
            contents = re.sub(r'(?m)^\s*IMPORTED_LINK_DEPENDENT_LIBRARIES(?:_[A-Z0-9_]+)?\s+"([^"]*)"\s*$',
                              runtime_hints, contents)
            if re.search(r"(?:Cheryl::Private_|spdlog::|gainput::|gl46[;>\"]|Freetype::|harfbuzz::|ICU::|CherylSdk::Hwloc)", contents):
                raise ValueError(f"Shared SDK exports a private dependency: {config.name}")
    if (stage / "dependencies").exists():
        raise ValueError("SDK must not contain an undeclared private dependency prefix")
    for dependency in metadata.get("shared_runtime_dependencies", []):
        name = dependency.get("runtime_name") or dependency.get("artifact")
        if name and dependency.get("deployment") != "sdk" and not system_runtime(name, metadata["system"]):
            raise ValueError(f"SDK leaves a non-system shared runtime external: {name}")
        if dependency.get("deployment") == "sdk":
            directory = "bin" if metadata["system"] == "Windows" else "lib"
            name = dependency.get("artifact")
            if not isinstance(name, str) or PurePosixPath(name).name != name or not name:
                raise ValueError("SDK runtime dependencies need artifact filenames")
            if not (stage / directory / name).is_file():
                raise ValueError(f"SDK is missing a declared shared runtime: {name}")


def ninja_archive_input(build, binary, archive):
    ninja = build / "build.ninja"
    if not ninja.is_file():
        return False
    contents = re.sub(r"\$\r?\n[ \t]*", "", ninja.read_text(encoding="utf-8"))
    output = re.escape(binary.relative_to(build).as_posix())
    for block in re.split(r"\n(?=build )", contents):
        header, _, variables = block.partition("\n")
        if not header.startswith("build "):
            continue
        outputs = header.split(": ", 1)[0].replace("\\", "/")
        if not re.search(r"(?:^|\s)" + output + r"(?:\s|$)", outputs):
            continue
        libraries = re.search(r"(?m)^  LINK_LIBRARIES = (.*)$", variables)
        if libraries:
            paths = libraries[1].replace("$ ", " ").replace("$:", ":").replace("\\", "/")
            return bool(re.search(r'(?:^|[/\s"])' + re.escape(archive.name) + r'(?=$|[\s"])', paths, re.IGNORECASE))
    return False


def ownership_evidence(build, metadata):
    """Check producer link evidence, keeping producer paths outside the SDK."""
    if "dependency_ownership" not in metadata:
        return []
    inventory = json.loads((build / "cheryl-dependency-ownership.json").read_text(encoding="utf-8"))
    if inventory.get("schema") != 1:
        raise ValueError("Unsupported dependency ownership inventory")
    evidence = []
    owners = {}
    maps = {path.stem: path.read_text(encoding="utf-8", errors="replace")
            for directory in (build, build / "link-maps") for path in directory.glob("*.map")}
    for entry in inventory["dependencies"]:
        archive = Path(entry["archive"])
        binary = Path(entry["binary"])
        owner = entry["target_owner"]
        if not archive.is_file() or not binary.is_file():
            raise ValueError(f"Dependency ownership evidence is missing an artifact: {entry['dependency']}")
        previous = owners.setdefault(archive.resolve(), owner)
        if previous != owner:
            raise ValueError(f"A private archive has multiple binary owners: {archive.name}")
        item = {"dependency": entry["dependency"], "owner": owner, "archive": archive.name,
                "owner_artifact": binary.name, "evidence": "static archive closure"}
        if metadata["linkage"] == "shared":
            # GNU link maps show archive(member); MSVC maps show archive:member.
            windows = metadata["system"] == "Windows"
            pattern = (r"(?i)(?:^|[\s/\\])" + re.escape(archive.stem) + r"(?:\.lib)?:" if windows
                       else re.escape(archive.name) + r"\(")
            owner_map = maps.get(owner, "")
            extracted = bool(re.search(pattern, owner_map))
            if not extracted and archive.name not in owner_map:
                if not windows or not ninja_archive_input(build, binary, archive):
                    raise ValueError(f"Owner link map has no dependency archive input: {owner}/{archive.name}")
            for other, contents in maps.items():
                if other != owner and re.search(pattern, contents):
                    raise ValueError(f"Private dependency members are linked into two owners: {archive.name}/{other}")
            item["evidence"] = ("owner link map archive members" if extracted else
                                "owner Ninja link archive input; no members required" if windows else
                                "owner link map archive input; no members required")
        evidence.append(item)
    expected = {(entry["dependency"], entry["owner"]) for entry in metadata["dependency_ownership"]}
    actual = {(entry["dependency"], entry["owner"]) for entry in evidence}
    if expected != actual:
        raise ValueError("SDK ownership metadata does not match the producer inventory")
    return evidence


def shared_runtime_evidence(build, metadata):
    """Resolve exact runtime names from the declared producer shared libraries."""
    if "shared_runtime_dependencies" not in metadata:
        return []
    inventory = json.loads((build / "cheryl-shared-dependencies.json").read_text(encoding="utf-8"))
    if inventory.get("schema") != 1:
        raise ValueError("Unsupported shared dependency inventory")
    declarations = {(entry["dependency"], entry["consumer"]): entry for entry in inventory["dependencies"]}
    expected = {(entry["dependency"], entry["consumer"]) for entry in metadata["shared_runtime_dependencies"]}
    if expected != set(declarations):
        raise ValueError("SDK shared metadata does not match the producer inventory")
    evidence = []
    for entry in metadata["shared_runtime_dependencies"]:
        declaration = declarations[entry["dependency"], entry["consumer"]]
        item = dict(entry)
        library = Path(declaration["library"]) if declaration["library"] else None
        if library:
            if not library.is_file() or library.name != entry["artifact"]:
                raise ValueError(f"Shared dependency evidence is missing an artifact: {entry['dependency']}")
            if metadata["system"] == "Windows":
                if declaration["system"]:
                    item["runtime_name"] = library.stem + ".dll" if library.suffix.lower() == ".lib" else library.name
                elif library.suffix.lower() != ".dll":
                    raise ValueError(f"Windows shared dependency needs a DLL location: {entry['dependency']}")
                else:
                    item["runtime_name"] = library.name
            else:
                output = subprocess.check_output(["readelf", "-d", str(library)], text=True)
                soname = re.search(r"\(SONAME\).*?\[([^]]+)\]", output)
                item["runtime_name"] = soname[1] if soname else library.name
            item["runtime_version"] = library.resolve().name
        elif declaration["system"]:
            item["runtime_name"] = "icu.dll" if metadata["system"] == "Windows" and "icu" in entry["dependency"].lower() else ""
        else:
            raise ValueError("A non-system shared dependency has no runtime artifact")
        evidence.append(item)
    return evidence


def runtime_imports(library, system):
    if system == "Windows":
        tool = shutil.which("dumpbin")
        if tool:
            command = [tool, "/nologo", "/dependents", str(library)]
        else:
            tool = shutil.which("llvm-readobj")
            if not tool:
                raise RuntimeError("SDK runtime import verification requires dumpbin or llvm-readobj")
            command = [tool, "--coff-imports", str(library)]
        output = subprocess.check_output(command, text=True)
        return re.findall(r"(?im)^\s*(?:Name:\s*)?([^\s]+\.dll)\s*$", output)
    output = subprocess.check_output(["readelf", "-d", str(library)], text=True)
    return re.findall(r"\(NEEDED\).*?\[([^]]+)\]", output)


def shared_runtime_closure(stage, metadata):
    """Record bundled native runtimes reached through the installed binaries."""
    system = metadata["system"]
    records = metadata.setdefault("shared_runtime_dependencies", [])
    declared = {runtime_key(entry.get("runtime_name", entry.get("artifact", "")), system) for entry in records}
    binaries = runtime_files(stage, system)
    pending = []
    for entry in records:
        if entry.get("deployment") != "sdk":
            continue
        name = entry.get("runtime_name", entry["artifact"])
        library = binaries.get(runtime_key(name, system))
        if not library:
            raise ValueError(f"SDK is missing a declared shared runtime: {name}")
        pending.append((entry["dependency"], library))
    pending.extend(("component::" + library.name, library) for library in binaries.values() if cheryl_runtime(library.name))
    inspected = set()
    for parent, library in pending:
        if library.resolve() in inspected:
            continue
        inspected.add(library.resolve())
        for dependency in runtime_imports(library, system):
            check_runtime_name(dependency)
            if system_runtime(dependency, system):
                continue
            key = runtime_key(dependency, system)
            child = binaries.get(key)
            if not child:
                raise ValueError(f"SDK is missing a required shared runtime: {library.name}: {dependency}")
            pending.append(("runtime::" + dependency, child))
            if key in declared or cheryl_runtime(dependency):
                continue
            item = {"dependency": "runtime::" + dependency, "consumer": parent,
                    "consumer_artifact": library.name, "artifact": child.name, "runtime_name": child.name,
                    "runtime_version": child.resolve().name, "deployment": "sdk", "evidence": "runtime import closure"}
            records.append(item)
            declared.add(key)


def check_runtime_imports(stage, metadata):
    system = metadata["system"]
    binaries = runtime_files(stage, system)
    declared = {runtime_key(entry.get("runtime_name", entry.get("artifact", "")), system)
                for entry in metadata.get("shared_runtime_dependencies", []) if entry.get("deployment") == "sdk"}
    recorded_files = {binaries[name].resolve() for name in declared if name in binaries}
    evidence = []
    for library in binaries.values():
        if library.is_symlink():
            continue
        if embedded_runtime(library.name):
            raise ValueError(f"SDK contains an embedded private dependency as a shared runtime: {library.name}")
        if not cheryl_runtime(library.name) and library.resolve() not in recorded_files:
            raise ValueError(f"SDK contains an undeclared shared runtime: {library.name}")
        imports = runtime_imports(library, system)
        for name in imports:
            check_runtime_name(name)
            if embedded_runtime(name):
                raise ValueError(f"SDK imports an embedded private dependency: {library.name}: {name}")
            if system_runtime(name, system):
                continue
            key = runtime_key(name, system)
            if key not in binaries:
                raise ValueError(f"SDK is missing a required shared runtime: {library.name}: {name}")
            if not cheryl_runtime(name) and key not in declared:
                raise ValueError(f"SDK imports a private dependency without a shared declaration: {library.name}: {name}")
        evidence.append({"binary": library.name, "imports": imports})
    if not evidence and metadata["linkage"] == "shared":
        raise ValueError("Shared SDK contains no binaries to inspect")
    return evidence


def archive_sdk(stage, destination):
    check_symlinks(stage, allow_symlinks=destination.suffix != ".zip")
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
            with tarfile.open(fileobj=compressed, mode="w", dereference=False) as archive:
                for path in [stage, *sorted(stage.rglob("*"))]:
                    info = archive.gettarinfo(str(path), arcname=path.relative_to(stage.parent).as_posix())
                    info.uid = info.gid = info.mtime = 0
                    info.uname = info.gname = ""
                    if info.isfile():
                        with path.open("rb") as contents:
                            archive.addfile(info, contents)
                    else:
                        archive.addfile(info)


def bundle(source, build, output):
    metadata = json.loads((build / "cheryl-sdk.json").read_text(encoding="utf-8"))
    metadata.update(source_identity(source))
    metadata["dependency_ownership"] = ownership_evidence(build, metadata)
    if "shared_runtime_dependencies" in metadata:
        metadata["shared_runtime_dependencies"] = shared_runtime_evidence(build, metadata)
    if metadata["system"] == "Linux":
        try:
            release = platform.freedesktop_os_release()
        except OSError:
            release = {}
        metadata["distribution"] = {key: release[key] for key in ("ID", "VERSION_ID") if key in release}
    name = sdk_name(metadata)
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        stage = Path(temporary) / name
        subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage), "--component", "CherylSDK"], check=True)
        shared_runtime_closure(stage, metadata)
        if metadata["system"] == "Windows":
            manifest = json.loads((source / ".github/ci/vcpkg.json").read_text(encoding="utf-8"))
            metadata["vcpkg_baseline"] = manifest["builtin-baseline"]
            metadata["system_dependencies"] = ["Windows 10 1903+ system APIs including ICU and OpenGL", "MSVC DLL runtime"]
        else:
            metadata["system_dependencies"] = ["Linux loader, glibc and NSS", "C++ compiler runtime",
                                               "OpenGL/GL driver, X11/xcb/Wayland/xkbcommon/libdecor (when selected)",
                                               "ALSA/PulseAudio/JACK service clients (when selected)", "libudev (when selected)"]
        check_contents(stage, metadata)
        metadata["runtime_import_evidence"] = check_runtime_imports(stage, metadata)
        runtime_directories = []
        if metadata["system"] == "Windows":
            runtime_directories = ["bin"]
        elif any((stage / "lib").glob("*.so*")):
            runtime_directories = ["lib"]
        metadata["runtime_directories"] = [name for name in runtime_directories if (stage / name).is_dir()]
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
    args = parser.parse_args()
    bundle(args.source.resolve(), args.build_dir.resolve(), args.output.resolve())


if __name__ == "__main__":
    main()
