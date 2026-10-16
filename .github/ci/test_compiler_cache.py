"""Check compiler cache layers, worker artifacts and family publication."""

import importlib.util
import io
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import tarfile
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location(
    "compiler_cache", Path(__file__).with_name("compiler_cache.py")
)
compiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler)

PREFIX = "ccache-v3-Linux-X64-linux-gcc14"
ENTRY_KEY = "ab/" + "c" * 38
OTHER_KEY = "ab/" + "d" * 38
THIRD_KEY = "de/" + "a" * 38
MANIFEST_KEY = "ef/" + "1" * 38
# Native paths emitted by ccache 4.9.1 during the isolated integration probe.
LEGACY_ENTRY_KEY = "b2/896vhhiqfc3do1apbsusjkkq4kaku90"
LEGACY_MANIFEST_KEY = "78/11h1krhvauruo5qojg5tqov17loqsk2"
# The same 20-byte result digest encoded by ccache 4.14's file backend.
MODERN_EQUIVALENT_KEY = "b2/8937e31969ec1b7015657cf7274a6894553c90"
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


def write(directory, name, contents, *, mtime=None):
    path = directory / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(contents)
    if mtime is not None:
        os.utime(path, (mtime, mtime))
    return path


def snapshot(directory):
    return {name: (path.read_bytes(), path.stat().st_mtime_ns)
            for name, path in compiler._files(directory).items()}


def archive(path, entries):
    """Write (name, bytes, mtime) entries or explicit TarInfo fixtures."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(path, "w", format=tarfile.PAX_FORMAT) as output:
        for entry in entries:
            if isinstance(entry, tarfile.TarInfo):
                output.addfile(entry)
                continue
            name, contents, mtime = entry
            member = tarfile.TarInfo(name)
            member.size = len(contents)
            member.mtime = mtime
            output.addfile(member, io.BytesIO(contents))
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
        self.assertEqual(Path(pull["delta-path"]), self.root / "build/cache/compiler/delta")
        self.assertEqual(Path(pull["delta-archive"]), self.root / "build/cache/compiler-artifacts/delta.tar")

    def test_keys(self):
        result = compiler.metadata(PREFIX, environment=pull_environment(), event=pull_event(), root=self.root)
        self.assertEqual(result["current-key"], f"{PREFIX}-pr-12345-2")
        self.assertTrue(result["current-key"].startswith(result["current-prefix"]))
        self.assertNotEqual(result["current-prefix"], result["main-prefix"])
        self.assertNotEqual(result["current-prefix"], result["develop-prefix"])
        self.assertEqual(result["restore-main"], "true")
        self.assertEqual(result["restore-develop"], "true")

    def test_prefix(self):
        for prefix in ("ccache-v1-profile", "ccache-v2-ubuntu-24.04-X64-linux-gcc-hwloc-Release",
                       "ccache-v3-../escape", "ccache-v3-profile|read-only", "ccache-v3-profile with spaces",
                       "ccache-v3-Windows-X64-linux-gcc14", "ccache-v3-Linux-X64-windows-msvc"):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                compiler.metadata(prefix, environment=ENVIRONMENT, event={}, root=self.root)

    def test_families(self):
        for prefix in (PREFIX, "ccache-v3-Linux-X64-linux-clang18", "ccache-v3-Windows-X64-windows-msvc"):
            with self.subTest(prefix=prefix):
                result = compiler.metadata(prefix, environment=ENVIRONMENT, event={}, root=self.root)
                self.assertEqual(result["current-key"], f"{prefix}-main-12345-2")

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
        for key in (ENTRY_KEY, LEGACY_ENTRY_KEY):
            with self.subTest(key=key):
                current = write(self.current, key, RESULT_HEADER + b"current serialization metadata")
                parent = write(self.parent, key, RESULT_HEADER + b"parent metadata")
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
        self.delta = Path(self.metadata["delta-path"])
        self.output = Path(self.metadata["delta-archive"])
        self.develop = Path(self.metadata["develop-path"])
        self.main = Path(self.metadata["main-path"])
        self.deltas = self.root / "artifacts"

    def configure(self):
        return compiler.configure(environment=self.environment, event=self.event, root=self.root)

    def pack(self):
        return compiler.pack(self.output, environment=self.environment, event=self.event, root=self.root)

    def publish(self):
        return compiler.publish(self.deltas, environment=self.environment, event=self.event, root=self.root)

    def artifact(self, name, entries):
        return archive(self.deltas / name / "delta.tar", entries)

    def test_configure(self):
        write(self.current, ENTRY_KEY, RESULT_HEADER + b"previous PR result", mtime=10)
        write(self.develop, OTHER_KEY, RESULT_HEADER + b"develop result", mtime=20)
        write(self.main, THIRD_KEY, RESULT_HEADER + b"main result", mtime=30)
        before = {path: snapshot(path) for path in (self.current, self.develop, self.main)}
        result = self.configure()
        self.assertIn(result["CCACHE_REMOTE_ONLY"], ("1", "true"))
        self.assertNotEqual(Path(result["CCACHE_DIR"]), self.current)
        self.assertEqual(result["CCACHE_REMOTE_STORAGE"], compiler.storage_configuration(
            self.delta, [self.current, self.develop, self.main]))
        self.assertEqual(compiler._entries(self.delta), {})
        for path, contents in before.items():
            self.assertEqual(snapshot(path), contents)

    def test_dirty_delta(self):
        entry = write(self.delta, ENTRY_KEY, RESULT_HEADER + b"unfinished worker")
        with self.assertRaises(ValueError):
            self.configure()
        self.assertEqual(entry.read_bytes(), RESULT_HEADER + b"unfinished worker")

    def test_pack(self):
        self.configure()
        write(self.current, ENTRY_KEY, RESULT_HEADER + b"restored result", mtime=10)
        write(self.develop, OTHER_KEY, RESULT_HEADER + b"shared result", mtime=20)
        write(self.main, THIRD_KEY, RESULT_HEADER + b"main result", mtime=30)
        write(self.develop, MANIFEST_KEY, MANIFEST_HEADER + b"old refs", mtime=40)
        before = {path: snapshot(path) for path in (self.current, self.develop, self.main)}
        write(self.delta, ENTRY_KEY, RESULT_HEADER + b"other serialization")
        write(self.delta, OTHER_KEY, RESULT_HEADER + b"shared serialization")
        write(self.delta, THIRD_KEY, RESULT_HEADER + b"main serialization")
        write(self.delta, MANIFEST_KEY, MANIFEST_HEADER + b"new refs", mtime=50)
        result = self.pack()
        self.assertEqual(result, {"has-content": "true", "removed": "3", "trimmed": "0"})
        with tarfile.open(self.output, "r:") as packed:
            self.assertEqual(packed.getnames(), [MANIFEST_KEY])
            self.assertEqual(packed.extractfile(MANIFEST_KEY).read(), MANIFEST_HEADER + b"new refs")
        for path, contents in before.items():
            self.assertEqual(snapshot(path), contents)

    def test_pack_mtime(self):
        self.configure()
        timestamp = 1_600_000_000.125
        original = write(self.delta, ENTRY_KEY, RESULT_HEADER + b"new result", mtime=timestamp)
        original_mtime = original.stat().st_mtime_ns
        self.assertEqual(self.pack()["has-content"], "true")
        with tarfile.open(self.output, "r:") as packed:
            self.assertEqual(packed.getmember(ENTRY_KEY).mtime, timestamp)
        target = self.deltas / "worker" / "delta.tar"
        target.parent.mkdir(parents=True)
        self.output.rename(target)
        self.assertEqual(self.publish()["changed"], "true")
        self.assertEqual((self.current / ENTRY_KEY).stat().st_mtime_ns, original_mtime)

    def test_native_formats(self):
        self.configure()
        contents = {
            ENTRY_KEY: RESULT_HEADER + b"modern result",
            LEGACY_ENTRY_KEY: RESULT_HEADER + b"legacy result",
            LEGACY_MANIFEST_KEY: MANIFEST_HEADER + b"legacy refs",
        }
        for name, value in contents.items():
            write(self.delta, name, value, mtime=1_600_000_000.125)
        self.assertEqual(self.pack()["has-content"], "true")
        with tarfile.open(self.output, "r:") as packed:
            self.assertEqual(set(packed.getnames()), set(contents))
        target = self.deltas / "worker" / "delta.tar"
        target.parent.mkdir(parents=True)
        self.output.rename(target)
        self.assertEqual(self.publish()["archives"], "1")
        self.assertEqual({name: path.read_bytes() for name, path in compiler._entries(self.current).items()}, contents)

    def test_native_names(self):
        invalid = (
            "ab/" + "cd" + "w" * 29,
            "ab/" + "gg" + "0" * 29,
            "ab/" + "cd" + "0" * 28,
            "ab/" + "cd" + "0" * 30,
            "AB/" + "0" * 38,
            "ab/CACHEDIR.TAG",
            LEGACY_ENTRY_KEY + ".tmp",
        )
        for index, name in enumerate(invalid):
            with self.subTest(name=name):
                directory = self.root / str(index)
                write(directory, ENTRY_KEY, RESULT_HEADER + b"valid modern result")
                write(directory, LEGACY_ENTRY_KEY, RESULT_HEADER + b"valid legacy result")
                write(directory, name, b"unexpected file")
                before = snapshot(directory)
                with self.assertRaises(ValueError) as rejected:
                    compiler._native_entries(directory)
                self.assertIn(repr(name), str(rejected.exception))
                self.assertEqual(snapshot(directory), before)

    def test_distinct_encodings(self):
        self.configure()
        write(self.current, LEGACY_ENTRY_KEY, RESULT_HEADER + b"same digest", mtime=10)
        before = snapshot(self.current)
        write(self.delta, MODERN_EQUIVALENT_KEY, RESULT_HEADER + b"same digest", mtime=20)
        self.assertEqual(self.pack()["removed"], "0")
        with tarfile.open(self.output, "r:") as packed:
            self.assertEqual(packed.getnames(), [MODERN_EQUIVALENT_KEY])
        self.assertEqual(snapshot(self.current), before)

    def test_pack_shadow(self):
        self.configure()
        write(self.current, MANIFEST_KEY, MANIFEST_HEADER + b"current refs", mtime=10)
        write(self.develop, MANIFEST_KEY, MANIFEST_HEADER + b"selected refs", mtime=10)
        write(self.delta, MANIFEST_KEY, MANIFEST_HEADER + b"selected refs", mtime=20)
        self.assertEqual(self.pack()["removed"], "0")
        with tarfile.open(self.output, "r:") as packed:
            self.assertEqual(packed.extractfile(MANIFEST_KEY).read(), MANIFEST_HEADER + b"selected refs")

    def test_pack_empty(self):
        self.configure()
        write(self.output.parent, self.output.name, b"stale archive")
        result = self.pack()
        self.assertEqual(result, {"has-content": "false", "removed": "0", "trimmed": "0"})
        self.assertFalse(self.output.exists())

    def test_pack_path(self):
        self.configure()
        outside = self.root / "outside.tar"
        with self.assertRaises(ValueError):
            compiler.pack(outside, environment=self.environment, event=self.event, root=self.root)
        self.assertFalse(outside.exists())

    def test_pack_format(self):
        self.configure()
        write(self.delta, "unexpected/file", b"not a native cache entry")
        with self.assertRaises(ValueError):
            self.pack()
        self.assertFalse(self.output.exists())

    def test_publish_union(self):
        original = write(self.current, ENTRY_KEY, RESULT_HEADER + b"original result", mtime=10)
        write(self.current, MANIFEST_KEY, MANIFEST_HEADER + b"old refs", mtime=10)
        self.artifact("a-worker", [
            (ENTRY_KEY, RESULT_HEADER + b"other serialization", 30),
            (OTHER_KEY, RESULT_HEADER + b"first job", 20),
            (MANIFEST_KEY, MANIFEST_HEADER + b"newest complete refs", 40),
        ])
        self.artifact("z-worker", [
            (THIRD_KEY, RESULT_HEADER + b"second job", 25),
            (MANIFEST_KEY, MANIFEST_HEADER + b"older complete refs", 35),
        ])
        result = self.publish()
        self.assertEqual(result["changed"], "true")
        self.assertEqual(result["archives"], "2")
        self.assertEqual(set(compiler._entries(self.current)), {ENTRY_KEY, OTHER_KEY, THIRD_KEY, MANIFEST_KEY})
        self.assertEqual(original.read_bytes(), RESULT_HEADER + b"original result")
        self.assertEqual(original.stat().st_mtime_ns, 30_000_000_000)
        self.assertEqual((self.current / OTHER_KEY).read_bytes(), RESULT_HEADER + b"first job")
        self.assertEqual((self.current / THIRD_KEY).read_bytes(), RESULT_HEADER + b"second job")
        self.assertEqual((self.current / MANIFEST_KEY).read_bytes(), MANIFEST_HEADER + b"newest complete refs")

    def test_manifest_tie(self):
        self.artifact("z-worker", [(MANIFEST_KEY, MANIFEST_HEADER + b"z refs", 20)])
        self.artifact("a-worker", [(MANIFEST_KEY, MANIFEST_HEADER + b"a refs", 20)])
        self.publish()
        self.assertEqual((self.current / MANIFEST_KEY).read_bytes(), MANIFEST_HEADER + b"z refs")

    def test_publish_parents(self):
        write(self.develop, ENTRY_KEY, RESULT_HEADER + b"parent result", mtime=10)
        write(self.main, THIRD_KEY, RESULT_HEADER + b"main result", mtime=20)
        before = {path: snapshot(path) for path in (self.develop, self.main)}
        self.artifact("worker", [
            (ENTRY_KEY, RESULT_HEADER + b"other serialization", 30),
            (THIRD_KEY, RESULT_HEADER + b"main serialization", 30),
            (OTHER_KEY, RESULT_HEADER + b"own result", 30),
        ])
        result = self.publish()
        self.assertEqual(result["changed"], "true")
        self.assertEqual(result["removed"], "2")
        self.assertEqual(set(compiler._entries(self.current)), {OTHER_KEY})
        for path, contents in before.items():
            self.assertEqual(snapshot(path), contents)

    def test_publish_trim(self):
        write(self.current, ENTRY_KEY, RESULT_HEADER + b"old", mtime=10)
        newest = RESULT_HEADER + b"newest"
        self.artifact("worker", [(OTHER_KEY, newest, 20)])
        trim = compiler.trim
        with patch.object(compiler, "trim", side_effect=lambda directory: trim(directory, limit=len(newest))):
            result = self.publish()
        self.assertEqual(result["trimmed"], "1")
        self.assertEqual(result["changed"], "true")
        self.assertEqual(set(compiler._entries(self.current)), {OTHER_KEY})
        self.assertEqual((self.current / OTHER_KEY).read_bytes(), newest)

    def test_unchanged(self):
        write(self.current, ENTRY_KEY, RESULT_HEADER + b"own result", mtime=10)
        before = snapshot(self.current)
        result = self.publish()
        self.assertEqual(result["changed"], "false")
        self.assertEqual(result["has-content"], "true")
        self.assertEqual(result["archives"], "0")
        self.assertEqual(snapshot(self.current), before)

    def test_empty_layer(self):
        result = self.publish()
        self.assertEqual(result["changed"], "true")
        self.assertEqual(result["has-content"], "true")
        self.assertTrue((self.current / compiler.EMPTY_MARKER).exists())
        again = self.publish()
        self.assertEqual(again["changed"], "false")
        self.assertEqual(again["has-content"], "true")

    def test_empty_replacement(self):
        write(self.develop, ENTRY_KEY, RESULT_HEADER + b"parent result", mtime=10)
        write(self.current, ENTRY_KEY, RESULT_HEADER + b"own serialization", mtime=20)
        before = snapshot(self.develop)
        result = self.publish()
        self.assertEqual(result["changed"], "true")
        self.assertEqual(result["has-content"], "true")
        self.assertEqual(result["removed"], "1")
        self.assertTrue((self.current / compiler.EMPTY_MARKER).is_file())
        self.assertEqual(compiler._entries(self.current), {})
        self.assertEqual(snapshot(self.develop), before)

    def test_restored_empty(self):
        marker = write(self.current, compiler.EMPTY_MARKER, b"restored tombstone", mtime=10)
        result = self.publish()
        self.assertEqual(result["changed"], "false")
        self.assertEqual(result["has-content"], "true")
        self.assertTrue(marker.is_file())

    def test_new_entries(self):
        marker = write(self.current, compiler.EMPTY_MARKER, b"")
        self.artifact("worker", [(ENTRY_KEY, RESULT_HEADER + b"new result", 20)])
        result = self.publish()
        self.assertEqual(result["changed"], "true")
        self.assertFalse(marker.exists())
        self.assertEqual((self.current / ENTRY_KEY).read_bytes(), RESULT_HEADER + b"new result")

    def test_trim(self):
        write(self.current, LEGACY_ENTRY_KEY, b"12345", mtime=10)
        write(self.current, OTHER_KEY, b"123456", mtime=20)
        write(self.current, THIRD_KEY, b"1234567", mtime=30)
        self.assertEqual(compiler.trim(self.current, limit=13), 1)
        self.assertEqual(set(compiler._entries(self.current)), {OTHER_KEY, THIRD_KEY})

    def test_legacy_plan(self):
        prefix = "ccache-v2-ubuntu-24.04-X64-linux-gcc-hwloc-Release"
        result = compiler.migration_plan(prefix, environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(result, {"path": str(self.current), "prefix": prefix + "-pr-",
                                  "key": prefix + "-pr-12345-2"})
        self.assertEqual(self.environment["CHERYL_CCACHE_PREFIX"], PREFIX)

    def test_legacy_family(self):
        for prefix in ("ccache-v2-ubuntu-24.04-X64-linux-clang-debug-Debug",
                       "ccache-v2-windows-2025-X64-windows-msvc-hwloc-RelWithDebInfo",
                       "ccache-v2-ubuntu-24.04-ARM64-linux-gcc-hwloc-Release",
                       "ccache-v2-ubuntu-24.04-X64-unknown-Release"):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                compiler.migration_plan(prefix, environment=self.environment, event=self.event, root=self.root)

    def test_legacy_profiles(self):
        profiles = (
            (PREFIX, "ubuntu-24.04", "linux-gcc-hwloc-Release"),
            (PREFIX, "ubuntu-24.04", "linux-gcc-neutral-Release"),
            (PREFIX, "ubuntu-24.04", "linux-modules-Release"),
            (PREFIX, "ubuntu-24.04", "linux-modules-shared-Release"),
            ("ccache-v3-Linux-X64-linux-clang18", "ubuntu-24.04", "linux-clang-debug-Debug"),
            ("ccache-v3-Windows-X64-windows-msvc", "windows-2025", "windows-msvc-hwloc-RelWithDebInfo"),
            ("ccache-v3-Windows-X64-windows-msvc", "windows-2025", "windows-modules-RelWithDebInfo"),
            ("ccache-v3-Windows-X64-windows-msvc", "windows-2025", "windows-modules-shared-RelWithDebInfo"),
        )
        for family, runner, profile in profiles:
            with self.subTest(family=family, profile=profile):
                prefix = f"ccache-v2-{runner}-X64-{profile}"
                environment = {**self.environment, "CHERYL_CCACHE_PREFIX": family}
                result = compiler.migration_plan(prefix, environment=environment, event=self.event, root=self.root)
                self.assertEqual(result["path"], str(self.current))
                self.assertEqual(result["prefix"], prefix + "-pr-")

    def test_legacy_union(self):
        write(self.develop, THIRD_KEY, RESULT_HEADER + b"develop result", mtime=5)
        parent = snapshot(self.develop)
        write(self.current, LEGACY_ENTRY_KEY, RESULT_HEADER + b"first profile", mtime=10)
        write(self.current, LEGACY_MANIFEST_KEY, MANIFEST_HEADER + b"first refs", mtime=10)
        first = compiler.import_legacy(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(first["imported"], "2")
        self.assertEqual(compiler._entries(self.current), {})
        write(self.current, OTHER_KEY, RESULT_HEADER + b"second profile", mtime=20)
        write(self.current, LEGACY_MANIFEST_KEY, MANIFEST_HEADER + b"second refs", mtime=20)
        second = compiler.import_legacy(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(second["imported"], "2")
        self.assertEqual(compiler._entries(self.current), {})
        result = self.publish()
        self.assertEqual(result["changed"], "true")
        self.assertEqual(result["archives"], "0")
        self.assertEqual(set(compiler._entries(self.current)), {LEGACY_ENTRY_KEY, OTHER_KEY, LEGACY_MANIFEST_KEY})
        self.assertEqual((self.current / LEGACY_MANIFEST_KEY).read_bytes(), MANIFEST_HEADER + b"second refs")
        self.assertEqual(snapshot(self.develop), parent)

    def test_legacy_empty(self):
        marker = write(self.current, compiler.EMPTY_MARKER, b"old empty snapshot")
        result = compiler.import_legacy(environment=self.environment, event=self.event, root=self.root)
        self.assertEqual(result["imported"], "0")
        self.assertFalse(marker.exists())
        published = self.publish()
        self.assertEqual(published["changed"], "true")
        self.assertEqual(published["has-content"], "true")

    def test_archive_links(self):
        write(self.develop, OTHER_KEY, RESULT_HEADER + b"parent result", mtime=10)
        protected = snapshot(self.develop)
        outside = write(self.root, "outside", b"protected outside file")
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE):
            with self.subTest(kind=kind):
                member = tarfile.TarInfo(ENTRY_KEY)
                member.type = kind
                member.linkname = str(outside)
                self.artifact("unsafe", [member])
                with self.assertRaises(ValueError):
                    self.publish()
                self.assertEqual(outside.read_bytes(), b"protected outside file")
                self.assertEqual(snapshot(self.develop), protected)
                self.assertEqual(compiler._entries(self.current), {})

    def test_archive_paths(self):
        outside = write(self.root, "outside", b"protected outside file")
        for name in ("../outside", "/outside", r"C:\outside", ENTRY_KEY + "/../outside"):
            with self.subTest(name=name):
                self.artifact("unsafe", [(OTHER_KEY, RESULT_HEADER + b"valid", 10), (name, b"bad", 10)])
                with self.assertRaises(ValueError):
                    self.publish()
                self.assertEqual(outside.read_bytes(), b"protected outside file")
                self.assertEqual(compiler._entries(self.current), {})

    def test_archive_duplicate(self):
        self.artifact("unsafe", [(ENTRY_KEY, b"first", 10), (ENTRY_KEY, b"second", 20)])
        with self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(compiler._entries(self.current), {})

    def test_archive_size(self):
        self.artifact("unsafe", [(ENTRY_KEY, b"12345", 10), (OTHER_KEY, b"67890", 20)])
        with patch.object(compiler, "LAYER_LIMIT", 9), self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(compiler._entries(self.current), {})

    def test_archive_count(self):
        self.artifact("unsafe", [(ENTRY_KEY, b"first", 10), (OTHER_KEY, b"second", 20)])
        with patch.object(compiler, "ARCHIVE_ENTRY_LIMIT", 1), self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(compiler._entries(self.current), {})

    def test_validate_archives(self):
        write(self.current, ENTRY_KEY, RESULT_HEADER + b"restored result", mtime=5)
        before = snapshot(self.current)
        self.artifact("a-valid", [(OTHER_KEY, RESULT_HEADER + b"new result", 10)])
        self.artifact("z-unsafe", [("../outside", b"bad", 10)])
        with self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(snapshot(self.current), before)

    def test_artifact_name(self):
        write(self.deltas, "unexpected.txt", b"not a worker archive")
        with self.assertRaises(ValueError):
            self.publish()


if __name__ == "__main__":
    unittest.main()
