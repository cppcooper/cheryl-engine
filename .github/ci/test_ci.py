"""Check CI command selection and failure propagation without compiling Cheryl."""

from contextlib import redirect_stdout
import importlib.util
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import tempfile
import tarfile
import unittest
from unittest.mock import patch
import zipfile


spec = importlib.util.spec_from_file_location("ci_run", Path(__file__).with_name("run.py"))
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)
spec = importlib.util.spec_from_file_location("ci_package", Path(__file__).with_name("package.py"))
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)
spec = importlib.util.spec_from_file_location("ci_verify_sdk", Path(__file__).with_name("verify_sdk.py"))
verify_sdk = importlib.util.module_from_spec(spec)
with patch.dict("sys.modules", {"package": package}):
    spec.loader.exec_module(verify_sdk)


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        # Match run.py's canonical paths, including Windows 8.3 aliases.
        self.root = Path(self.temporary.name).resolve()
        self.source = self.root / "source with spaces"
        self.source.mkdir()
        self.build = self.root / "build"
        self.commands = []
        self.environments = []

    def execute(self, command, source, environment):
        command = list(map(str, command))
        self.commands.append(command)
        self.environments.append(dict(environment))
        self.assertEqual(source, self.source)
        if command[0] == "ctest":
            self.assertEqual(environment["CHERYL_STB_ALLOCATION_TTF"], str(self.source / "assets/fonts/DejaVuSans.ttf"))
            self.build.mkdir()
            (self.build / "junit.xml").write_text(
                '<testsuite><testcase name="passed"/><testcase name="fixture"><skipped/></testcase></testsuite>',
                encoding="utf-8")

    def invoke(self, *arguments):
        output = io.StringIO()
        with patch.object(ci, "run", self.execute), patch.dict(ci.os.environ, {}, clear=True), redirect_stdout(output):
            ci.main(["--source", str(self.source), "--build-dir", str(self.build), "--jobs", "2", *arguments])
        return output.getvalue()

    def test_headless(self):
        output = self.invoke("--profile", "headless", "--hwloc", "ON")
        command = next(command for command in self.commands if command[0] == "ctest")
        pattern = re.compile(command[command.index("-R") + 1])
        for name in ("tests-engine.text_runs.pages", "acceptance-engine.frame.done",
                     "tests-worker-topology-composition.workers.config", "consumer-worker-topology"):
            self.assertIsNotNone(pattern.search(name), name)
        for name in ("all-engine.text_runs.pages", "acceptance-worker-topology-hwloc.native.binding",
                     "acceptance-opengl.native.frame", "tests-engineXtext_runs.pages"):
            self.assertIsNone(pattern.search(name), name)
        self.assertIn("Completed 1 checks; 1 skipped.", output)
        self.assertIn("Not accepted (skipped): fixture", output)
        self.assertIn("-DCHERYL_BUILD_SHARED_LIBS=OFF", self.commands[0])
        self.assertFalse(any(command[0].endswith("demo") for command in self.commands))

    def test_modules(self):
        self.invoke("--profile", "modules")
        self.assertIn("-DCHERYL_BUILD_FIRST_GAME=ON", self.commands[0])
        build = next(command for command in self.commands if command[:2] == ["cmake", "--build"])
        self.assertIn("first-game", build)
        pattern = re.compile(next(command for command in self.commands if command[0] == "ctest")[-1])
        for owner in ("native-glfw", "opengl", "ui-tgui", "ui-rmlui", "audio-miniaudio", "ui-coexist"):
            self.assertIsNotNone(pattern.search(f"tests-{owner}.case"), owner)
        consumers = [Path(command[0]).name for command in self.commands if len(command) == 1]
        self.assertEqual(len(consumers), 6)
        self.assertFalse(any(name.startswith("demo") for name in consumers))

    def test_consumer(self):
        self.invoke("--profile", "consumer")
        self.assertFalse(any(command[0] == "ctest" for command in self.commands))
        self.assertIn("-DCHERYL_BUILD_TESTS=OFF", self.commands[0])
        self.assertEqual(self.commands[1][self.commands[1].index("--target") + 1:self.commands[1].index("--parallel")],
                         ["consumer-cengine", "consumer-headers-cheryl"])

    def test_build_phase(self):
        self.invoke("--profile", "headless", "--phase", "build", "--hwloc", "ON")
        self.assertEqual(len(self.commands), 2)
        self.assertEqual(self.commands[0][:2], ["cmake", "-S"])
        self.assertEqual(self.commands[1][:2], ["cmake", "--build"])
        for target in ("consumer-cengine", "tests-engine", "acceptance-engine", "tests-worker-topology-composition"):
            self.assertIn(target, self.commands[1])

    def test_test_phase(self):
        self.invoke("--profile", "modules", "--phase", "test", "--hwloc", "ON", "--sdk")
        self.assertEqual(self.commands[0][0], "ctest")
        self.assertFalse(any(command[0] == "cmake" for command in self.commands))
        pattern = re.compile(self.commands[0][-1])
        for name in ("acceptance-engine.runtime_adapter.close_before_update",
                     "tests-worker-topology-composition.workers.config", "tests-ui-coexist.case"):
            self.assertIsNotNone(pattern.search(name), name)
        self.assertEqual(len(self.commands), 7)
        self.assertTrue(all(len(command) == 1 for command in self.commands[1:]))

    def test_consumer_phase(self):
        self.invoke("--profile", "consumer", "--phase", "test")
        self.assertEqual(len(self.commands), 1)
        self.assertEqual(Path(self.commands[0][0]).stem, "cheryl-consumer")

    def test_shared(self):
        self.invoke("--profile", "modules", "--linkage", "shared")
        self.assertIn("-DCHERYL_BUILD_SHARED_LIBS=ON", self.commands[0])
        self.assertFalse(any(option.startswith(("-DBUILD_SHARED_LIBS=", "-DTGUI_SHARED_LIBS="))
                             for option in self.commands[0]))

    def test_windows_shared(self):
        with patch.object(ci.sys, "platform", "win32"):
            self.invoke("--profile", "consumer", "--phase", "test", "--linkage", "shared")
        self.assertIn(str(self.build), self.environments[-1]["PATH"].split(ci.os.pathsep))

    def test_windows(self):
        with patch.object(ci.sys, "platform", "win32"):
            self.invoke("--profile", "headless", "--build-type", "RelWithDebInfo", "--hwloc", "ON",
                        "--vcpkg-root", str(self.root / "vcpkg"))
        self.assertIn("-DVCPKG_MANIFEST_FEATURES=hwloc", self.commands[0])
        self.assertIn("-DVCPKG_TARGET_TRIPLET=x64-windows", self.commands[0])
        self.assertTrue(self.commands[-1][0].endswith("cheryl-consumer.exe"))

    def test_build_failure(self):
        calls = []

        def fail_build(command, source, environment):
            calls.append(command)
            if "--build" in command:
                raise subprocess.CalledProcessError(1, command)

        with patch.object(ci, "run", fail_build), self.assertRaises(subprocess.CalledProcessError):
            ci.main(["--source", str(self.source), "--build-dir", str(self.build), "--profile", "headless"])
        self.assertEqual(len(calls), 2)


class SdkPhaseTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.build = self.root / "consumer"
        self.packages = self.root / "packages"
        self.packages.mkdir()
        self.metadata = {"system": "Windows", "configuration": "RelWithDebInfo"}
        archive = self.packages / "sdk.zip"
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr("sdk/SDK-MANIFEST.json", json.dumps(self.metadata))
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        (self.packages / "SHA256SUMS").write_text(f"{digest}  {archive.name}", encoding="utf-8")

    def invoke(self, phase=None):
        arguments = ["verify_sdk.py", "--source", str(self.root), "--packages", str(self.packages),
                     "--build-dir", str(self.build), "--producer-build", str(self.root / "producer"), "--jobs", "2"]
        if phase:
            arguments += ["--phase", phase]
        with patch("sys.argv", arguments), patch.object(verify_sdk, "check_contents"), \
             patch.object(verify_sdk.subprocess, "run") as command, redirect_stdout(io.StringIO()):
            verify_sdk.main()
        return command.call_args_list

    def test_split(self):
        commands = self.invoke("build")
        self.assertEqual([call.args[0][:2] for call in commands], [["cmake", "-S"], ["cmake", "--build"]])
        # Testing uses the already extracted SDK and never opens the package again.
        (self.packages / "sdk.zip").unlink()
        (self.packages / "SHA256SUMS").unlink()
        commands = self.invoke("test")
        self.assertEqual(len(commands), 1)
        self.assertEqual(commands[0].args[0][0], "ctest")
        sdk = self.build.parent / "consumer-relocated-sdk/sdk"
        search = commands[0].kwargs["env"]["PATH"].split(verify_sdk.os.pathsep)
        self.assertEqual(search[:2], [str(sdk / "bin"), str(sdk / "dependencies/bin")])

    def test_combined(self):
        commands = self.invoke()
        self.assertEqual([call.args[0][0] for call in commands], ["cmake", "cmake", "ctest"])

    def test_missing_build(self):
        with self.assertRaises(FileNotFoundError):
            self.invoke("test")


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.stage = self.root / "sdk"
        self.metadata = {"version": "4.0.0", "component_versions": {"engine": "4.0.0"}, "linkage": "static",
                         "engine_library": "lib/libcheryl-engine-4.0.0.a", "engine_import_library": "", "system": "Linux"}
        for name in ("include/cheryl/core.h", self.metadata["engine_library"], "lib/cmake/Cheryl/CherylConfig.cmake",
                     "share/licenses/cheryl/LICENSE", "share/licenses/stb/LICENSE"):
            path = self.stage / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode())
        (self.stage / "SDK-MANIFEST.json").write_text(json.dumps(self.metadata), encoding="utf-8")

    def test_archive(self):
        package.check_contents(self.stage)
        expected = {path.relative_to(self.stage.parent).as_posix(): path.read_bytes()
                    for path in self.stage.rglob("*") if path.is_file()}
        for suffix in (".tar.gz", ".zip"):
            archive = self.root / ("sdk" + suffix)
            package.archive_sdk(self.stage, archive)
            first = archive.read_bytes()
            package.archive_sdk(self.stage, archive)
            self.assertEqual(first, archive.read_bytes())
            if suffix == ".zip":
                with zipfile.ZipFile(archive) as contents:
                    actual = {name: contents.read(name) for name in contents.namelist()}
            else:
                with tarfile.open(archive) as contents:
                    actual = {item.name: contents.extractfile(item).read() for item in contents if item.isfile()}
            self.assertEqual(expected, actual)

    def test_contents(self):
        for name in ("demo.exe", "engine.cpp", "backend.c"):
            path = self.stage / name
            path.write_bytes(b"unexpected")
            with self.assertRaisesRegex(ValueError, "application or implementation"):
                package.check_contents(self.stage)
            path.unlink()
        (self.stage / self.metadata["engine_library"]).unlink()
        (self.stage / "lib/libcheryl-engine-other.a").write_bytes(b"unrelated library")
        with self.assertRaisesRegex(ValueError, "compiled Engine"):
            package.check_contents(self.stage)

    def test_shared_artifacts(self):
        for system, library, import_library in (
            ("Linux", "lib/libcheryl-engine.so.4.0.0", ""),
            ("Windows", "bin/cheryl-engine-4.0.0.dll", "lib/cheryl-engine-4.0.0.lib"),
        ):
            with self.subTest(system=system):
                metadata = {**self.metadata, "system": system, "linkage": "shared",
                            "engine_library": library, "engine_import_library": import_library}
                for name in (library, import_library):
                    if name:
                        path = self.stage / name
                        path.parent.mkdir(parents=True, exist_ok=True)
                        path.write_bytes(b"compiled artifact")
                package.check_contents(self.stage, metadata)
                (self.stage / (import_library or library)).unlink()
                with self.assertRaisesRegex(ValueError, "compiled Engine"):
                    package.check_contents(self.stage, metadata)

    def test_artifact_paths(self):
        for name in ("../outside.a", "/outside.a", "C:/outside.lib", r"lib\engine.lib"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "relative compiled Engine"):
                package.check_contents(self.stage, {**self.metadata, "engine_library": name})

    def make_link(self, path, target):
        try:
            path.symlink_to(target)
        except (NotImplementedError, OSError) as error:
            self.skipTest(f"Symbolic-link fixture unavailable: {error}")

    def test_shared_links(self):
        library = self.stage / "lib/libcheryl-engine.so.4.0.0"
        library.write_bytes(b"shared Engine")
        soname = library.with_name("libcheryl-engine.so.4")
        linker = library.with_name("libcheryl-engine.so")
        self.make_link(soname, library.name)
        self.make_link(linker, soname.name)
        metadata = {**self.metadata, "linkage": "shared", "engine_library": "lib/" + library.name}
        (self.stage / "SDK-MANIFEST.json").write_text(json.dumps(metadata), encoding="utf-8")
        package.check_contents(self.stage, metadata)
        destination = self.root / "shared.tar.gz"
        package.archive_sdk(self.stage, destination)
        with tarfile.open(destination) as archive:
            member = archive.getmember("sdk/lib/" + linker.name)
            self.assertTrue(member.issym())
            self.assertEqual(member.linkname, soname.name)
            archive.extractall(self.root / "relocated", filter="data")
        relocated = self.root / "relocated/sdk/lib" / linker.name
        self.assertTrue(relocated.is_symlink())
        self.assertEqual(relocated.read_bytes(), b"shared Engine")
        with self.assertRaisesRegex(ValueError, "supported by the archive"):
            package.archive_sdk(self.stage, self.root / "shared.zip")

    def test_unsafe_links(self):
        outside = self.root / "outside"
        outside.write_bytes(b"outside SDK")
        link = self.stage / "lib/unsafe"
        for target, message in ((self.stage / self.metadata["engine_library"], "relative"),
                                (Path("../../outside"), "escapes"), (Path("missing"), "broken")):
            with self.subTest(target=target):
                self.make_link(link, target)
                with self.assertRaisesRegex(ValueError, message):
                    package.check_contents(self.stage)
                link.unlink()

    def test_native_files(self):
        prefix = self.root / "native"
        kept = ("include/dependency.h", "lib/dependency.lib", "lib/cmake/dependency-release.cmake",
                "bin/dependency.dll", "share/dependency/copyright", "share/dependency/config.cmake")
        omitted = ("debug/lib/dependency.lib", "lib/cmake/dependency-debug.cmake", "tools/demo.exe", "share/dependency/source.c")
        for name in (*kept, *omitted):
            path = prefix / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode())
        package.copy_native_dependencies(prefix, self.stage)
        actual = {path.relative_to(self.stage / "dependencies").as_posix()
                  for path in (self.stage / "dependencies").rglob("*") if path.is_file()}
        self.assertEqual(set(kept), actual)

    def test_manifest(self):
        source = self.root / "source"
        build = self.root / "build"
        output = self.root / "packages"
        source.mkdir()
        build.mkdir()
        identity = {"revision": "a" * 40, "submodules": {"extern/library": "b" * 40}}
        (source / "SOURCE-MANIFEST.json").write_text(json.dumps(identity), encoding="utf-8")
        (build / "cheryl-sdk.json").write_text(json.dumps({**self.metadata,
            "architecture": "x86_64", "compiler": "GNU", "compiler_version": "14.2.0", "configuration": "Release"}),
            encoding="utf-8")

        def install(command, check):
            import shutil
            self.assertIn("CherylSDK", command)
            shutil.copytree(self.stage, command[command.index("--prefix") + 1])

        with patch.object(package.subprocess, "run", install), \
             patch.object(package.platform, "freedesktop_os_release", return_value={"ID": "ubuntu", "VERSION_ID": "24.04"}), \
             redirect_stdout(io.StringIO()):
            archive = package.bundle(source, build, output)
        self.assertIn("4.0.0-sdk-linux-ubuntu24.04-x64-gnu14.2.0-release-static-aaaaaaaaaaaa", archive.name)
        manifest = json.loads((output / "SDK-MANIFEST.json").read_text(encoding="utf-8"))
        self.assertEqual(identity["submodules"], manifest["submodules"])
        self.assertEqual(manifest["component_versions"], {"engine": "4.0.0"})
        self.assertEqual(manifest["linkage"], "static")
        with tarfile.open(archive) as contents:
            names = contents.getnames()
            self.assertFalse(any(name.endswith((".cpp", ".c", ".exe")) for name in names))


if __name__ == "__main__":
    unittest.main()
