"""Check global vcpkg package contributions, observed usage and bounded merging."""

import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location("vcpkg_cache", Path(__file__).with_name("vcpkg_cache.py"))
cache = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cache)
ABI_A = "ab" + "1" * 62
ABI_B = "cd" + "2" * 62
KEY_A = f"archives/ab/{ABI_A}.zip"
KEY_B = f"archives/cd/{ABI_B}.zip"
DOWNLOAD = "downloads/icu4c-78_3-src.tgz"
ENVIRONMENT = {"GITHUB_RUN_ID": "12345", "GITHUB_RUN_ATTEMPT": "2"}


def write(directory, name, data):
    path = directory / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data.encode("utf-8") if isinstance(data, str) else data)
    return path


def record(data, *, day=None):
    result = {"sha256": hashlib.sha256(data).hexdigest(), "size": len(data)}
    if day is not None:
        result["last_used"] = day
    return result


def status(abi, *, name="icu", state="install ok installed", feature=None):
    fields = f"Package: {name}\nVersion: 78.3\nArchitecture: x64-windows\nMulti-Arch: same\n"
    if feature:
        fields += f"Feature: {feature}\n"
    return fields + f"Abi: {abi}\nStatus: {state}\n\n"


def snapshot(directory):
    return {name: (path.read_bytes(), path.stat().st_mtime_ns) for name, path in cache._files(directory).items()}


def contribution(directory, name, files, *, used=(), manifest=None, additions=()):
    if manifest is None:
        manifest = {"schema": 1, "files": {key: record(value) for key, value in files.items()}, "used": list(used)}
    encoded = manifest if isinstance(manifest, bytes) else json.dumps(manifest).encode()
    path = directory / name / "contribution.tar"
    path.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(path, "w", format=tarfile.PAX_FORMAT) as archive:
        for key, data in [(cache.MANIFEST_NAME, encoded), *files.items()]:
            member = tarfile.TarInfo(key)
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))
        for member in additions:
            archive.addfile(member)
    return path


class PoolTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.pool = self.root / "build/cache/vcpkg"
        self.installed = self.root / "build/ci/vcpkg_installed"
        self.contributions = self.root / "contributions"
        self.output = self.root / "build/cache/vcpkg-artifacts/contribution.tar"

    def seed(self, files, *, day=1):
        for name, data in files.items():
            write(self.pool, name, data)
        index = {"schema": 1, "files": {name: record(data, day=day) for name, data in files.items()}}
        write(self.pool, cache.INDEX_NAME, cache._encode(index))
        return index

    def pack(self, text=None):
        write(self.installed, "vcpkg/status", status(ABI_A) if text is None else text)
        return cache.pack(self.installed, root=self.root)

    def merge(self, *, day=10, limit=cache.DEFAULT_LIMIT):
        return cache.merge(self.contributions, root=self.root, now=day * 86400 + 123, limit_bytes=limit)

    def stage_output(self, name="worker"):
        target = self.contributions / name / "contribution.tar"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(self.output.read_bytes())

    def test_describe(self):
        values = cache.describe(environment=ENVIRONMENT, root=self.root)
        self.assertEqual(values["cache-prefix"], "vcpkg-v1-Windows-X64-msvc-")
        self.assertEqual(values["cache-key"], "vcpkg-v1-Windows-X64-msvc-12345-2")
        self.assertEqual(Path(values["cache-path"]), self.pool)
        self.assertEqual(Path(values["contribution-path"]), self.output)
        other = cache.describe(environment={**ENVIRONMENT, "GITHUB_REF": "refs/pull/42/merge",
                                            "ImageVersion": "different", "DEPENDENCY_PROFILE": "modules"}, root=self.root)
        self.assertEqual(other, values)

    def test_run_identity(self):
        for name, value in (("GITHUB_RUN_ID", "0"), ("GITHUB_RUN_ID", "../escape"),
                            ("GITHUB_RUN_ATTEMPT", ""), ("GITHUB_RUN_ATTEMPT", "retry")):
            with self.subTest(name=name), self.assertRaises(ValueError):
                cache.describe(environment={**ENVIRONMENT, name: value}, root=self.root)

    def test_status(self):
        text = (status(ABI_A) + status("", feature="tools")
                + status(ABI_B, name="hwloc", state="hold ok installed")
                + status("a" * 64, state="deinstall ok not-installed")
                + status("", name="uncacheable"))
        self.assertEqual(cache.parse_status(text), [KEY_A, KEY_B])

    def test_status_fields(self):
        text = status(ABI_A).replace("Status:", "Description: A description\n    with a continuation\nStatus:")
        self.assertEqual(cache.parse_status(text), [KEY_A])
        for value in (status("not-an-abi"), status(ABI_A) + " invalid continuation\n",
                      status(ABI_A).replace("Abi:", f"Abi: {ABI_B}\nAbi:"), "\0"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                cache.parse_status(value)

    def test_pack_changed(self):
        self.seed({KEY_A: b"existing ZIP", DOWNLOAD: b"old download"})
        write(self.pool, KEY_B, b"new ZIP")
        write(self.pool, DOWNLOAD, b"new download")
        before = snapshot(self.pool)
        result = self.pack(status(ABI_A) + status(ABI_B, name="hwloc"))
        self.assertEqual(result, {"has-content": "true", "files": "2", "used": "2"})
        with tarfile.open(self.output, "r:") as archive:
            self.assertEqual(set(archive.getnames()), {cache.MANIFEST_NAME, KEY_B, DOWNLOAD})
            with archive.extractfile(cache.MANIFEST_NAME) as source:
                manifest = json.load(source)
            self.assertEqual(manifest["used"], [KEY_A, KEY_B])
            self.assertEqual(set(manifest), {"schema", "files", "used"})
        self.assertEqual(snapshot(self.pool), before)

    def test_pack_usage(self):
        self.seed({KEY_A: b"ZIP"})
        before = snapshot(self.pool)
        result = self.pack()
        self.assertEqual(result, {"has-content": "true", "files": "0", "used": "1"})
        with tarfile.open(self.output, "r:") as archive:
            self.assertEqual(archive.getnames(), [cache.MANIFEST_NAME])
        self.assertEqual(snapshot(self.pool), before)

    def test_pack_empty(self):
        write(self.output.parent, self.output.name, b"stale contribution")
        self.assertEqual(self.pack("")["has-content"], "false")
        self.assertFalse(self.output.exists())

    def test_pack_missing_status(self):
        with self.assertRaises(ValueError):
            cache.pack(self.installed, root=self.root)

    def test_pack_bound(self):
        write(self.pool, DOWNLOAD, b"12345")
        write(self.installed, "vcpkg/status", "")
        with patch.object(cache, "CONTRIBUTION_LIMIT", 4), self.assertRaises(ValueError):
            cache.pack(self.installed, root=self.root)
        self.assertFalse(self.output.exists())

    def test_merge_union(self):
        self.seed({KEY_A: b"old ZIP"})
        contribution(self.contributions, "a-worker", {KEY_B: b"new ZIP"}, used=[KEY_B])
        contribution(self.contributions, "z-worker", {DOWNLOAD: b"source archive"}, used=[KEY_A])
        result = self.merge()
        self.assertEqual(result["changed"], "true")
        self.assertEqual(result["files"], "3")
        self.assertEqual((self.pool / KEY_A).read_bytes(), b"old ZIP")
        self.assertEqual((self.pool / KEY_B).read_bytes(), b"new ZIP")
        self.assertEqual((self.pool / DOWNLOAD).read_bytes(), b"source archive")
        index = cache.read_index(self.pool)
        self.assertTrue(all(value["last_used"] == 10 for value in index["files"].values()))
        actual = sum(path.stat().st_size for path in cache._files(self.pool).values())
        self.assertEqual(int(result["bytes"]), actual)

    def test_zip_identity(self):
        self.seed({KEY_A: b"first serialization"})
        contribution(self.contributions, "worker", {KEY_A: b"another serialization"}, used=[KEY_A])
        self.merge()
        self.assertEqual((self.pool / KEY_A).read_bytes(), b"first serialization")
        self.assertEqual(cache.read_index(self.pool)["files"][KEY_A]["last_used"], 10)

    def test_zip_order(self):
        contribution(self.contributions, "z-worker", {KEY_A: b"z serialization"}, used=[KEY_A])
        contribution(self.contributions, "a-worker", {KEY_A: b"a serialization"}, used=[KEY_A])
        self.merge()
        self.assertEqual((self.pool / KEY_A).read_bytes(), b"a serialization")

    def test_download_replace(self):
        self.seed({DOWNLOAD: b"old"})
        contribution(self.contributions, "worker", {DOWNLOAD: b"new"})
        self.merge()
        self.assertEqual((self.pool / DOWNLOAD).read_bytes(), b"new")

    def test_download_revert(self):
        self.seed({DOWNLOAD: b"original"})
        contribution(self.contributions, "a-worker", {DOWNLOAD: b"other"})
        contribution(self.contributions, "z-worker", {DOWNLOAD: b"original"})
        self.merge()
        self.assertEqual((self.pool / DOWNLOAD).read_bytes(), b"original")

    def test_usage_checkpoint(self):
        self.seed({KEY_A: b"ZIP"}, day=10)
        contribution(self.contributions, "worker", {}, used=[KEY_A])
        before = snapshot(self.pool)
        self.assertEqual(self.merge()["changed"], "false")
        self.assertEqual(snapshot(self.pool), before)
        self.assertEqual(self.merge(day=11)["changed"], "true")
        self.assertEqual(cache.read_index(self.pool)["files"][KEY_A]["last_used"], 11)

    def test_unknown_usage(self):
        self.seed({KEY_A: b"ZIP"}, day=10)
        contribution(self.contributions, "worker", {}, used=[KEY_B])
        self.assertEqual(self.merge()["changed"], "false")
        self.assertEqual(set(cache.inventory(self.pool)), {KEY_A})

    def test_roundtrip(self):
        write(self.pool, KEY_A, b"new ZIP")
        write(self.pool, DOWNLOAD, b"download")
        self.pack()
        self.stage_output()
        publisher = self.root / "publisher"
        result = cache.merge(self.contributions, root=publisher, now=10 * 86400)
        self.assertEqual(result["changed"], "true")
        self.assertEqual(cache.inventory(publisher / "build/cache/vcpkg"), cache.inventory(self.pool))

    def test_lru(self):
        self.seed({KEY_A: b"a" * 1024, KEY_B: b"b" * 1024})
        contribution(self.contributions, "worker", {}, used=[KEY_A])
        result = self.merge(limit=1700)
        self.assertEqual(result["removed"], "1")
        self.assertLessEqual(int(result["bytes"]), 1700)
        self.assertEqual(set(cache.inventory(self.pool)), {KEY_A})

    def test_lru_clock(self):
        index = self.seed({KEY_A: b"a" * 1024, KEY_B: b"b" * 1024})
        index["files"][KEY_A]["last_used"] = 2
        write(self.pool, cache.INDEX_NAME, cache._encode(index))
        os.utime(self.pool / KEY_B, (2_000_000_000, 2_000_000_000))
        os.utime(self.pool / KEY_A, (100, 100))
        self.merge(limit=1700)
        self.assertEqual(set(cache.inventory(self.pool)), {KEY_A})

    def test_trim_oversized(self):
        self.seed({KEY_A: b"a" * 100, KEY_B: b"b" * 2000})
        contribution(self.contributions, "worker", {}, used=[KEY_B])
        self.merge(limit=600)
        self.assertEqual(set(cache.inventory(self.pool)), {KEY_A})

    def test_preserve_last(self):
        self.seed({KEY_A: b"a" * 1024})
        before = snapshot(self.pool)
        with self.assertRaises(ValueError):
            self.merge(limit=100)
        self.assertEqual(snapshot(self.pool), before)

    def test_index_integrity(self):
        self.seed({KEY_A: b"ZIP"})
        write(self.pool, KEY_A, b"changed")
        before = snapshot(self.pool)
        with self.assertRaises(ValueError):
            self.merge()
        self.assertEqual(snapshot(self.pool), before)

    def test_index_schema(self):
        for value in ({"schema": 2, "files": {}}, {"schema": True, "files": {}},
                      {"schema": 1, "files": {}, "token": "secret"},
                      {"schema": 1, "files": {KEY_A: record(b"ZIP", day=-1)}}):
            with self.subTest(value=value):
                write(self.pool, cache.INDEX_NAME, json.dumps(value).encode())
                with self.assertRaises(ValueError):
                    cache.read_index(self.pool)

    def test_native_paths(self):
        paths = (f"archives/ff/{ABI_A}.zip", f"archives/ab/{'1' * 40}.zip",
                 "downloads/../credentials", "downloads/.git-credentials", "downloads/.ssh/id_rsa",
                 "downloads/CON.txt", "downloads/a\\b", "downloads/file.", "other/file")
        for name in paths:
            with self.subTest(name=name), self.assertRaises(ValueError):
                cache._payload_path(name)

    def test_inventory_paths(self):
        # Native filesystems can normalize separators and trailing dots before
        # inventory sees them. Keep raw-spelling checks above and in TAR inputs.
        paths = (f"archives/ff/{ABI_A}.zip", f"archives/ab/{'1' * 40}.zip",
                 "downloads/.git-credentials", "downloads/.ssh/id_rsa", "other/file")
        for index, name in enumerate(paths):
            with self.subTest(name=name):
                directory = self.root / str(index)
                write(directory, name, b"unexpected")
                with self.assertRaises(ValueError):
                    cache.inventory(directory)

    def test_contribution_paths(self):
        self.seed({KEY_A: b"ZIP"})
        before = snapshot(self.pool)
        for name in ("../outside", "/absolute", "downloads/.netrc", f"archives/ff/{ABI_B}.zip",
                     "downloads/../credentials", "downloads/a\\b", "downloads/file.", "downloads/CON.txt"):
            with self.subTest(name=name):
                contribution(self.contributions, "unsafe", {name: b"bad"})
                with self.assertRaises(ValueError):
                    self.merge()
                self.assertEqual(snapshot(self.pool), before)

    def test_contribution_links(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE):
            with self.subTest(kind=kind):
                member = tarfile.TarInfo(DOWNLOAD)
                member.type, member.linkname = kind, "../outside"
                contribution(self.contributions, "unsafe", {}, additions=[member])
                with self.assertRaises(ValueError):
                    self.merge()
                self.assertFalse((self.root / "outside").exists())

    def test_case_collision(self):
        contribution(self.contributions, "a-worker", {"downloads/Tool/a.txt": b"a"})
        contribution(self.contributions, "z-worker", {"downloads/tool/b.txt": b"b"})
        with self.assertRaises(ValueError):
            self.merge()
        self.assertFalse((self.pool / cache.INDEX_NAME).exists())

    def test_file_directory(self):
        contribution(self.contributions, "a-worker", {"downloads/tool": b"file"})
        contribution(self.contributions, "z-worker", {"downloads/tool/a.txt": b"nested"})
        with self.assertRaises(ValueError):
            self.merge()
        self.assertFalse((self.pool / "downloads/tool").exists())

    def test_digest(self):
        self.seed({KEY_A: b"ZIP"})
        before = snapshot(self.pool)
        manifest = {"schema": 1, "files": {KEY_B: record(b"different")}, "used": []}
        contribution(self.contributions, "unsafe", {KEY_B: b"corrupted"}, manifest=manifest)
        with self.assertRaises(ValueError):
            self.merge()
        self.assertEqual(snapshot(self.pool), before)

    def test_manifest_schema(self):
        for manifest in ({"schema": 2, "files": {}, "used": []},
                         {"schema": 1, "files": {}, "used": [KEY_A, KEY_A]},
                         {"schema": 1, "files": {}, "used": [DOWNLOAD]},
                         {"schema": 1, "files": {}, "used": [], "credentials": "secret"},
                         b'{"schema":1,"schema":1,"files":{},"used":[]}'):
            with self.subTest(manifest=manifest):
                contribution(self.contributions, "unsafe", {}, manifest=manifest)
                with self.assertRaises(ValueError):
                    self.merge()

    def test_payload_match(self):
        contribution(self.contributions, "unsafe", {KEY_A: b"unlisted"},
                     manifest={"schema": 1, "files": {}, "used": []})
        with self.assertRaises(ValueError):
            self.merge()

    def test_duplicate_member(self):
        member = tarfile.TarInfo(cache.MANIFEST_NAME)
        contribution(self.contributions, "unsafe", {}, additions=[member])
        with self.assertRaises(ValueError):
            self.merge()

    def test_contribution_bound(self):
        contribution(self.contributions, "unsafe", {DOWNLOAD: b"12345"})
        with patch.object(cache, "CONTRIBUTION_LIMIT", 4), self.assertRaises(ValueError):
            self.merge()

    def test_contribution_count(self):
        for number in range(33):
            contribution(self.contributions, f"worker-{number}", {})
        with self.assertRaises(ValueError):
            self.merge()
        self.assertFalse((self.pool / cache.INDEX_NAME).exists())

    def test_aggregate_size(self):
        for number in range(4):
            contribution(self.contributions, f"worker-{number}", {})
        with patch.object(cache, "AGGREGATE_LIMIT", 39_000), self.assertRaises(ValueError):
            self.merge()
        self.assertFalse((self.pool / cache.INDEX_NAME).exists())

    def test_extended_header(self):
        path = self.contributions / "unsafe/contribution.tar"
        path.parent.mkdir(parents=True)
        for kind in (tarfile.XHDTYPE, tarfile.XGLTYPE, tarfile.GNUTYPE_LONGNAME, tarfile.GNUTYPE_LONGLINK):
            with self.subTest(kind=kind):
                member = tarfile.TarInfo("././@ExtendedHeader")
                member.type, member.size = kind, cache.METADATA_LIMIT + 1
                path.write_bytes(member.tobuf(format=tarfile.USTAR_FORMAT) + b"\0" * 1024)
                with self.assertRaisesRegex(ValueError, "extended header"):
                    self.merge()
                self.assertFalse((self.pool / cache.INDEX_NAME).exists())

    def test_validate_all(self):
        self.seed({KEY_A: b"ZIP"})
        before = snapshot(self.pool)
        contribution(self.contributions, "a-valid", {KEY_B: b"new"})
        contribution(self.contributions, "z-invalid", {"../outside": b"bad"})
        with self.assertRaises(ValueError):
            self.merge()
        self.assertEqual(snapshot(self.pool), before)

    def test_pool_link(self):
        outside = write(self.root, "outside", b"secret")
        self.pool.mkdir(parents=True)
        try:
            (self.pool / "downloads").symlink_to(outside.parent, target_is_directory=True)
        except (NotImplementedError, OSError) as error:
            self.skipTest(f"Symbolic-link fixture unavailable: {error}")
        with self.assertRaises(ValueError):
            cache.inventory(self.pool)
        self.assertEqual(outside.read_bytes(), b"secret")


if __name__ == "__main__":
    unittest.main()
