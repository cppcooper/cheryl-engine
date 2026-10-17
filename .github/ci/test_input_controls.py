"""Check control generation rejects unmapped native additions."""

import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


script = Path(__file__).resolve().parents[2] / "tools/input/generate_controls.py"
spec = importlib.util.spec_from_file_location("input_controls", script)
controls = importlib.util.module_from_spec(spec)
spec.loader.exec_module(controls)


class ControlGenerationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.keys = root / "glfw.h"
        self.keys.write_text("#define GLFW_KEY_A 65\n#define GLFW_KEY_LEFT_CONTROL 341\n#define GLFW_KEY_F25 314\n")
        self.portable = root / "controls.h"
        self.portable.write_text("enum class KeyboardKey { Unknown, A, LeftControl, F25 };\n"
                                 "enum class GamepadControl { Unknown, LeftStickX, A };\n")
        (root / "GainputInputDeviceKeyboard.h").write_text("enum Key { KeyA, KeyCtrlL };")
        self.pad = root / "GainputInputDevicePad.h"
        self.pad.write_text("enum PadButton { PadButtonLeftStickX, PadButtonA, PadButtonMax_ };")
        self.paths = patch.multiple(controls, CONTROLS=self.portable, GLFW=self.keys, GAINPUT=root)
        self.paths.start()
        self.addCleanup(self.paths.stop)

    def test_names(self):
        generated = controls.generate()
        self.assertIn("GLFW_KEY_LEFT_CONTROL, CE::Input::KeyboardKey::LeftControl, gainput::KeyCtrlL", generated)
        self.assertIn("GLFW_KEY_F25, CE::Input::KeyboardKey::F25, gainput::InvalidDeviceButtonId", generated)
        self.assertIn("gainput::PadButtonA, CE::Input::GamepadControl::A", generated)

    def test_new_key(self):
        with self.keys.open("a") as stream:
            stream.write("#define GLFW_KEY_B 66\n")
        with self.assertRaisesRegex(ValueError, "GLFW keys need portable identities"):
            controls.generate()

    def test_new_pad(self):
        self.pad.write_text("enum PadButton { PadButtonLeftStickX, PadButtonA, PadButtonB, PadButtonMax_ };")
        with self.assertRaisesRegex(ValueError, "Gainput controls need portable identities"):
            controls.generate()

    def test_missing_key(self):
        self.portable.write_text("enum class KeyboardKey { Unknown, B };\n"
                                 "enum class GamepadControl { Unknown, LeftStickX, A };\n")
        with self.assertRaisesRegex(ValueError, "No unique GLFW mapping"):
            controls.generate()


if __name__ == "__main__":
    unittest.main()
