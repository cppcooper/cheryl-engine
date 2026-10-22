"""Check native preparation policy without installing or building packages."""

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location("prepare", Path(__file__).with_name("prepare.py"))
prepare = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(prepare)


class PreparationTests(unittest.TestCase):
    def test_triplets(self):
        self.assertEqual(prepare.dependency_triplet("Windows", "AMD64"), "x64-windows")
        self.assertEqual(prepare.dependency_triplet("Linux", "x86_64"), "x64-linux-dynamic")
        self.assertEqual(prepare.dependency_triplet("Linux", "aarch64"), "arm64-linux-dynamic")
        with self.assertRaises(ValueError):
            prepare.dependency_triplet("Linux", "unknown")

    def test_supplied_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(prepare, "run", return_value=SimpleNamespace(stdout="unrelated-revision\n")) as run:
                with self.assertRaisesRegex(ValueError, "existing checkout is unchanged"):
                    prepare.prepare(root, root / "installed")
            self.assertEqual(run.call_count, 1)
            self.assertEqual(run.call_args.args[0], ["git", "rev-parse", "HEAD"])
            self.assertFalse((root / "installed").exists())


if __name__ == "__main__":
    unittest.main()
