"""Check relocation validation without loading or compiling native libraries."""

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location("sdk_package", Path(__file__).with_name("package.py"))
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)
spec = importlib.util.spec_from_file_location("sdk_verify", Path(__file__).with_name("verify_sdk.py"))
verify_sdk = importlib.util.module_from_spec(spec)
with patch.dict("sys.modules", {"package": package}):
    spec.loader.exec_module(verify_sdk)


class RelocationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.sdk = self.root / "relocated sdk"
        self.library = self.sdk / "lib/libfreetype.so.6"
        self.library.parent.mkdir(parents=True)
        self.library.write_bytes(b"native runtime fixture")

    def result(self, command, output):
        return subprocess.CompletedProcess(command, 0, stdout=output, stderr="")

    def test_runpath(self):
        for path in (None, "/producer/lib", "$ORIGIN:/producer/lib"):
            dynamic = "" if path is None else f"(RUNPATH) Library runpath: [{path}]\n"
            with self.subTest(path=path), patch.object(verify_sdk.subprocess, "run",
                    side_effect=lambda command, **kwargs: self.result(command, dynamic)) as execute:
                with self.assertRaisesRegex(ValueError, "relative runtime search path"):
                    verify_sdk.check_relocated_runtimes(self.sdk, {"system": "Linux"})
                self.assertEqual(execute.call_args.args[0][0], "readelf")

    def test_external_runtime(self):
        outside = self.root / "host libraries/libpng16.so.16"
        outside.parent.mkdir()
        outside.write_bytes(b"host runtime fixture")

        def execute(command, **kwargs):
            output = ("(RUNPATH) Library runpath: [$ORIGIN]\n" if command[0] == "readelf"
                      else f"libpng16.so.16 => {outside} (0x123456)\n")
            return self.result(command, output)

        with patch.object(verify_sdk.subprocess, "run", side_effect=execute), \
             self.assertRaisesRegex(ValueError, "loads libpng16.so.16 outside its package"):
            verify_sdk.check_relocated_runtimes(self.sdk, {"system": "Linux"})

    def test_relocated_runtime(self):
        dependency = self.library.with_name("libpng16.so.16")
        dependency.write_bytes(b"packaged runtime fixture")
        commands = []

        def execute(command, **kwargs):
            commands.append(command)
            output = "(RUNPATH) Library runpath: [$ORIGIN]\n"
            if command[0] == "ldd":
                output = "libc.so.6 => /system/libc.so.6 (0x123456)\n"
                if command[1] == self.library:
                    output += f"libpng16.so.16 => {dependency} (0x234567)\n"
            return self.result(command, output)

        with patch.object(verify_sdk.subprocess, "run", side_effect=execute):
            verify_sdk.check_relocated_runtimes(self.sdk, {"system": "Linux"})
        self.assertEqual({Path(command[1]) for command in commands if command[0] == "ldd"}, {self.library, dependency})
        self.assertEqual(len(commands), 4)

    def test_missing_runtime(self):
        def execute(command, **kwargs):
            output = ("(RUNPATH) Library runpath: [$ORIGIN]\n" if command[0] == "readelf"
                      else "libpng16.so.16 => not found\n")
            return self.result(command, output)

        with patch.object(verify_sdk.subprocess, "run", side_effect=execute), \
             self.assertRaisesRegex(ValueError, "cannot load libpng16.so.16"):
            verify_sdk.check_relocated_runtimes(self.sdk, {"system": "Linux"})

    def test_windows(self):
        with patch.object(verify_sdk.subprocess, "run") as execute:
            verify_sdk.check_relocated_runtimes(self.sdk, {"system": "Windows"})
        execute.assert_not_called()


if __name__ == "__main__":
    unittest.main()
