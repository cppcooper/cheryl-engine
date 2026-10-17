"""Check native notice selection without querying the host package database."""

import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


spec = importlib.util.spec_from_file_location("native_notices", Path(__file__).with_name("native_notices.py"))
notices = importlib.util.module_from_spec(spec)
spec.loader.exec_module(notices)


class NoticeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.system = self.root / "system"
        self.destination = self.root / "sdk/share/licenses/native"
        self.prefix = self.root / "provider"
        self.owners = {}
        self.calls = []

    def file(self, name, contents="notice\n"):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")
        return path

    def library(self, name="libsample.so.1", package="sample", manager="dpkg-query"):
        library = self.file("system/usr/lib/" + name, "binary")
        if package:
            self.owners[manager, str(library)] = package
        return library

    def query(self, command, **kwargs):
        self.calls.append(command)
        package = self.owners.get((command[0], command[-1]))
        stdout = f"{package}: {command[-1]}\n" if command[0] == "dpkg-query" else f"{package}\n"
        return subprocess.CompletedProcess(command, 0 if package else 1, stdout=stdout if package else "")

    def collect(self, libraries, **kwargs):
        return notices.collect_notices(libraries, self.destination, system_root=self.system, run=self.query,
                                       query_packages=True, **kwargs)

    def test_debian(self):
        library = self.library(package="sample:amd64")
        self.file("system/usr/share/doc/sample/copyright", "See /usr/share/common-licenses/LGPL-2.1.\n")
        self.file("system/usr/share/common-licenses/LGPL-2.1", "LGPL text\n")
        result = self.collect([library])
        self.assertEqual(result["libraries"][0]["package"], "sample:amd64")
        self.assertEqual(result["libraries"][0]["notices"], ["sample/copyright", "sample/references/LGPL-2.1"])
        self.assertEqual((self.destination / "sample/references/LGPL-2.1").read_text(), "LGPL text\n")
        self.assertNotIn(str(self.root), json.dumps(result))

    def test_arch(self):
        library = self.library(manager="pacman")
        self.file("system/usr/share/licenses/sample/LICENSE", "license\n")
        self.file("system/usr/share/licenses/sample/extra/NOTICE", "attribution\n")
        result = self.collect([library])
        self.assertEqual(result["libraries"][0]["notices"], ["sample/licenses/LICENSE", "sample/licenses/extra/NOTICE"])
        self.assertTrue(any(command[0] == "pacman" for command in self.calls))

    def test_relative(self):
        library = self.library()
        self.file("system/usr/share/doc/sample/copyright", "See COPYING\n")
        self.file("system/usr/share/doc/sample/COPYING", "See copyright\n")
        result = self.collect([library])
        self.assertEqual(result["libraries"][0]["notices"], ["sample/copyright", "sample/references/COPYING"])

    def test_license_link_text(self):
        library = self.library()
        self.file("system/usr/share/doc/sample/copyright", "LICENSES/LGPL-2.1-or-later.txt\n")
        self.file("system/usr/share/doc/sample/LICENSES/LGPL-2.1-or-later.txt", "LGPL text\n")
        result = self.collect([library])
        self.assertEqual(result["libraries"][0]["notices"],
                         ["sample/copyright", "sample/references/LGPL-2.1-or-later.txt"])

    def test_override(self):
        library = self.library()
        self.file("system/usr/share/doc/sample/copyright", "system copy\n")
        self.file("provider/share/sample/LICENSE", "provider copy\n")
        result = self.collect([library], native_prefix=self.prefix)
        self.assertEqual(result["libraries"][0]["origin"], "native-prefix")
        self.assertEqual((self.destination / "sample/LICENSE").read_text(), "provider copy\n")
        self.assertFalse((self.destination / "sample/copyright").exists())

    def test_unowned(self):
        library = self.library("libicuuc.so.78", package=None)
        self.file("provider/share/icu/copyright", "ICU notice\n")
        self.file("provider/share/unrelated/LICENSE", "unrelated\n")
        result = self.collect([library], native_prefix=self.prefix)
        self.assertEqual(result["libraries"][0]["package"], "icu")
        self.assertEqual(result["libraries"][0]["notices"], ["icu/copyright"])

    def test_symlink_owner(self):
        library = self.library("libsample.so.1.2")
        link = library.with_name("libsample.so")
        link.symlink_to(library.name)
        self.file("system/usr/share/doc/sample/copyright")
        result = self.collect([link])
        self.assertEqual(result["libraries"][0]["library"], "libsample.so")
        self.assertEqual(result["libraries"][0]["package"], "sample")

    def test_missing(self):
        library = self.library("libdw.so.1", package="libelf", manager="pacman")
        self.file("provider/share/unrelated/LICENSE")
        with self.assertRaisesRegex(ValueError, r"No native notices for libdw.so.1 \(libelf\).+share/libelf/"):
            self.collect([library], native_prefix=self.prefix)
        self.assertFalse(self.destination.exists())

    def test_selection(self):
        library = self.library()
        self.file("provider/share/sample/README", "not a license\n")
        with self.assertRaisesRegex(ValueError, "No native notices"):
            self.collect([library], native_prefix=self.prefix)

    def test_owned(self):
        libraries = [self.library(name) for name in ("libcheryl-engine.so.5", "libcengine.so", "libcmRmlUi.so",
                                                   "libcmNativeGLFW.so", "libglfw.so.3", "libtgui.so.1", "librmlui.so")]
        result = self.collect(libraries)
        self.assertEqual(result["libraries"], [])
        self.assertEqual(result["already_in_sdk"], sorted(library.name for library in libraries))
        self.assertEqual(self.calls, [])

    def test_shared_package(self):
        libraries = [self.library(name, package="libelf") for name in ("libdw.so.1", "libelf.so.1")]
        self.file("system/usr/share/doc/libelf/copyright")
        result = self.collect(libraries)
        self.assertEqual(len(result["libraries"]), 2)
        self.assertEqual(list(self.destination.rglob("copyright")), [self.destination / "libelf/copyright"])

    def test_missing_manager(self):
        library = self.library(package=None)
        self.file("provider/share/sample/LICENSE")

        def unavailable(command, **kwargs):
            raise FileNotFoundError(command[0])

        result = notices.collect_notices([library], self.destination, native_prefix=self.prefix,
                                         system_root=self.system, run=unavailable, query_packages=True)
        self.assertEqual(result["libraries"][0]["package"], "sample")

    def test_windows_prefix(self):
        library = self.library("hwloc-15.dll", package=None)
        self.file("provider/share/hwloc/copyright", "hwloc notice\n")

        def unavailable(command, **kwargs):
            self.fail("A Windows native-prefix collection must not query Linux package managers")

        result = notices.collect_notices([library], self.destination, native_prefix=self.prefix,
                                         system_root=self.system, run=unavailable, query_packages=False)
        self.assertEqual(result["libraries"][0]["package"], "hwloc")
        self.assertEqual(result["libraries"][0]["notices"], ["hwloc/copyright"])


if __name__ == "__main__":
    unittest.main()
