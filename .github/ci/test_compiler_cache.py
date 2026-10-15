"""Check compiler cache layers using temporary files and mocked ccache commands."""

import importlib.util
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location(
    "compiler_cache", Path(__file__).with_name("compiler_cache.py")
)
compiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler)

PREFIX = "ccache-v2-ubuntu-24.04-X64-linux-gcc-hwloc-Release"
ENTRY_KEY = "ab/" + "c" * 38
RESULT_HEADER = b"\xcc\xac\x01\x00"
MANIFEST_HEADER = b"\xcc\xac\x01\x01"
ENVIRONMENT = {
    "GITHUB_EVENT_NAME": "push",
    "GITHUB_REF": "refs/heads/main",
    "GITHUB_RUN_ID": "12345",
    "GITHUB_RUN_ATTEMPT": "2",
    "CHERYL_CCACHE_PREFIX": PREFIX,
}


def pull_environment(number=3):
    return {**ENVIRONMENT, "GITHUB_EVENT_NAME": "pull_request", "GITHUB_REF": f"refs/pull/{number}/merge"}


def pull_event(base="develop", number=3):
    return {"number": number, "pull_request": {"number": number, "base": {"ref": base}}}


def write(directory, name, contents):
    path = directory / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(contents)
    return path


class MetadataTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()

    def test_roles(self):
        for ref, role, restore_main in (
            ("refs/heads/main", "main", False),
            ("refs/heads/develop", "develop", True),
            ("refs/heads/feature", "branch", True),
            ("refs/tags/v4.0.0", "branch", True),
        ):
            with self.subTest(ref=ref):
                result = compiler.select_role({**ENVIRONMENT, "GITHUB_REF": ref}, {})
                self.assertEqual(result["role"], role)
                self.assertEqual(result["restore-main"], restore_main)
                self.assertFalse(result["restore-develop"])

    def test_pull_base(self):
        for base, restore_develop in (("main", False), ("develop", True)):
            with self.subTest(base=base):
                result = compiler.select_role(pull_environment(), pull_event(base))
                self.assertEqual(result["role"], "pr")
                self.assertTrue(result["restore-main"])
                self.assertEqual(result["restore-develop"], restore_develop)

    def test_pull_mismatch(self):
        with self.assertRaises(ValueError):
            compiler.select_role(pull_environment(3), pull_event(number=30))

    def test_paths(self):
        main = compiler.metadata(PREFIX, environment=ENVIRONMENT, event={}, root=self.root)
        develop = compiler.metadata(PREFIX, environment={**ENVIRONMENT, "GITHUB_REF": "refs/heads/develop"},
                                    event={}, root=self.root)
        pull = compiler.metadata(PREFIX, environment=pull_environment(), event=pull_event(), root=self.root)
        self.assertEqual(main["current-path"], pull["main-path"])
        self.assertEqual(develop["current-path"], pull["develop-path"])
        self.assertEqual(Path(main["current-path"]), self.root / "build/cache/compiler/main")
        self.assertEqual(Path(develop["current-path"]), self.root / "build/cache/compiler/develop")
        self.assertEqual(Path(pull["current-path"]), self.root / "build/cache/compiler/pr")

    def test_keys(self):
        result = compiler.metadata(PREFIX, environment=pull_environment(), event=pull_event(), root=self.root)
        self.assertEqual(result["current-key"], f"{PREFIX}-pr-12345-2")
        self.assertTrue(result["current-key"].startswith(result["current-prefix"]))
        self.assertNotEqual(result["current-prefix"], result["main-prefix"])
        self.assertNotEqual(result["current-prefix"], result["develop-prefix"])
        self.assertEqual(result["restore-main"], "true")
        self.assertEqual(result["restore-develop"], "true")

    def test_prefix(self):
        for prefix in ("ccache-v1-profile", "ccache-v2-../escape", "ccache-v2-profile|read-only",
                       "ccache-v2-profile with spaces"):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                compiler.metadata(prefix, environment=ENVIRONMENT, event={}, root=self.root)

    def test_run_identity(self):
        for name, value in (("GITHUB_RUN_ID", "0"), ("GITHUB_RUN_ID", "../escape"),
                            ("GITHUB_RUN_ATTEMPT", "0"), ("GITHUB_RUN_ATTEMPT", "retry")):
            with self.subTest(name=name, value=value), self.assertRaises(ValueError):
                compiler.metadata(PREFIX, environment={**ENVIRONMENT, name: value}, event={}, root=self.root)


class StorageTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.current = self.root / "current"
        self.parent = self.root / "parent"
        self.current.mkdir()
        self.parent.mkdir()

    def test_file_urls(self):
        self.assertEqual(compiler.file_url(PurePosixPath("/tmp/cache layer#1")),
                         "file:///tmp/cache%20layer%231")
        self.assertEqual(compiler.file_url(PureWindowsPath(r"D:\build files\cache#1")),
                         "file:///D:/build%20files/cache%231")

    def test_storage_order(self):
        main = self.root / "main"
        result = compiler.storage_configuration(self.current, [self.parent, main])
        self.assertEqual(result, " ".join((compiler.file_url(self.current) + "|update-mtime=true",
                                          compiler.file_url(self.parent) + "|read-only",
                                          compiler.file_url(main) + "|read-only")))

    def test_fingerprint(self):
        path = write(self.current, "entries/result", b"compiled result")
        initial = compiler.fingerprint(self.current)
        os.utime(path, (1_600_000_000, 1_600_000_000))
        self.assertEqual(compiler.fingerprint(self.current), initial)
        path.write_bytes(b"modified result")
        os.utime(path, (1_600_000_000, 1_600_000_000))
        self.assertNotEqual(compiler.fingerprint(self.current), initial)

    def test_entry_names(self):
        path = write(self.current, "entries/result", b"compiled result")
        initial = compiler.fingerprint(self.current)
        path.rename(path.with_name("renamed"))
        self.assertNotEqual(compiler.fingerprint(self.current), initial)

    def test_deduplicate(self):
        duplicate = write(self.current, "entries/result", b"compiled result")
        unique = write(self.current, "entries/new", b"new result")
        parent = write(self.parent, "entries/result", b"compiled result")
        self.assertEqual(compiler.deduplicate(self.current, [self.parent]), 1)
        self.assertFalse(duplicate.exists())
        self.assertEqual(unique.read_bytes(), b"new result")
        self.assertEqual(parent.read_bytes(), b"compiled result")

    def test_changed_manifest(self):
        current = write(self.current, "entries/manifest", b"new refs")
        parent = write(self.parent, "entries/manifest", b"old refs")
        self.assertEqual(compiler.deduplicate(self.current, [self.parent]), 0)
        self.assertEqual(current.read_bytes(), b"new refs")
        self.assertEqual(parent.read_bytes(), b"old refs")

    def test_result_identity(self):
        current = write(self.current, ENTRY_KEY, RESULT_HEADER + b"current serialization metadata")
        parent = write(self.parent, ENTRY_KEY, RESULT_HEADER + b"parent metadata")
        self.assertEqual(compiler.deduplicate(self.current, [self.parent]), 1)
        self.assertFalse(current.exists())
        self.assertEqual(parent.read_bytes(), RESULT_HEADER + b"parent metadata")

    def test_unknown_format(self):
        for index, (name, current_bytes, parent_bytes) in enumerate((
            (ENTRY_KEY, b"\xcc\xac\x01", RESULT_HEADER + b"parent"),
            (ENTRY_KEY, b"\xcc\xac\x02\x00current", b"\xcc\xac\x02\x00parent"),
            (ENTRY_KEY, b"\xcc\xac\x01\x02current", b"\xcc\xac\x01\x02parent"),
            (ENTRY_KEY, RESULT_HEADER + b"current", b"unknown parent"),
            ("entries/result", RESULT_HEADER + b"current", RESULT_HEADER + b"parent"),
        )):
            with self.subTest(name=name, header=current_bytes[:4]):
                current = self.current / str(index)
                parent = self.parent / str(index)
                entry = write(current, name, current_bytes)
                write(parent, name, parent_bytes)
                self.assertEqual(compiler.deduplicate(current, [parent]), 0)
                self.assertEqual(entry.read_bytes(), current_bytes)

    def test_manifest_shadow(self):
        main = self.root / "main"
        selected = MANIFEST_HEADER + b"main refs"
        entry = write(self.current, ENTRY_KEY, selected)
        develop = write(self.parent, ENTRY_KEY, MANIFEST_HEADER + b"other refs")
        baseline = write(main, ENTRY_KEY, selected)
        self.assertEqual(compiler.deduplicate(self.current, [self.parent, main]), 0)
        self.assertEqual(entry.read_bytes(), selected)
        self.assertEqual(develop.read_bytes(), MANIFEST_HEADER + b"other refs")
        self.assertEqual(baseline.read_bytes(), selected)

    def test_parent_miss(self):
        main = self.root / "main"
        entry = write(self.current, ENTRY_KEY, MANIFEST_HEADER + b"main refs")
        write(main, ENTRY_KEY, MANIFEST_HEADER + b"main refs")
        self.assertEqual(compiler.deduplicate(self.current, [self.parent, main]), 1)
        self.assertFalse(entry.exists())

    def test_other_path(self):
        path = write(self.current, "entries/result", b"same bytes")
        write(self.parent, "elsewhere/result", b"same bytes")
        self.assertEqual(compiler.deduplicate(self.current, [self.parent]), 0)
        self.assertTrue(path.exists())

    def test_missing_parents(self):
        path = write(self.current, "entries/result", b"own result")
        self.assertEqual(compiler.deduplicate(self.current, [self.root / "missing"]), 0)
        self.assertEqual(path.read_bytes(), b"own result")

    def make_link(self, path, target):
        try:
            path.symlink_to(target)
        except (NotImplementedError, OSError) as error:
            self.skipTest(f"Symbolic-link fixture unavailable on this host: {error}")

    def test_symlink(self):
        target = write(self.root, "outside", b"outside layer")
        self.make_link(self.current / "link", target)
        with self.assertRaises(ValueError):
            compiler.fingerprint(self.current)
        self.assertEqual(target.read_bytes(), b"outside layer")

    def test_validate_before_delete(self):
        duplicate = write(self.current, "entries/result", b"compiled result")
        write(self.parent, "entries/result", b"compiled result")
        outside = write(self.root, "outside", b"outside layer")
        self.make_link(self.parent / "unsafe", outside)
        with self.assertRaises(ValueError):
            compiler.deduplicate(self.current, [self.parent])
        self.assertEqual(duplicate.read_bytes(), b"compiled result")
        self.assertEqual(outside.read_bytes(), b"outside layer")


class WorkflowTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.environment = pull_environment()
        self.event = pull_event()
        self.metadata = compiler.metadata(PREFIX, environment=self.environment, event=self.event, root=self.root)
        self.current = Path(self.metadata["current-path"])

    def test_configure(self):
        with patch.object(compiler.subprocess, "run") as command:
            result = compiler.configure(environment=self.environment, event=self.event, root=self.root)
        command.assert_not_called()
        self.assertIn(result["CCACHE_REMOTE_ONLY"], ("1", "true"))
        self.assertNotEqual(Path(result["CCACHE_DIR"]), self.current)
        storage = result["CCACHE_REMOTE_STORAGE"]
        self.assertEqual(storage, compiler.storage_configuration(
            self.current, [Path(self.metadata["develop-path"]), Path(self.metadata["main-path"])]))
        self.assertTrue((self.root / "build/cache/compiler-state.json").is_file())

    def test_cold_start(self):
        with patch.object(compiler.subprocess, "run") as command:
            compiler.configure(environment=self.environment, event=self.event, root=self.root)
            write(self.current, "entries/result", b"new result")
            result = compiler.prepare_save(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(result["changed"], "true")
        self.assertEqual(result["removed"], "0")
        self.assertEqual(command.call_count, 1)
        self.assertEqual(command.call_args.args[0], ["ccache", "--trim-dir", str(self.current),
                                                    "--trim-max-size", "1GiB", "--trim-method", "mtime"])
        self.assertTrue(command.call_args.kwargs["check"])

    def test_hit_only(self):
        path = write(self.current, "entries/result", b"own result")
        with patch.object(compiler.subprocess, "run"):
            compiler.configure(environment=self.environment, event=self.event, root=self.root)
            os.utime(path, (1_600_000_000, 1_600_000_000))
            result = compiler.prepare_save(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(result["changed"], "false")

    def test_parent_subtraction(self):
        parent = Path(self.metadata["develop-path"])
        parent_file = write(parent, "entries/result", b"shared result")
        with patch.object(compiler.subprocess, "run"):
            compiler.configure(environment=self.environment, event=self.event, root=self.root)
            duplicate = write(self.current, "entries/result", b"shared result")
            write(self.current, "entries/new", b"own result")
            result = compiler.prepare_save(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(result, {"changed": "true", "removed": "1"})
        self.assertFalse(duplicate.exists())
        self.assertEqual(parent_file.read_bytes(), b"shared result")

    def test_empty_layer(self):
        with patch.object(compiler.subprocess, "run"):
            compiler.configure(environment=self.environment, event=self.event, root=self.root)
            result = compiler.prepare_save(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(result["changed"], "false")

    def test_empty_replacement(self):
        write(Path(self.metadata["develop-path"]), "entries/result", b"shared result")
        write(self.current, "entries/result", b"shared result")
        empty = self.root / "empty"
        empty.mkdir()
        with patch.object(compiler.subprocess, "run"):
            compiler.configure(environment=self.environment, event=self.event, root=self.root)
            result = compiler.prepare_save(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(result, {"changed": "true", "removed": "1"})
        self.assertTrue((self.current / ".cheryl-empty-layer").is_file())
        self.assertEqual(compiler.fingerprint(self.current), compiler.fingerprint(empty))

    def test_restored_empty(self):
        write(self.current, ".cheryl-empty-layer", b"")
        with patch.object(compiler.subprocess, "run"):
            compiler.configure(environment=self.environment, event=self.event, root=self.root)
            result = compiler.prepare_save(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(result["changed"], "false")
        self.assertEqual(result["removed"], "0")

    def test_new_entries(self):
        marker = write(self.current, ".cheryl-empty-layer", b"")
        with patch.object(compiler.subprocess, "run"):
            compiler.configure(environment=self.environment, event=self.event, root=self.root)
            entry = write(self.current, "entries/result", b"own result")
            result = compiler.prepare_save(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(result["changed"], "true")
        self.assertFalse(marker.exists())
        self.assertEqual(entry.read_bytes(), b"own result")


if __name__ == "__main__":
    unittest.main()
