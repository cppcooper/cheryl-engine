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
spec = importlib.util.spec_from_file_location("ci_configure_components", Path(__file__).with_name("configure_components.py"))
configure_components = importlib.util.module_from_spec(spec)
spec.loader.exec_module(configure_components)
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
        for name in ("tests-engine.text_runs.pages", "acceptance-engine.frame.done", "acceptance-logging.process_lifetime",
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
        self.assertEqual(self.commands[1][1:], [str(self.source / "tools/input/generate_controls.py"), "--check"])
        build = next(command for command in self.commands if command[:2] == ["cmake", "--build"])
        self.assertIn("first-game", build)
        pattern = re.compile(next(command for command in self.commands if command[0] == "ctest")[-1])
        for owner in ("native-glfw", "opengl", "ui-tgui", "ui-rmlui", "audio-miniaudio", "ui-coexist"):
            self.assertIsNotNone(pattern.search(f"tests-{owner}.case"), owner)
        consumers = [Path(command[0]).name for command in self.commands if len(command) == 1]
        self.assertEqual(len(consumers), 8)
        self.assertIn("consumer-module-ui-tgui-native", build)
        self.assertIn("cheryl-ui-tgui-consumer-native", consumers)
        self.assertFalse(any(name.startswith("demo") for name in consumers))

    def test_consumer(self):
        self.invoke("--profile", "consumer")
        self.assertFalse(any(command[0] == "ctest" for command in self.commands))
        self.assertIn("-DCHERYL_BUILD_TESTS=OFF", self.commands[0])
        self.assertEqual(self.commands[1][self.commands[1].index("--target") + 1:self.commands[1].index("--parallel")],
                         ["consumer-cengine", "consumer-headers-cheryl", "consumer-module-startup"])

    def test_build_phase(self):
        self.invoke("--profile", "headless", "--phase", "build", "--hwloc", "ON")
        self.assertEqual(len(self.commands), 2)
        self.assertEqual(self.commands[0][:2], ["cmake", "-S"])
        self.assertEqual(self.commands[1][:2], ["cmake", "--build"])
        for target in ("consumer-cengine", "tests-engine", "acceptance-engine", "acceptance-logging",
                       "tests-worker-topology-composition"):
            self.assertIn(target, self.commands[1])

    def test_test_phase(self):
        self.invoke("--profile", "modules", "--phase", "test", "--hwloc", "ON", "--sdk")
        self.assertEqual(self.commands[0][0], "ctest")
        self.assertFalse(any(command[0] == "cmake" for command in self.commands))
        pattern = re.compile(self.commands[0][-1])
        for name in ("acceptance-engine.runtime_adapter.close_before_update",
                     "acceptance-logging.process_lifetime",
                     "tests-worker-topology-composition.workers.config", "tests-ui-coexist.case"):
            self.assertIsNotNone(pattern.search(name), name)
        self.assertEqual(len(self.commands), 9)
        self.assertTrue(all(len(command) == 1 for command in self.commands[1:]))

    def test_consumer_phase(self):
        self.invoke("--profile", "consumer", "--phase", "test")
        self.assertEqual(len(self.commands), 2)
        self.assertEqual(Path(self.commands[0][0]).stem, "cheryl-consumer")

    def test_shared(self):
        self.invoke("--profile", "modules", "--linkage", "shared")
        self.assertIn("-DCHERYL_BUILD_SHARED_LIBS=ON", self.commands[0])
        self.assertFalse(any(option.startswith(("-DBUILD_SHARED_LIBS=", "-DTGUI_SHARED_LIBS="))
                             for option in self.commands[0]))

    def test_sdk_preserves_linkage(self):
        self.invoke("--profile", "consumer", "--phase", "build", "--sdk")
        self.assertIn("-DCHERYL_BUILD_SHARED_LIBS=OFF", self.commands[0])
        self.assertIn("-DCHERYL_BUILD_SDK=ON", self.commands[0])
        self.assertFalse(any(option.startswith(("-DBUILD_SHARED_LIBS=", "-DTGUI_SHARED_LIBS=", "-DCHERYL_SDK_NATIVE_PREFIX="))
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
        self.assertNotIn("-DVCPKG_MANIFEST_INSTALL=OFF", self.commands[0])
        self.assertTrue(self.commands[-1][0].endswith("cheryl-startup-consumer.exe"))

    def test_windows_preinstalled(self):
        installed = self.build / "vcpkg_installed"
        with patch.object(ci.sys, "platform", "win32"):
            self.invoke("--profile", "modules", "--phase", "build", "--hwloc", "ON",
                        "--vcpkg-root", str(self.root / "vcpkg"), "--vcpkg-installed-dir", str(installed))
        self.assertIn(f"-DVCPKG_INSTALLED_DIR={installed.as_posix()}", self.commands[0])
        self.assertIn("-DVCPKG_MANIFEST_INSTALL=OFF", self.commands[0])
        self.assertIn("-DVCPKG_MANIFEST_FEATURES=hwloc;native", self.commands[0])
        self.assertEqual(len(self.commands), 3)
        self.assertEqual(self.commands[1][1:], [str(self.source / "tools/input/generate_controls.py"), "--check"])

    def test_preinstalled_requires_root(self):
        with redirect_stdout(io.StringIO()), patch("sys.stderr", new_callable=io.StringIO) as errors, \
             self.assertRaises(SystemExit) as raised:
            self.invoke("--profile", "headless", "--vcpkg-installed-dir", str(self.build / "vcpkg_installed"))
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("--vcpkg-installed-dir requires --vcpkg-root", errors.getvalue())
        self.assertEqual(self.commands, [])

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
        standalone = self.root / ".github/ci/sdk-consumer/standalone"
        standalone.mkdir(parents=True)
        (standalone / "CMakeLists.txt").write_text("project(independent)", encoding="utf-8")
        self.metadata = {"system": "Windows", "configuration": "RelWithDebInfo", "linkage": "shared"}
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
             patch.object(verify_sdk, "check_runtime_imports"), \
             patch.object(verify_sdk.subprocess, "run") as command, redirect_stdout(io.StringIO()):
            verify_sdk.main()
        return command.call_args_list

    def test_split(self):
        commands = self.invoke("build")
        self.assertEqual([call.args[0][:2] for call in commands],
                         [["cmake", "-S"], ["cmake", "-S"], ["cmake", "--build"], ["cmake", "--build"]])
        standalone = commands[1].args[0]
        self.assertIn("-DCMAKE_DISABLE_FIND_PACKAGE_ICU=TRUE", standalone)
        self.assertNotIn(f"-DCHERYL_REPOSITORY_ROOT={self.root.as_posix()}", standalone)
        self.assertTrue((self.root / "consumer-standalone-source/CMakeLists.txt").is_file())
        # Testing uses the already extracted SDK and never opens the package again.
        (self.packages / "sdk.zip").unlink()
        (self.packages / "SHA256SUMS").unlink()
        commands = self.invoke("test")
        self.assertEqual(len(commands), 2)
        self.assertEqual(commands[0].args[0][0], "ctest")
        sdk = self.build.parent / "consumer-relocated-sdk/sdk"
        search = commands[0].kwargs["env"]["PATH"].split(verify_sdk.os.pathsep)
        self.assertEqual(search[0], str(sdk / "bin"))

    def test_combined(self):
        commands = self.invoke()
        self.assertEqual([call.args[0][0] for call in commands], ["cmake", "cmake", "cmake", "cmake", "ctest", "ctest"])

    def test_missing_build(self):
        with self.assertRaises(FileNotFoundError):
            self.invoke("test")


class ConfigureMatrixTests(unittest.TestCase):
    def test_stops_and_preserves_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            arguments = ["--build-root", str(root), "--vcpkg-root", str(root / "tools"),
                         "--installed-dir", str(root / "installed"), "--case", "native", "--case", "audio"]
            failure = subprocess.CompletedProcess([], 1, stdout="configure output\n", stderr="failure detail\n")
            with patch.object(configure_components.subprocess, "run", return_value=failure) as command, \
                 redirect_stdout(io.StringIO()), self.assertRaises(subprocess.CalledProcessError):
                configure_components.main(arguments)
            command.assert_called_once()
            self.assertEqual((root / "sdk-config-native/configure.log").read_text(encoding="utf-8"),
                             "configure output\nfailure detail\n")
            self.assertFalse((root / "sdk-config-audio").exists())
            with self.assertRaisesRegex(ValueError, "preserving existing files"):
                configure_components.main(arguments)


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

    def test_shared_runtime_payload(self):
        metadata = {**self.metadata, "shared_runtime_dependencies": [
            {"dependency": "native", "consumer": "owner", "artifact": "libnative.so.2", "deployment": "sdk"}]}
        with self.assertRaisesRegex(ValueError, "missing a declared shared runtime"):
            package.check_contents(self.stage, metadata)
        (self.stage / "lib/libnative.so.2").write_bytes(b"shared native library")
        package.check_contents(self.stage, metadata)

    def test_shared_privacy(self):
        metadata = {**self.metadata, "linkage": "shared"}
        for name in ("include/spdlog/spdlog.h", "include/GLFW/glfw3.h", "include/miniaudio.h", "dependencies/lib/native.a"):
            with self.subTest(name=name):
                path = self.stage / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"private")
                with self.assertRaisesRegex(ValueError, "private dependenc"):
                    package.check_contents(self.stage, metadata)
                path.unlink()
                while path.parent != self.stage and not any(path.parent.iterdir()):
                    path = path.parent
                    path.rmdir()
        exports = self.stage / "lib/cmake/Cheryl/CherylTargets.cmake"
        exports.write_text('INTERFACE_LINK_LIBRARIES "ICU::uc"', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "exports a private dependency"):
            package.check_contents(self.stage, metadata)

    def test_shared_runtime_hints(self):
        metadata = {**self.metadata, "linkage": "shared", "component_versions": {"engine": "4.0.0", "opengl": "2.0.0"},
                    "shared_runtime_dependencies": [
            {"dependency": "harfbuzz::harfbuzz", "deployment": "system"},
            {"dependency": "glfw", "sdk_target": "Cheryl::Dependency_glfw", "deployment": "system"}]}
        exports = self.stage / "lib/cmake/Cheryl/CherylTargets-release.cmake"
        exports.write_text('  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "harfbuzz::harfbuzz;Cheryl::Dependency_glfw;Cheryl::OpenGL"\n',
                           encoding="utf-8")
        package.check_contents(self.stage, metadata)
        for target in ("ICU::uc", "spdlog::spdlog", "undeclared_runtime", "Cheryl::Workers"):
            with self.subTest(target=target):
                exports.write_text(f'  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "{target}"\n', encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "undeclared dependent runtime"):
                    package.check_contents(self.stage, metadata)
        exports.write_text('  INTERFACE_LINK_LIBRARIES "harfbuzz::harfbuzz"\n', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "exports a private dependency"):
            package.check_contents(self.stage, metadata)

    def test_owner_link_evidence(self):
        build = self.root / "producer"
        maps = build / "link-maps"
        maps.mkdir(parents=True)
        archive = build / "libprivate.a"
        binary = build / "libowner.so"
        archive.write_bytes(b"archive")
        binary.write_bytes(b"owner")
        inventory = {"schema": 1, "dependencies": [{"dependency": "private", "target_owner": "owner",
                     "archive": str(archive), "binary": str(binary)}]}
        (build / "cheryl-dependency-ownership.json").write_text(json.dumps(inventory), encoding="utf-8")
        metadata = {**self.metadata, "linkage": "shared", "dependency_ownership": [{"dependency": "private", "owner": "owner"}]}
        (maps / "owner.map").write_text(str(archive) + "(member.o)", encoding="utf-8")
        evidence = package.ownership_evidence(build, metadata)
        self.assertEqual(evidence[0]["archive"], archive.name)
        self.assertNotIn(str(build), json.dumps(evidence))
        (maps / "other.map").write_text(str(archive) + "(member.o)", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "two owners"):
            package.ownership_evidence(build, metadata)

    def test_runtime_imports(self):
        library = self.stage / "lib/libcheryl-owner.so"
        library.write_bytes(b"binary")
        dependency = self.stage / "lib/libcheryl-module-native-glfw.so.2"
        dependency.write_bytes(b"component")
        metadata = {**self.metadata, "linkage": "shared"}
        with patch.object(package, "runtime_imports", side_effect=lambda path, system:
                          [dependency.name] if path == library else []):
            evidence = package.check_runtime_imports(self.stage, metadata)
        self.assertEqual(next(entry for entry in evidence if entry["binary"] == library.name)["imports"], [dependency.name])
        for dependency in ("libfreetype.so.6", "libhidapi-hidraw.so.0"):
            with self.subTest(dependency=dependency), patch.object(package, "runtime_imports", return_value=[dependency]):
                with self.assertRaisesRegex(ValueError, "missing a required shared runtime"):
                    package.check_runtime_imports(self.stage, metadata)

    def test_declared_shared_imports(self):
        library = self.stage / "lib/libcheryl-owner.so"
        library.write_bytes(b"binary")
        (self.stage / "lib/libfreetype.so.6").write_bytes(b"native runtime")
        metadata = {**self.metadata, "linkage": "shared", "shared_runtime_dependencies": [
            {"dependency": "Freetype::Freetype", "consumer": "owner", "runtime_name": "libfreetype.so.6", "deployment": "sdk"}]}
        with patch.object(package, "runtime_imports", side_effect=lambda path, system:
                          ["libfreetype.so.6"] if path == library else []):
            self.assertEqual(package.check_runtime_imports(self.stage, metadata)[0]["imports"], ["libfreetype.so.6"])
        for name, message in (("libfreetype.so.7", "missing a required shared runtime"),
                              ("libspdlog.so.1", "embedded private dependency")):
            with self.subTest(name=name), patch.object(package, "runtime_imports", return_value=[name]):
                with self.assertRaisesRegex(ValueError, message):
                    package.check_runtime_imports(self.stage, metadata)

    def test_static_sdk_checks_shared_imports(self):
        library = self.stage / "lib/libtgui.so.1"
        library.write_bytes(b"binary")
        metadata = {**self.metadata, "shared_runtime_dependencies": [
            {"dependency": "tgui", "consumer": "adapter", "runtime_name": "libtgui.so.1", "deployment": "sdk"}]}
        with patch.object(package.subprocess, "check_output", return_value='(NEEDED) Shared library: [libspdlog.so.1]'):
            with self.assertRaisesRegex(ValueError, "embedded private dependency"):
                package.check_runtime_imports(self.stage, metadata)

    def test_native_dll_closure(self):
        directory = self.stage / "bin"
        directory.mkdir()
        for name in ("freetype.dll", "png.dll", "obsolete-icu78.dll"):
            (directory / name).write_bytes(b"fixture")
        metadata = {**self.metadata, "system": "Windows", "shared_runtime_dependencies": [
            {"dependency": "freetype", "consumer": "engine", "artifact": "freetype.dll",
             "runtime_name": "freetype.dll", "deployment": "sdk"}]}
        with patch.object(package, "runtime_imports", side_effect=lambda library, system:
                          ["png.dll", "KERNEL32.dll"] if library.name == "freetype.dll" else ["KERNEL32.dll"]):
            package.shared_runtime_closure(self.stage, metadata)
        records = metadata["shared_runtime_dependencies"]
        self.assertEqual([record["artifact"] for record in records], ["freetype.dll", "png.dll"])
        self.assertEqual(records[1]["consumer_artifact"], "freetype.dll")
        with patch.object(package, "runtime_imports", return_value=[]), \
             self.assertRaisesRegex(ValueError, "undeclared shared runtime: obsolete-icu78.dll"):
            package.check_runtime_imports(self.stage, metadata)

    def test_linux_native_closure(self):
        imports = {
            "libcheryl-engine.so.4": ["libfreetype.so.6", "libdw.so.1", "libstdc++.so.6", "libGL.so.1", "libudev.so.1"],
            "libfreetype.so.6": ["libbrotlidec.so.1", "libc.so.6"],
            "libbrotlidec.so.1": ["libbrotlicommon.so.1", "libc.so.6"],
            "libbrotlicommon.so.1": ["libc.so.6"],
            "libdw.so.1": ["libelf.so.1", "libz.so.1", "libc.so.6"],
            "libelf.so.1": ["libz.so.1", "libc.so.6"],
            "libz.so.1": ["libc.so.6"],
        }
        for name in imports:
            (self.stage / "lib" / name).write_bytes(b"runtime fixture")
        metadata = {**self.metadata, "shared_runtime_dependencies": [
            {"dependency": "freetype", "consumer": "engine", "artifact": "libfreetype.so.6",
             "runtime_name": "libfreetype.so.6", "deployment": "sdk"}]}
        with patch.object(package, "runtime_imports", side_effect=lambda path, system: imports[path.name]):
            package.shared_runtime_closure(self.stage, metadata)
            evidence = package.check_runtime_imports(self.stage, metadata)
        records = metadata["shared_runtime_dependencies"]
        self.assertEqual({entry["artifact"] for entry in records}, set(imports) - {"libcheryl-engine.so.4"})
        self.assertEqual(len(evidence), len(imports))
        self.assertEqual(next(entry for entry in records if entry["artifact"] == "libelf.so.1")["consumer_artifact"], "libdw.so.1")

    def test_missing_native_closure(self):
        library = self.stage / "lib/libcheryl-engine.so.4"
        library.write_bytes(b"Engine")
        (self.stage / "lib/libdw.so.1").write_bytes(b"diagnostics")
        metadata = {**self.metadata, "shared_runtime_dependencies": []}
        with patch.object(package, "runtime_imports", side_effect=lambda path, system:
                          ["libdw.so.1"] if path == library else ["libelf.so.1"]):
            with self.assertRaisesRegex(ValueError, "missing a required shared runtime: libdw.so.1: libelf.so.1"):
                package.shared_runtime_closure(self.stage, metadata)
        with patch.object(package, "runtime_imports", return_value=["libpng16.so.16"]):
            with self.assertRaisesRegex(ValueError, "missing a required shared runtime"):
                package.check_runtime_imports(self.stage, metadata)

    def test_runtime_baseline(self):
        for name in ("ld-linux-x86-64.so.2", "libc.so.6", "libmvec.so.1", "libstdc++.so.6", "libnss_dns.so.2", "libGL.so.1",
                     "libX11.so.6", "libxcb-xinput.so.0", "libwayland-client.so.0", "libxkbcommon.so.0",
                     "libdecor-0.so.0", "libdrm_amdgpu.so.1", "libgbm.so.1", "libasound.so.2", "libpulsecommon-17.0.so",
                     "libjack.so.0", "libudev.so.1"):
            with self.subTest(name=name):
                self.assertTrue(package.system_runtime(name, "Linux"))
        for name in ("libglib-2.0.so.0", "libgobject-2.0.so.0", "libgraphite2.so.3", "libpng16.so.16",
                     "libbrotlidec.so.1", "libdw.so.1", "libelf.so.1", "libicuuc.so.78", "libffi.so.8"):
            with self.subTest(name=name):
                self.assertFalse(package.system_runtime(name, "Linux"))
        self.assertTrue(package.system_runtime("icu.dll", "Windows"))
        self.assertTrue(package.system_runtime("KERNEL32.dll", "Windows"))
        self.assertFalse(package.system_runtime("icuuc78.dll", "Windows"))

    def test_external_native_runtime(self):
        metadata = {**self.metadata, "shared_runtime_dependencies": [
            {"dependency": "freetype", "artifact": "libfreetype.so.6", "deployment": "system"}]}
        with self.assertRaisesRegex(ValueError, "leaves a non-system shared runtime external"):
            package.check_contents(self.stage, metadata)

    def test_sdk_names(self):
        for system in ("Linux", "Windows"):
            for linkage in ("static", "shared"):
                with self.subTest(system=system, linkage=linkage):
                    expected = f"cheryl-SDK-v4.0.0-{system.lower()}" + ("-static" if linkage == "static" else "")
                    self.assertEqual(package.sdk_name({**self.metadata, "system": system, "linkage": linkage}), expected)

    def test_shared_runtime_evidence(self):
        build = self.root / "producer"
        build.mkdir()
        library = build / "libnative.so"
        library.write_bytes(b"shared")
        inventory = {"schema": 1, "dependencies": [{"dependency": "native", "consumer": "owner",
                     "library": str(library), "system": False, "owned": True}]}
        (build / "cheryl-shared-dependencies.json").write_text(json.dumps(inventory), encoding="utf-8")
        metadata = {**self.metadata, "shared_runtime_dependencies": [{"dependency": "native", "consumer": "owner",
                    "artifact": "libnative.so", "deployment": "sdk"}]}
        with patch.object(package.subprocess, "check_output", return_value='(SONAME) Library soname: [libnative.so.2]'):
            evidence = package.shared_runtime_evidence(build, metadata)
        self.assertEqual(evidence[0]["runtime_name"], "libnative.so.2")
        self.assertNotIn(str(build), json.dumps(evidence))
        metadata["shared_runtime_dependencies"][0]["consumer"] = "another"
        with self.assertRaisesRegex(ValueError, "does not match"):
            package.shared_runtime_evidence(build, metadata)

    def test_msvc_owner_map(self):
        build = self.root / "producer"
        build.mkdir()
        archive = build / "private.lib"
        binary = build / "owner.dll"
        archive.write_bytes(b"archive")
        binary.write_bytes(b"owner")
        inventory = {"schema": 1, "dependencies": [{"dependency": "private", "target_owner": "owner",
                     "archive": str(archive), "binary": str(binary)}]}
        (build / "cheryl-dependency-ownership.json").write_text(json.dumps(inventory), encoding="utf-8")
        (build / "owner.map").write_text("private:member.obj", encoding="utf-8")
        metadata = {**self.metadata, "system": "Windows", "linkage": "shared",
                    "dependency_ownership": [{"dependency": "private", "owner": "owner"}]}
        evidence = package.ownership_evidence(build, metadata)
        self.assertEqual(evidence[0]["evidence"], "owner link map archive members")
        (build / "other.map").write_text("private:member.obj", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "two owners"):
            package.ownership_evidence(build, metadata)

    def test_unused_msvc_archive(self):
        build = self.root / "producer"
        build.mkdir()
        archive, binary = build / "unused.lib", build / "owner.dll"
        archive.write_bytes(b"archive")
        binary.write_bytes(b"owner")
        inventory = {"schema": 1, "dependencies": [{"dependency": "unused", "target_owner": "owner",
                     "archive": str(archive), "binary": str(binary)}]}
        (build / "cheryl-dependency-ownership.json").write_text(json.dumps(inventory), encoding="utf-8")
        metadata = {**self.metadata, "system": "Windows", "linkage": "shared",
                    "dependency_ownership": [{"dependency": "unused", "owner": "owner"}]}
        ninja = build / "build.ninja"
        ninja.write_text('build owner.dll owner.lib: LINKER objects\n  LINK_LIBRARIES = "C:/native/unused.lib"\n', encoding="utf-8")
        evidence = package.ownership_evidence(build, metadata)
        self.assertEqual(evidence[0]["evidence"], "owner Ninja link archive input; no members required")
        ninja.write_text('build another.dll: LINKER objects\n  LINK_LIBRARIES = "C:/native/unused.lib"\n', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "no dependency archive input"):
            package.ownership_evidence(build, metadata)

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
        self.assertEqual("cheryl-SDK-v4.0.0-linux-static.tar.gz", archive.name)
        manifest = json.loads((output / "SDK-MANIFEST.json").read_text(encoding="utf-8"))
        self.assertEqual(identity["submodules"], manifest["submodules"])
        self.assertEqual(manifest["component_versions"], {"engine": "4.0.0"})
        self.assertEqual(manifest["linkage"], "static")
        with tarfile.open(archive) as contents:
            names = contents.getnames()
            self.assertFalse(any(name.endswith((".cpp", ".c", ".exe")) for name in names))


if __name__ == "__main__":
    unittest.main()
