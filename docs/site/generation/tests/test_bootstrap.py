"""Bootstrap regressions with isolated caches and no real package installation."""

from contextlib import redirect_stdout
import hashlib
import importlib
from importlib import metadata
import importlib.util
import io
import json
import os
from pathlib import Path
import queue
import re
import stat
import subprocess
import sys
import tarfile
import tempfile
import textwrap
import threading
import types
import unittest
from unittest import mock
import zipfile


BOOTSTRAP_PATH = Path(__file__).resolve().parents[1] / "bootstrap.py"
SPEC = importlib.util.spec_from_file_location("cheryl_docs_bootstrap", BOOTSTRAP_PATH)
bootstrap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bootstrap)


class TemporaryCase(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="cheryl-docs-test-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.cache = self.root / "cache"


class PythonCacheTests(TemporaryCase):
    def setUp(self):
        super().setUp()
        self.requirements = self.root / "requirements.txt"
        self.requirements.write_text(
            "\n".join(f"{name}=={version}" for name, version in bootstrap.MINIMUM_PACKAGES.items()),
            encoding="utf-8",
        )
        self.enterContext(mock.patch.object(bootstrap, "REQUIREMENTS", self.requirements))
        self.expected = bootstrap.locked_versions()
        self.available = {}
        self.probe = self.enterContext(mock.patch.object(
            bootstrap, "probe_python", side_effect=lambda python, packages, exact: self.available.get(Path(python))
        ))
        self.builder = mock.Mock()
        self.builder.create.side_effect = self.make_prefix
        self.env_builder = self.enterContext(mock.patch.object(bootstrap.venv, "EnvBuilder", return_value=self.builder))
        self.pip_installer = self.enterContext(mock.patch.object(
            bootstrap, "pip_installer", return_value=self.root / "fixture-pip.whl"
        ))
        self.installer = self.enterContext(mock.patch.object(bootstrap.subprocess, "run", side_effect=self.install))

    def make_prefix(self, prefix):
        python = prefix / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
        python.parent.mkdir(parents=True, exist_ok=True)
        python.write_bytes(b"fixture interpreter")

    def install(self, command, **kwargs):
        self.available[Path(command[0])] = self.expected.copy()
        return subprocess.CompletedProcess(command, 0)

    def seed(self):
        identity = bootstrap.environment_identity()
        prefix = self.cache / "python" / identity
        self.make_prefix(prefix)
        python = prefix / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
        self.available[python] = self.expected.copy()
        bootstrap.atomic_json(prefix / "ready.json", {"identity": identity, "versions": self.expected})
        return python, prefix / "ready.json"

    def test_status(self):
        with self.assertRaises(bootstrap.ToolError):
            bootstrap.cached_python(self.cache, install=False)
        self.env_builder.assert_not_called()
        self.pip_installer.assert_not_called()
        self.installer.assert_not_called()
        self.assertFalse(self.cache.exists())

    def test_reuse(self):
        python, marker = self.seed()
        original = marker.read_bytes()
        self.installer.side_effect = AssertionError("A ready cache must not invoke pip")
        self.assertEqual(bootstrap.cached_python(self.cache, install=True), (python, self.expected))
        self.assertEqual(bootstrap.cached_python(self.cache, install=False), (python, self.expected))
        self.env_builder.assert_not_called()
        self.pip_installer.assert_not_called()
        self.assertEqual(marker.read_bytes(), original)

    def test_install_failure(self):
        self.installer.side_effect = subprocess.CalledProcessError(1, ["fixture", "pip"])
        with self.assertRaises(subprocess.CalledProcessError):
            bootstrap.cached_python(self.cache, install=True)
        self.assertEqual(list(self.cache.rglob("ready.json")), [])
        self.installer.side_effect = self.install
        python, versions = bootstrap.cached_python(self.cache, install=True)
        self.assertEqual(versions, self.expected)
        self.assertTrue((python.parent.parent / "ready.json").is_file())
        self.assertEqual(self.installer.call_count, 2)

    def test_probe_failure(self):
        self.installer.side_effect = lambda command, **kwargs: subprocess.CompletedProcess(command, 0)
        with self.assertRaises(bootstrap.ToolError):
            bootstrap.cached_python(self.cache, install=True)
        self.assertEqual(list(self.cache.rglob("ready.json")), [])

    def test_bad_marker(self):
        python, marker = self.seed()
        for record in ("{", "[]", json.dumps({"identity": "another environment"})):
            with self.subTest(record=record):
                marker.write_text(record, encoding="utf-8")
                self.available.pop(python, None)
                with self.assertRaises(bootstrap.ToolError):
                    bootstrap.cached_python(self.cache, install=False)
                repaired, versions = bootstrap.cached_python(self.cache, install=True)
                self.assertEqual((repaired, versions), (python, self.expected))
                self.assertEqual(bootstrap.read_json(marker)["identity"], bootstrap.environment_identity())

    def test_broken_cache(self):
        python, marker = self.seed()
        self.available.pop(python)
        with self.assertRaises(bootstrap.ToolError):
            bootstrap.cached_python(self.cache, install=False)
        self.assertEqual(bootstrap.cached_python(self.cache, install=True), (python, self.expected))
        self.assertTrue(marker.exists())
        self.assertEqual(self.installer.call_count, 1)

    def test_lock_change(self):
        previous, marker = self.seed()
        original = marker.read_bytes()
        with self.requirements.open("a", encoding="utf-8") as output:
            output.write("\n# changed lock identity\n")
        current, _ = bootstrap.cached_python(self.cache, install=True)
        self.assertNotEqual(current, previous)
        self.assertEqual(marker.read_bytes(), original)
        self.assertEqual(self.installer.call_count, 1)

    def test_python_change(self):
        previous, marker = self.seed()
        original = marker.read_bytes()
        with mock.patch.object(bootstrap.sys, "executable", str(self.root / "another-python")):
            current, _ = bootstrap.cached_python(self.cache, install=True)
        self.assertNotEqual(current, previous)
        self.assertEqual(marker.read_bytes(), original)


class ProbeTests(TemporaryCase):
    """Use real dist-info discovery; stub only the external packaging parser."""

    imports = ("sphinx", "breathe", "exhale", "myst_parser", "sphinx_rtd_theme", "sphinxcontrib.mermaid",
               "tree_sitter", "tree_sitter_cpp")

    def setUp(self):
        super().setUp()
        self.site = self.root / "site"
        self.site.mkdir()
        self.requirements = {}
        for name in self.imports:
            (self.site / f"{name}.py").write_text("", encoding="utf-8")

    def distribution(self, name, version, requirements=()):
        normalized = re.sub(r"[-_.]+", "_", name).lower()
        path = self.site / f"{normalized}-fixture.dist-info"
        path.mkdir(exist_ok=True)
        record = path / "METADATA"
        record.write_text(
            f"Metadata-Version: 2.1\nName: {name}\nVersion: {version}\n"
            + "".join(f"Requires-Dist: {requirement}\n" for requirement in requirements),
            encoding="utf-8",
        )
        return record

    def requirement(self, text, name, minimum=None, active=True, extras=()):
        specifier = mock.MagicMock()
        specifier.__bool__.return_value = minimum is not None
        specifier.contains.side_effect = lambda version, **kwargs: (
            tuple(int(item) for item in version.split(".")) >= minimum
        )
        marker = None if active else types.SimpleNamespace(evaluate=lambda environment: False)
        self.requirements[text] = types.SimpleNamespace(
            name=name, specifier=specifier, marker=marker, extras=frozenset(extras)
        )

    def run_probe(self, command, **kwargs):
        requirement_module = types.ModuleType("packaging.requirements")
        requirement_module.Requirement = lambda value: self.requirements[value]
        version_module = types.ModuleType("packaging.version")
        version_module.Version = lambda value: tuple(int(part) for part in value.split("."))
        specifier_module = types.ModuleType("packaging.specifiers")
        specifier_module.SpecifierSet = lambda value: types.SimpleNamespace(
            contains=lambda version, **kwargs: value == ">=3"
        )
        packaging = types.ModuleType("packaging")
        discovered = metadata.Distribution.discover

        def distribution(name):
            found = next(iter(discovered(name=name, path=[str(self.site)])), None)
            if found is None:
                raise metadata.PackageNotFoundError(name)
            return found

        original_import = importlib.import_module

        def import_module(name, *args):
            if name in self.imports:
                if not (self.site / f"{name}.py").is_file():
                    raise ModuleNotFoundError(name)
                return types.ModuleType(name)
            return original_import(name, *args)

        output = io.StringIO()
        status = 0
        with (
            mock.patch.dict(sys.modules, {
                "packaging": packaging,
                "packaging.requirements": requirement_module,
                "packaging.specifiers": specifier_module,
                "packaging.version": version_module,
            }),
            mock.patch.object(metadata, "distribution", side_effect=distribution),
            mock.patch.object(importlib, "import_module", side_effect=import_module),
            mock.patch.object(sys, "argv", [str(command[0]), command[3]]),
            redirect_stdout(output),
        ):
            try:
                exec(command[2], {"__name__": "__main__"})
            except SystemExit as error:
                status = error.code
        return subprocess.CompletedProcess(command, status, stdout=output.getvalue(), stderr="")

    def probe(self, packages, exact=False):
        with mock.patch.object(bootstrap.subprocess, "run", side_effect=self.run_probe):
            return bootstrap.probe_python(Path(sys.executable), packages, exact)

    def test_newer_system(self):
        self.distribution("Fixture-Doc", "2.0.0")
        self.assertEqual(self.probe({"Fixture-Doc": "1.0.0"}), {"Fixture-Doc": "2.0.0"})

    def test_exact_version(self):
        self.distribution("Fixture-Doc", "2.0.0")
        self.assertIsNone(self.probe({"Fixture-Doc": "1.0.0"}, exact=True))

    def test_missing_metadata(self):
        record = self.distribution("Fixture-Doc", "1.0.0")
        record.unlink()
        record.parent.rmdir()
        self.assertIsNone(self.probe({"Fixture-Doc": "1.0.0"}))

    def test_changed_metadata(self):
        self.distribution("Fixture-Doc", "1.0.0")
        self.assertIsNotNone(self.probe({"Fixture-Doc": "1.0.0"}, exact=True))
        self.distribution("Fixture-Doc", "0.9.0")
        self.assertIsNone(self.probe({"Fixture-Doc": "1.0.0"}, exact=True))

    def test_dependency(self):
        self.requirement("Fixture-Dep>=2", "Fixture-Dep", (2, 0, 0))
        self.distribution("Fixture-Doc", "1.0.0", ["Fixture-Dep>=2"])
        self.assertIsNone(self.probe({"Fixture-Doc": "1.0.0"}))
        self.distribution("Fixture-Dep", "1.0.0")
        self.assertIsNone(self.probe({"Fixture-Doc": "1.0.0"}))
        self.distribution("Fixture-Dep", "2.0.0")
        self.assertEqual(self.probe({"Fixture-Doc": "1.0.0"}), {
            "Fixture-Doc": "1.0.0", "Fixture-Dep": "2.0.0",
        })

    def test_inactive_dependency(self):
        self.requirement("Fixture-Optional; extra == 'unused'", "Fixture-Optional", active=False)
        self.distribution("Fixture-Doc", "1.0.0", ["Fixture-Optional; extra == 'unused'"])
        self.assertEqual(self.probe({"Fixture-Doc": "1.0.0"}), {"Fixture-Doc": "1.0.0"})

    def test_dependency_extra(self):
        self.requirement("Fixture-Dep[feature]>=1", "Fixture-Dep", (1, 0, 0), extras={"feature"})
        extra = "Fixture-Extra>=2; extra == 'feature'"
        self.requirement(extra, "Fixture-Extra", (2, 0, 0))
        self.requirements[extra].marker = types.SimpleNamespace(
            evaluate=lambda environment: environment["extra"] == "feature"
        )
        self.distribution("Fixture-Doc", "1.0.0", ["Fixture-Dep[feature]>=1"])
        self.distribution("Fixture-Dep", "1.0.0", [extra])
        self.assertIsNone(self.probe({"Fixture-Doc": "1.0.0"}))
        self.distribution("Fixture-Extra", "1.0.0")
        self.assertIsNone(self.probe({"Fixture-Doc": "1.0.0"}))
        self.distribution("Fixture-Extra", "2.0.0")
        self.assertEqual(self.probe({"Fixture-Doc": "1.0.0"}), {
            "Fixture-Doc": "1.0.0", "Fixture-Dep": "1.0.0", "Fixture-Extra": "2.0.0",
        })

    def test_dependency_alias(self):
        self.requirement("fixture_doc>=1", "fixture_doc", (1, 0, 0))
        self.distribution("Fixture-Doc", "1.5.0")
        self.distribution("Fixture-Driver", "1.0.0", ["fixture_doc>=1"])
        for exact in (False, True):
            with self.subTest(exact=exact):
                self.assertIsNone(self.probe({"Fixture-Doc": "2.0.0", "Fixture-Driver": "1.0.0"}, exact))

    def test_missing_import(self):
        self.distribution("Fixture-Doc", "1.0.0")
        for name in ("breathe", "sphinxcontrib.mermaid", "tree_sitter", "tree_sitter_cpp"):
            with self.subTest(module=name):
                module = self.site / f"{name}.py"
                module.unlink()
                self.assertIsNone(self.probe({"Fixture-Doc": "1.0.0"}))
                module.write_text("", encoding="utf-8")

    def test_requires_python(self):
        self.requirement("Fixture-Dep>=1", "Fixture-Dep", (1, 0, 0))
        self.distribution("Fixture-Doc", "1.0.0", ["Fixture-Dep>=1"])
        record = self.distribution("Fixture-Dep", "1.0.0")
        original = record.read_text(encoding="utf-8")
        record.write_text(original + "Requires-Python: >=99\n", encoding="utf-8")
        self.assertIsNone(self.probe({"Fixture-Doc": "1.0.0"}))
        record.write_text(original + "Requires-Python: >=3\n", encoding="utf-8")
        self.assertIsNotNone(self.probe({"Fixture-Doc": "1.0.0"}))


class SelectionTests(TemporaryCase):
    def test_system_tools(self):
        versions = {name: "99.0.0" for name in bootstrap.MINIMUM_PACKAGES}
        native = self.root / "system-doxygen"
        with (
            mock.patch.object(bootstrap, "probe_python", return_value=versions),
            mock.patch.object(bootstrap, "select_doxygen", return_value=(native, "99.0.0")) as select_native,
            mock.patch.object(bootstrap, "cached_python") as cached,
        ):
            tools = bootstrap.select_tools(self.cache, install=True)
        self.assertEqual(tools["python_source"], "system")
        self.assertEqual(tools["python"], sys.executable)
        self.assertEqual(tools["packages"], versions)
        cached.assert_not_called()
        select_native.assert_called_once_with(self.cache, False, True)
        self.assertFalse(self.cache.exists())

    def test_force_pinned(self):
        python = self.root / "cached-python"
        with (
            mock.patch.object(bootstrap, "probe_python") as system_probe,
            mock.patch.object(bootstrap, "cached_python", return_value=(python, {"fixture": "1.0.0"})),
            mock.patch.object(bootstrap, "select_doxygen", return_value=(self.root / "doxygen", "1.0.0")),
        ):
            tools = bootstrap.select_tools(self.cache, force_pinned=True)
        system_probe.assert_not_called()
        self.assertEqual(tools["python_source"], "pinned")
        self.assertEqual(tools["python"], str(python))

    def test_explicit_tool(self):
        native = self.root / "explicit-doxygen"
        native.write_bytes(b"fixture executable")
        versions = {name: "99.0.0" for name in bootstrap.MINIMUM_PACKAGES}
        with (
            mock.patch.object(bootstrap, "doxygen_version", return_value="99.0.0"),
            mock.patch.object(bootstrap, "probe_python", return_value=versions),
            mock.patch.object(bootstrap, "select_doxygen") as fallback,
            mock.patch.object(bootstrap, "download_archive") as download,
            mock.patch.object(bootstrap, "cached_python") as cached,
        ):
            tools = bootstrap.select_tools(self.cache, install=True, doxygen=native)
        self.assertEqual(tools["doxygen"], str(native.resolve()))
        self.assertEqual(tools["doxygen_version"], "99.0.0")
        self.assertEqual(tools["python_source"], "system")
        fallback.assert_not_called()
        download.assert_not_called()
        cached.assert_not_called()
        self.assertFalse(self.cache.exists())

    def test_named_tool(self):
        native = self.root / "custom-doxygen"
        native.write_bytes(b"fixture executable")
        with (
            mock.patch.object(bootstrap.shutil, "which", return_value=str(native)) as lookup,
            mock.patch.object(bootstrap, "doxygen_version", return_value="99.0.0"),
            mock.patch.object(bootstrap, "probe_python", return_value={"fixture": "1.0.0"}),
            mock.patch.object(bootstrap, "select_doxygen") as fallback,
        ):
            tools = bootstrap.select_tools(self.cache, doxygen="chosen-doxygen")
        lookup.assert_called_once_with("chosen-doxygen")
        self.assertEqual(tools["doxygen"], str(native.resolve()))
        fallback.assert_not_called()

    def test_invalid_tool(self):
        native = self.root / "explicit-doxygen"
        native.write_bytes(b"fixture executable")
        for version in (None, "0.1.0"):
            with (
                self.subTest(version=version),
                mock.patch.object(bootstrap, "doxygen_version", return_value=version),
                mock.patch.object(bootstrap, "probe_python") as probe,
                mock.patch.object(bootstrap, "cached_python") as cached,
                mock.patch.object(bootstrap, "select_doxygen") as fallback,
            ):
                with self.assertRaises(bootstrap.ToolError):
                    bootstrap.select_tools(self.cache, install=True, doxygen=native)
                probe.assert_not_called()
                cached.assert_not_called()
                fallback.assert_not_called()
                self.assertFalse(self.cache.exists())

    def test_missing_tool(self):
        with (
            mock.patch.object(bootstrap.shutil, "which", return_value=None),
            mock.patch.object(bootstrap, "doxygen_version") as version,
            mock.patch.object(bootstrap, "probe_python") as probe,
            mock.patch.object(bootstrap, "select_doxygen") as fallback,
        ):
            with self.assertRaisesRegex(bootstrap.ToolError, "not found"):
                bootstrap.select_tools(self.cache, install=True, doxygen=str(self.root / "missing-doxygen"))
        version.assert_not_called()
        probe.assert_not_called()
        fallback.assert_not_called()
        self.assertFalse(self.cache.exists())

    def test_pinned_conflict(self):
        with (
            mock.patch.object(bootstrap, "validate_doxygen") as validation,
            mock.patch.object(bootstrap, "probe_python") as probe,
            mock.patch.object(bootstrap, "cached_python") as cached,
            mock.patch.object(bootstrap, "select_doxygen") as fallback,
        ):
            with self.assertRaisesRegex(bootstrap.ToolError, "--doxygen.*--force-pinned"):
                bootstrap.select_tools(self.cache, force_pinned=True, install=True, doxygen="explicit-doxygen")
        validation.assert_not_called()
        probe.assert_not_called()
        cached.assert_not_called()
        fallback.assert_not_called()
        self.assertFalse(self.cache.exists())

    def test_exec_selection(self):
        tools = {
            "python": str(self.root / "python"), "python_source": "system", "packages": {"fixture": "1.0.0"},
            "doxygen": str(self.root / "doxygen"), "doxygen_version": "99.0.0",
        }
        for forced in (False, True):
            with (
                self.subTest(forced=forced),
                mock.patch.object(bootstrap, "select_tools", return_value=tools) as selection,
                mock.patch.object(bootstrap.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)) as command,
            ):
                arguments = ["exec", "--cache-dir", str(self.cache)]
                arguments += ["--force-pinned"] if forced else ["--doxygen", tools["doxygen"]]
                self.assertEqual(bootstrap.main([*arguments, "--", "python", "-c", "pass"]), 0)
                selection.assert_called_once_with(
                    self.cache.resolve(), forced, install=True, doxygen=None if forced else tools["doxygen"]
                )
                self.assertEqual(command.call_args.args[0], [tools["python"], "-c", "pass"])
                environment = command.call_args.kwargs["env"]
                self.assertEqual(environment["CHERYL_DOCS_FORCE_PINNED"], "1" if forced else "0")
                self.assertEqual(environment["CHERYL_DOCS_DOXYGEN"], tools["doxygen"])


class ArchiveTests(TemporaryCase):
    def archive(self, kind, name, mode=None):
        path = self.root / f"fixture.{kind}"
        if kind == "tar":
            with tarfile.open(path, "w") as bundle:
                member = tarfile.TarInfo(name)
                if mode is None:
                    member.size = 7
                    member.mode = 0o755
                    bundle.addfile(member, io.BytesIO(b"fixture"))
                else:
                    member.type = mode
                    member.linkname = "../outside"
                    bundle.addfile(member)
        else:
            with zipfile.ZipFile(path, "w") as bundle:
                member = zipfile.ZipInfo(name)
                member.create_system = 3
                member.external_attr = ((mode or stat.S_IFREG) | 0o755) << 16
                bundle.writestr(member, b"fixture")
        return path

    def test_extract(self):
        for kind in ("tar", "zip"):
            with self.subTest(kind=kind):
                destination = self.root / f"files-{kind}"
                bootstrap.extract_archive(self.archive(kind, "bin/doxygen"), destination)
                executable = destination / "bin/doxygen"
                self.assertEqual(executable.read_bytes(), b"fixture")
                if os.name != "nt":
                    self.assertTrue(executable.stat().st_mode & stat.S_IXUSR)

    def test_paths(self):
        for kind in ("tar", "zip"):
            for name in ("../outside", "bin/../../outside", "/outside", "C:/outside", "\\\\host\\share\\outside"):
                with self.subTest(kind=kind, name=name):
                    with self.assertRaises(bootstrap.ToolError):
                        bootstrap.extract_archive(self.archive(kind, name), self.root / "files")
                    self.assertFalse((self.root / "outside").exists())

    def test_tar_special(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.CHRTYPE, tarfile.BLKTYPE, tarfile.FIFOTYPE):
            with self.subTest(kind=kind):
                with self.assertRaises(bootstrap.ToolError):
                    bootstrap.extract_archive(self.archive("tar", "special", kind), self.root / "files")
                self.assertFalse((self.root / "outside").exists())

    def test_zip_link(self):
        with self.assertRaises(bootstrap.ToolError):
            bootstrap.extract_archive(self.archive("zip", "link", stat.S_IFLNK), self.root / "files")

    @unittest.skipIf(os.name == "nt", "Fixture symlink creation needs POSIX permissions")
    def test_existing_link(self):
        destination = self.root / "files"
        destination.mkdir()
        outside = self.root / "outside"
        outside.mkdir()
        (destination / "bin").symlink_to(outside, target_is_directory=True)
        with self.assertRaises(bootstrap.ToolError):
            bootstrap.extract_archive(self.archive("tar", "bin/doxygen"), destination)
        self.assertEqual(list(outside.iterdir()), [])

    def test_verified_download(self):
        destination = self.root / "download"
        payload = b"verified archive fixture"
        with mock.patch.object(bootstrap.urllib.request, "urlopen", return_value=io.BytesIO(payload)):
            bootstrap.download_archive("https://example.invalid/fixture", destination, hashlib.sha256(payload).hexdigest())
        self.assertEqual(destination.read_bytes(), payload)
        self.assertEqual(list(self.root.glob("*.tmp")), [])

    def test_bad_hash(self):
        destination = self.root / "download"
        destination.write_bytes(b"previous verified archive")
        with mock.patch.object(bootstrap.urllib.request, "urlopen", return_value=io.BytesIO(b"corrupt")):
            with self.assertRaises(bootstrap.ToolError):
                bootstrap.download_archive("https://example.invalid/fixture", destination, "0" * 64)
        self.assertEqual(destination.read_bytes(), b"previous verified archive")
        self.assertEqual(list(self.root.glob("*.tmp")), [])


class NativeCacheTests(TemporaryCase):
    def setUp(self):
        super().setUp()
        self.payload = b"fixture doxygen"
        self.archive = self.root / "doxygen.tar"
        basename = "doxygen.exe" if os.name == "nt" else "doxygen"
        with tarfile.open(self.archive, "w") as bundle:
            member = tarfile.TarInfo(f"bin/{basename}")
            member.size = len(self.payload)
            member.mode = 0o755
            bundle.addfile(member, io.BytesIO(self.payload))
        self.digest = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        manifest = self.root / "native.json"
        bootstrap.atomic_json(manifest, {
            "version": "1.0.0", "minimum_version": "1.0.0", "platforms": {
                "fixture-host": {"url": "https://example.invalid/doxygen", "sha256": self.digest},
            },
        })
        self.enterContext(mock.patch.object(bootstrap, "NATIVE_TOOLS", manifest))
        self.enterContext(mock.patch.object(bootstrap, "native_platform", return_value="fixture-host"))
        self.version = self.enterContext(mock.patch.object(bootstrap, "doxygen_version", side_effect=self.version_of))
        self.download = self.enterContext(mock.patch.object(
            bootstrap.urllib.request, "urlopen", side_effect=lambda *args, **kwargs: io.BytesIO(self.archive.read_bytes())
        ))

    def version_of(self, executable):
        try:
            return "1.0.0" if executable.read_bytes() == self.payload else None
        except OSError:
            return None

    def select(self, install=True):
        return bootstrap.select_doxygen(self.cache, force_pinned=True, install=install)

    def test_reuse(self):
        selected = self.select()
        self.download.side_effect = AssertionError("A ready native cache must not download")
        self.assertEqual(self.select(install=False), selected)
        self.assertEqual(self.select(), selected)

    def test_failed_binary(self):
        self.version.side_effect = lambda path: None
        with self.assertRaises(bootstrap.ToolError):
            self.select()
        self.assertEqual(list(self.cache.rglob("ready.json")), [])

    def test_bad_marker(self):
        executable, _ = self.select()
        marker = executable.parent.parent.parent / "ready.json"
        records = (
            {"sha256": self.digest},
            {"sha256": self.digest, "executable": None},
            {"sha256": self.digest, "executable": "../outside"},
            {"sha256": self.digest, "executable": str(self.root / "outside")},
        )
        for record in records:
            with self.subTest(record=record):
                bootstrap.atomic_json(marker, record)
                with self.assertRaises(bootstrap.ToolError):
                    self.select(install=False)
                self.assertEqual(self.select(), (executable, "1.0.0"))
        self.assertEqual(self.download.call_count, 1)

    def test_corrupt_archive(self):
        executable, _ = self.select()
        prefix = executable.parent.parent.parent
        (prefix / "ready.json").unlink()
        (prefix / "download").write_bytes(b"corrupt cached download")
        self.assertEqual(self.select(), (executable, "1.0.0"))
        self.assertEqual(self.download.call_count, 2)
        self.assertEqual(hashlib.sha256((prefix / "download").read_bytes()).hexdigest(), self.digest)

    def test_unsupported_host(self):
        with mock.patch.object(bootstrap, "native_platform", return_value="unsupported-host"):
            with self.assertRaises(bootstrap.ToolError):
                self.select()
        self.download.assert_not_called()
        self.assertFalse(self.cache.exists())

    def test_newer_system(self):
        native = self.root / "system-doxygen"
        with (
            mock.patch.object(bootstrap.shutil, "which", return_value=str(native)),
            mock.patch.object(bootstrap, "doxygen_version", return_value="2.0.0"),
        ):
            self.assertEqual(bootstrap.select_doxygen(self.cache, False, False), (native, "2.0.0"))
        self.download.assert_not_called()

    def test_old_system(self):
        with (
            mock.patch.object(bootstrap.shutil, "which", return_value=str(self.root / "system-doxygen")),
            mock.patch.object(bootstrap, "doxygen_version", return_value="0.9.0"),
        ):
            with self.assertRaises(bootstrap.ToolError):
                bootstrap.select_doxygen(self.cache, False, False)
        self.download.assert_not_called()


class PipInstallerTests(TemporaryCase):
    def setUp(self):
        super().setUp()
        self.payload = b"fixture wheel"
        manifest = self.root / "native.json"
        bootstrap.atomic_json(manifest, {"python_installer": {
            "version": "1.0.0", "url": "https://example.invalid/pip.whl",
            "sha256": hashlib.sha256(self.payload).hexdigest(),
        }})
        self.enterContext(mock.patch.object(bootstrap, "NATIVE_TOOLS", manifest))
        self.download = self.enterContext(mock.patch.object(
            bootstrap.urllib.request, "urlopen", side_effect=lambda *args, **kwargs: io.BytesIO(self.payload)
        ))

    def test_verified_reuse(self):
        wheel = bootstrap.pip_installer(self.cache, {"pip": "1.0.0"})
        self.assertEqual(wheel.read_bytes(), self.payload)
        self.assertEqual(bootstrap.pip_installer(self.cache, {"pip": "1.0.0"}), wheel)
        self.assertEqual(self.download.call_count, 1)
        wheel.write_bytes(b"corrupt cached wheel")
        self.assertEqual(bootstrap.pip_installer(self.cache, {"pip": "1.0.0"}), wheel)
        self.assertEqual(wheel.read_bytes(), self.payload)
        self.assertEqual(self.download.call_count, 2)

    def test_pin_mismatch(self):
        with self.assertRaises(bootstrap.ToolError):
            bootstrap.pip_installer(self.cache, {"pip": "2.0.0"})
        self.download.assert_not_called()
        self.assertFalse(self.cache.exists())


CHILD_SETUP = textwrap.dedent(r'''
    import importlib.util
    import json
    import os
    from pathlib import Path
    import sys

    spec = importlib.util.spec_from_file_location("fixture_bootstrap", sys.argv[1])
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    cache = Path(sys.argv[2])
    module.environment_identity = lambda: "fixture"
    module.locked_versions = lambda: {"fixture": "1.0.0"}
    module.pip_installer = lambda *args: cache / "fixture-pip.whl"
    prefix = cache / "python/fixture"
    installed = prefix / "installed"
    module.probe_python = lambda *args, **kwargs: {"fixture": "1.0.0"} if installed.exists() else None

    class Builder:
        def __init__(self, *args, **kwargs):
            pass

        def create(self, path):
            path.mkdir(parents=True, exist_ok=True)

    def install(*args, **kwargs):
        with (cache / "receipts").open("a", encoding="utf-8") as output:
            output.write(str(os.getpid()) + "\n")
        print("install", flush=True)
        if not sys.stdin.readline():
            raise RuntimeError("Fixture installer was interrupted")
        installed.write_text("complete", encoding="utf-8")

    module.venv.EnvBuilder = Builder
    module.subprocess.run = install
    print("ready", flush=True)
    sys.stdin.readline()
    python, versions = module.cached_python(cache, install=True)
    print("done:" + str(python), flush=True)
''')


class LockTests(TemporaryCase):
    def setUp(self):
        super().setUp()
        self.children = []
        self.readers = []
        self.messages = queue.Queue()
        self.addCleanup(self.stop_children)

    def stop_children(self):
        for child in self.children:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=5)
            child.stdin.close()
        for reader in self.readers:
            reader.join(timeout=5)
        for child in self.children:
            child.stdout.close()
            child.stderr.close()

    def child(self, script=CHILD_SETUP):
        child = subprocess.Popen(
            [sys.executable, "-u", "-c", script, str(BOOTSTRAP_PATH), str(self.cache)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
        )
        self.children.append(child)

        def read():
            for line in child.stdout:
                self.messages.put((child, line.rstrip("\n")))

        reader = threading.Thread(target=read, daemon=True)
        self.readers.append(reader)
        reader.start()
        return child

    def message(self):
        try:
            return self.messages.get(timeout=5)
        except queue.Empty:
            self.fail("Fixture child did not reach the expected checkpoint")

    def release(self, child):
        child.stdin.write("continue\n")
        child.stdin.flush()

    def finish(self, child):
        child.wait(timeout=5)
        self.assertEqual(child.returncode, 0, child.stderr.read())

    def test_parallel(self):
        children = [self.child() for _ in range(3)]
        self.assertEqual([self.message()[1] for _ in children], ["ready"] * 3)
        for child in children:
            self.release(child)
        owner, event = self.message()
        self.assertEqual(event, "install")
        with self.assertRaises(queue.Empty):
            self.messages.get(timeout=0.25)
        self.release(owner)
        completed = [self.message()[1] for _ in children]
        self.assertTrue(all(event.startswith("done:") for event in completed), completed)
        self.assertEqual(len(set(completed)), 1)
        for child in children:
            self.finish(child)
        self.assertEqual(len((self.cache / "receipts").read_text().splitlines()), 1)
        self.assertEqual(bootstrap.read_json(self.cache / "python/fixture/ready.json")["versions"], {"fixture": "1.0.0"})

    def test_abandoned(self):
        owner = self.child()
        self.assertEqual(self.message(), (owner, "ready"))
        self.release(owner)
        self.assertEqual(self.message(), (owner, "install"))
        waiter = self.child()
        self.assertEqual(self.message(), (waiter, "ready"))
        self.release(waiter)
        with self.assertRaises(queue.Empty):
            self.messages.get(timeout=0.25)
        owner.kill()
        owner.wait(timeout=5)
        self.assertEqual(self.message(), (waiter, "install"))
        self.assertFalse((self.cache / "python/fixture/ready.json").exists())
        self.release(waiter)
        child, event = self.message()
        self.assertIs(child, waiter)
        self.assertTrue(event.startswith("done:"), event)
        self.finish(waiter)
        self.assertEqual(len((self.cache / "receipts").read_text().splitlines()), 2)


if __name__ == "__main__":
    unittest.main()
