"""Check release acceptance and retry behavior without network or compilation."""

from contextlib import redirect_stderr, redirect_stdout
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile


spec = importlib.util.spec_from_file_location("ci_release", Path(__file__).with_name("release.py"))
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)

SHA = "a" * 40
HEAD = "b" * 40
OTHER = "c" * 40
TAG = "v4.0.0"
VERSIONS = {variable: "1.0.0" for variable in release.COMPONENTS.values()}
VERSIONS.update({"VERSION_PROJECT_CHERYL": "4.0.0", "VERSION_PROJECT_ENGINE": "4.0.0"})
VERSIONS.update({"VERSION_PROJECT_MODULE_STARTUP": "4.0.0", "VERSION_PROJECT_MODULE_WORKERS": "4.0.0"})
ENVIRONMENT = {"GITHUB_ACTIONS": "true", "GITHUB_REPOSITORY": release.REPOSITORY,
               "GITHUB_EVENT_NAME": "push", "GITHUB_REF": "refs/heads/main", "GITHUB_SHA": SHA}


class Clock:
    def __init__(self):
        self.value = 0

    def now(self):
        return self.value

    def sleep(self, seconds):
        self.value += seconds


class FakeApi:
    repository = release.REPOSITORY

    def __init__(self):
        self.main = SHA
        self.main_revisions = []
        self.parents = [{"sha": OTHER}, {"sha": HEAD}]
        self.pulls = [{"number": 21, "title": "Release 4.0.0", "state": "closed",
                       "merged_at": "2026-10-15T12:00:00Z", "merge_commit_sha": SHA,
                       "head": {"ref": "develop", "sha": HEAD, "repo": {"full_name": self.repository}},
                       "base": {"ref": "main", "repo": {"full_name": self.repository}}}]
        self.runs = [{"id": 10, "head_sha": SHA, "head_branch": "main", "event": "push",
                      "head_repository": {"full_name": self.repository}, "status": "completed"}]
        self.jobs = {10: [{"name": "Documentation checks", "status": "completed", "conclusion": "success"}]}
        self.tag = None
        self.annotated_tags = {}
        self.release = None
        self.assets = {}
        self.writes = []
        self.queries = []
        self.uploaded = []
        self.fail_upload = None
        self.leave_starter = False
        self.release_on_get = None
        self.asset_on_get = None

    def pages(self, path, field=None, **query):
        self.queries.append((path, field, query))
        if path == f"/commits/{SHA}/pulls":
            return copy.deepcopy(self.pulls)
        if path == "/actions/workflows/documentation.yml/runs":
            return copy.deepcopy(self.runs)
        if path.startswith("/actions/runs/") and path.endswith("/jobs"):
            return copy.deepcopy(self.jobs[int(path.split("/")[3])])
        if path == "/releases":
            return [copy.deepcopy(self.release)] if self.release else []
        if path == "/releases/20/assets":
            return copy.deepcopy(list(self.assets.values()))
        raise AssertionError(f"Unexpected pages request: {path}")

    def request(self, method, path, body=None, **query):
        if method != "GET":
            self.writes.append((method, path, copy.deepcopy(body)))
        if method == "GET" and path == "/git/ref/heads/main":
            revision = self.main_revisions.pop(0) if self.main_revisions else self.main
            return {"ref": "refs/heads/main", "object": {"type": "commit", "sha": revision}}
        if method == "GET" and path == f"/commits/{SHA}":
            return {"sha": SHA, "parents": copy.deepcopy(self.parents)}
        if method == "GET" and path == f"/git/ref/tags/{TAG}":
            return copy.deepcopy(self.tag)
        if method == "GET" and path.startswith("/git/tags/"):
            return copy.deepcopy(self.annotated_tags[path.rsplit("/", 1)[1]])
        if method == "GET" and path == "/releases/20":
            return copy.deepcopy(self.release_on_get if self.release_on_get is not None else self.release)
        if path.startswith("/releases/assets/"):
            identifier = int(path.rsplit("/", 1)[1])
            name, asset = next((name, asset) for name, asset in self.assets.items() if asset["id"] == identifier)
            if method == "GET":
                return copy.deepcopy(self.asset_on_get if self.asset_on_get is not None else asset)
            if method == "DELETE":
                del self.assets[name]
                return None
        if method == "POST" and path == "/git/refs":
            self.tag = {"ref": body["ref"], "object": {"type": "commit", "sha": body["sha"]}}
            return copy.deepcopy(self.tag)
        if method == "POST" and path == "/releases":
            self.release = {"id": 20, **body, "upload_url": "https://uploads.github.com/repos/cppcooper/cheryl-engine/releases/20/assets{?name,label}"}
            return copy.deepcopy(self.release)
        if method == "PATCH" and path == "/releases/20":
            self.release.update(body)
            return copy.deepcopy(self.release)
        raise AssertionError(f"Unexpected API request: {method} {path}")

    def upload(self, upload_url, path):
        if self.fail_upload == len(self.uploaded) + 1:
            self.fail_upload = None
            if self.leave_starter:
                self.assets[path.name] = {"id": len(self.assets) + 1, "name": path.name,
                                          "state": "starter", "size": 0, "digest": None}
            raise RuntimeError("Simulated upload failure")
        data = path.read_bytes()
        asset = {"id": len(self.assets) + 1, "name": path.name, "state": "uploaded", "size": len(data),
                 "digest": "sha256:" + hashlib.sha256(data).hexdigest()}
        self.assets[path.name] = asset
        self.uploaded.append((path.name, data))
        self.writes.append(("UPLOAD", path.name, None))
        return copy.deepcopy(asset)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.candidates = self.directory / "candidates"
        self.candidates.mkdir()
        self.packages = self.directory / "packages"
        self.api = FakeApi()
        self.clock = Clock()
        self.metadata = {}
        for variant, fields in release.VARIANTS.items():
            metadata = dict(zip(("system", "linkage", "configuration", "compiler"), fields))
            metadata.update({"version": "4.0.0", "revision": SHA, "architecture": "AMD64",
                             "submodules": {"extern/example": HEAD},
                             "features": {name: "ON" for name in release.FEATURES},
                             "component_versions": {name: VERSIONS[release.COMPONENTS[name]]
                                                    for name in release.REQUIRED_COMPONENTS}})
            self.metadata[variant] = metadata
            self.bundle(variant)
        self.assemble()

    def snapshot(self, source, revision, destination):
        identity = {"revision": revision, "submodules": {"extern/example": HEAD}}
        with release._tar_writer(destination) as archive:
            release._tar_bytes(archive, release._archive_root(destination) + "/SOURCE-MANIFEST.json", release._json_bytes(identity))
            release._tar_bytes(archive, release._archive_root(destination) + "/tracked.txt", b"pinned source\n")
        return identity

    def assemble(self, *, local_validation=False):
        if self.packages.exists():
            shutil.rmtree(self.packages)
        with patch.object(release, "source_snapshot", side_effect=self.snapshot):
            return release.assemble_release(self.candidates, self.directory, self.packages, SHA, VERSIONS,
                                            local_validation=local_validation)

    def bundle(self, variant, *, inside=None, release_name=False):
        directory = self.candidates / variant
        directory.mkdir(exist_ok=True)
        for path in directory.iterdir():
            path.unlink()
        metadata = self.metadata[variant]
        platform = metadata["system"].lower()
        name = f"cheryl-4.0.0-sdk-{platform}-{metadata['linkage']}-{SHA[:12]}"
        if release_name:
            name = f"cheryl-SDK-v4.0.0-{platform}" + ("-static" if metadata["linkage"] == "static" else "")
        contents = (json.dumps(metadata if inside is None else inside, sort_keys=True) + "\n").encode()
        files = {"SDK-MANIFEST.json": contents, "include/TGUI/Config.hpp": platform.encode(),
                 "lib/cmake/Cheryl/CherylConfig.cmake": b"set(Cheryl_FOUND TRUE)\n",
                 "lib/cmake/Cheryl/CherylConfigVersion.cmake": b'set(PACKAGE_VERSION "4.0.0")\n'}
        if metadata["system"] == "Windows":
            path = directory / (name + ".zip")
            with zipfile.ZipFile(path, "w") as archive:
                for filename, data in files.items():
                    archive.writestr(name + "/" + filename, data)
        else:
            path = directory / (name + ".tar.gz")
            with tarfile.open(path, "w:gz") as archive:
                for filename, data in files.items():
                    member = tarfile.TarInfo(name + "/" + filename)
                    member.size = len(data)
                    archive.addfile(member, io.BytesIO(data))
                archive.addfile(release._tar_member(name + "/lib/libtest.so", link="libtest.so.1"))
                release._tar_bytes(archive, name + "/lib/libtest.so.1", b"native library")
        (directory / "SDK-MANIFEST.json").write_text(json.dumps(metadata), encoding="utf-8")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        (directory / "SHA256SUMS").write_text(f"{digest}  {path.name}\n", encoding="utf-8")
        return path

    def publish(self):
        return release.publish_release(self.api, self.packages, SHA, VERSIONS,
                                       sleep=self.clock.sleep, monotonic=self.clock.now, docs_timeout=1)

    def rejected(self, message):
        with self.assertRaisesRegex(RuntimeError, message):
            self.publish()
        self.assertEqual(self.api.writes, [])

    def candidate_rejected(self, message):
        with self.assertRaisesRegex(RuntimeError, message):
            self.assemble()
        self.assertEqual(self.api.writes, [])

    def update_checksums(self):
        (self.packages / "SHA256SUMS").write_text("".join(
            f"{release._digest(path)}  {path.name}\n" for path in sorted(self.packages.iterdir()) if path.name != "SHA256SUMS"),
            encoding="utf-8")

    def test_publish(self):
        self.assertEqual(self.publish(), "published")
        self.assertEqual(self.api.tag["object"]["sha"], SHA)
        self.assertFalse(self.api.release["draft"])
        self.assertEqual(len(self.api.uploaded), 6)
        self.assertEqual(self.api.writes[-1], ("PATCH", "/releases/20", {"draft": False, "make_latest": "true"}))
        creation = next(body for method, path, body in self.api.writes if path == "/releases")
        self.assertTrue(creation["draft"])
        self.assertEqual(creation["target_commitish"], SHA)
        self.assertTrue(creation["generate_release_notes"])
        uploaded = dict(self.api.uploaded)
        aggregate = json.loads(uploaded["SDK-MANIFESTS.json"])
        self.assertEqual(aggregate["revision"], SHA)
        self.assertEqual(len(aggregate["sdks"]), 4)
        sums = uploaded["SHA256SUMS"].decode().splitlines()
        self.assertEqual(len(sums), 5)
        for line in sums:
            digest, name = line.split("  ")
            self.assertEqual(digest, hashlib.sha256(uploaded[name]).hexdigest())
        docs = next(query for path, field, query in self.api.queries if field == "workflow_runs")
        self.assertEqual(docs, {"branch": "main", "event": "push", "head_sha": SHA})
        self.assertTrue(all(query == {"filter": "latest"} for path, field, query in self.api.queries if field == "jobs"))

    def test_not_release(self):
        self.api.pulls = []
        shutil.rmtree(self.packages)
        self.assertEqual(self.publish(), "not-release")
        self.assertEqual(self.api.writes, [])

    def test_ordinary_title(self):
        self.api.pulls[0]["title"] = "Updates the build"
        self.assertEqual(self.publish(), "not-release")
        self.assertEqual(self.api.writes, [])

    def test_version_title(self):
        self.api.pulls[0]["title"] = "Release 4.1.0"
        self.rejected("PR title")

    def test_fork(self):
        self.api.pulls[0]["head"]["repo"]["full_name"] = "someone/cheryl-engine"
        self.assertEqual(self.publish(), "not-release")
        self.assertEqual(self.api.writes, [])

    def test_wrong_merge(self):
        self.api.pulls[0]["merge_commit_sha"] = OTHER
        self.assertEqual(self.publish(), "not-release")
        self.assertEqual(self.api.writes, [])

    def test_stack_merge(self):
        self.api.parents[1]["sha"] = OTHER
        self.rejected("ordinary merge")

    def test_squash_merge(self):
        self.api.parents = self.api.parents[:1]
        self.rejected("ordinary merge")

    def test_stale(self):
        self.api.main = OTHER
        self.assertEqual(self.publish(), "stale")
        self.assertEqual(self.api.writes, [])

    def test_main_advances(self):
        self.api.main_revisions = [SHA, OTHER]
        self.assertEqual(self.publish(), "stale")
        self.assertEqual(self.api.writes, [])

    def test_missing_variant(self):
        shutil.rmtree(self.candidates / "candidate-sdk-windows-modules")
        self.candidate_rejected("four expected")

    def test_checksum(self):
        path = self.bundle("candidate-sdk-linux-modules")
        path.write_bytes(path.read_bytes() + b"changed")
        self.candidate_rejected("checksum mismatch")

    def test_windows_checksum_lines(self):
        checksum = self.candidates / "candidate-sdk-windows-modules/SHA256SUMS"
        line = checksum.read_bytes().rstrip(b"\n")
        for ending in (b"", b"\n", b"\r\n"):
            with self.subTest(ending=ending):
                checksum.write_bytes(line + ending)
                self.assemble()
        for ending in (b"\r", b"\n\n", b"\r\nextra", b"\nextra"):
            with self.subTest(ending=ending):
                checksum.write_bytes(line + ending)
                self.candidate_rejected("safe archive basename")

    def test_unsafe_name(self):
        (self.candidates / "candidate-sdk-linux-modules/SHA256SUMS").write_text("a" * 64 + "  ../outside.tar.gz\n", encoding="utf-8")
        self.candidate_rejected("safe archive basename")

    def test_revision(self):
        self.metadata["candidate-sdk-linux-modules"]["revision"] = OTHER
        self.bundle("candidate-sdk-linux-modules")
        self.candidate_rejected("SDK revision or version")

    def test_component(self):
        self.metadata["candidate-sdk-linux-modules"]["component_versions"]["opengl"] = "2.0.0"
        self.bundle("candidate-sdk-linux-modules")
        self.candidate_rejected("component version")

    def test_missing_component(self):
        del self.metadata["candidate-sdk-linux-modules"]["component_versions"]["native_glfw"]
        self.bundle("candidate-sdk-linux-modules")
        self.candidate_rejected("missing required component")

    def test_archive_manifest(self):
        inside = copy.deepcopy(self.metadata["candidate-sdk-linux-modules"])
        inside["revision"] = OTHER
        self.bundle("candidate-sdk-linux-modules", inside=inside)
        self.candidate_rejected("outer manifest disagree")

    def test_four_archives(self):
        names = release.release_names("4.0.0")
        self.assertEqual({path.name for path in self.packages.iterdir()}, set(names.values()) | release.SIDECARS)
        root = release._archive_root(self.packages / names["static"])
        with tarfile.open(self.packages / names["static"], "r:gz") as archive:
            self.assertEqual(archive.extractfile(root + "/linux/include/TGUI/Config.hpp").read(), b"linux")
            self.assertEqual(archive.extractfile(root + "/windows/include/TGUI/Config.hpp").read(), b"windows")
            link = archive.getmember(root + "/linux/lib/libtest.so")
            self.assertTrue(link.issym())
            self.assertEqual(link.linkname, "libtest.so.1")
            dispatcher = archive.extractfile(root + "/lib/cmake/Cheryl/CherylConfig.cmake").read().decode()
            self.assertIn('CMAKE_SYSTEM_NAME STREQUAL "Linux"', dispatcher)
            self.assertIn('CMAKE_SYSTEM_NAME STREQUAL "Windows"', dispatcher)
            self.assertIn("../../../${_cheryl_platform}/lib/cmake/Cheryl/CherylConfig.cmake", dispatcher)
        aggregate = json.loads((self.packages / "SDK-MANIFESTS.json").read_bytes())
        self.assertEqual(aggregate["source"]["archive"], names["source"])
        self.assertEqual(aggregate["sdks"]["candidate-sdk-linux-modules"]["prefix"], "linux")
        self.assertEqual(aggregate["sdks"]["candidate-sdk-windows-modules"]["prefix"], "windows")

    def test_deterministic_assembly(self):
        before = {path.name: path.read_bytes() for path in self.packages.iterdir()}
        self.assemble()
        self.assertEqual(before, {path.name: path.read_bytes() for path in self.packages.iterdir()})

    def test_shared_payload_identity(self):
        inputs = [self.bundle(variant, release_name=True) for variant, fields in release.VARIANTS.items()
                  if fields[1] == "shared"]
        self.assemble()
        for path in inputs:
            self.assertEqual(path.read_bytes(), (self.packages / path.name).read_bytes())

    def test_local_configuration(self):
        variant = "candidate-sdk-windows-modules"
        self.metadata[variant]["configuration"] = "Release"
        self.metadata[variant]["features"]["hwloc"] = "OFF"
        self.bundle(variant)
        self.candidate_rejected("accepted CI variant")
        self.assemble(local_validation=True)
        _, aggregate = release.validate_release(self.packages, SHA, VERSIONS, local_validation=True)
        self.assertEqual(aggregate["sdks"][variant]["manifest"], self.metadata[variant])
        self.rejected("Local validation archives cannot be published")

    def test_missing_worker_version(self):
        del self.metadata["candidate-sdk-linux-modules"]["component_versions"]["workers"]
        self.bundle("candidate-sdk-linux-modules")
        self.candidate_rejected("missing required component")

    def test_missing_hwloc(self):
        self.metadata["candidate-sdk-linux-modules"]["features"]["hwloc"] = "OFF"
        self.bundle("candidate-sdk-linux-modules")
        self.candidate_rejected("required release features")

    def test_missing_opengl_startup(self):
        self.metadata["candidate-sdk-linux-modules"]["features"]["startup_opengl"] = "OFF"
        self.bundle("candidate-sdk-linux-modules")
        self.candidate_rejected("required release features")

    def test_source_pins(self):
        self.metadata["candidate-sdk-linux-modules"]["submodules"]["extern/example"] = OTHER
        self.bundle("candidate-sdk-linux-modules")
        self.candidate_rejected("submodule pins differ")

    def test_missing_source_identity(self):
        del self.metadata["candidate-sdk-linux-modules"]["submodules"]
        self.bundle("candidate-sdk-linux-modules")
        self.candidate_rejected("source identity is invalid")

    def test_extra_release_file(self):
        (self.packages / "unexpected.zip").write_bytes(b"extra")
        self.rejected("exactly four named archives")

    def test_release_checksum(self):
        name = release.release_names("4.0.0")["source"]
        path = self.packages / name
        path.write_bytes(path.read_bytes() + b"changed")
        self.rejected("checksum mismatch")

    def test_duplicate_checksum(self):
        path = self.packages / "SHA256SUMS"
        path.write_bytes(path.read_bytes() + path.read_bytes().splitlines(keepends=True)[0])
        self.rejected("invalid or duplicate")

    def test_release_inner_manifest(self):
        path = self.packages / "SDK-MANIFESTS.json"
        aggregate = json.loads(path.read_bytes())
        aggregate["sdks"]["candidate-sdk-linux-modules"]["manifest"]["compiler_version"] = "different"
        path.write_bytes(release._json_bytes(aggregate))
        self.update_checksums()
        self.rejected("outer manifest disagree")

    def test_source_archive_manifest(self):
        path = self.packages / release.release_names("4.0.0")["source"]
        with release._tar_writer(path) as archive:
            release._tar_bytes(archive, release._archive_root(path) + "/SOURCE-MANIFEST.json",
                               release._json_bytes({"revision": OTHER, "submodules": {"extern/example": HEAD}}))
        self.update_checksums()
        self.rejected("source archive and its release identity disagree")

    def test_unsafe_archive(self):
        path = self.bundle("candidate-sdk-linux-modules")
        for filename, link in (("../outside", None), ("lib/escape", "../../../outside"),
                               ("lib/escape", "/outside")):
            with self.subTest(filename=filename, link=link):
                with tarfile.open(path, "w:gz") as archive:
                    archive.addfile(release._tar_member(release._archive_root(path) + "/" + filename, link=link))
                checksum = path.parent / "SHA256SUMS"
                checksum.write_text(f"{release._digest(path)}  {path.name}\n", encoding="utf-8")
                self.candidate_rejected("unsafe|escapes")

    def test_duplicate_archive_member(self):
        path = self.bundle("candidate-sdk-linux-modules")
        with tarfile.open(path, "w:gz") as archive:
            for _ in range(2):
                release._tar_bytes(archive, release._archive_root(path) + "/SDK-MANIFEST.json", b"{}")
        (path.parent / "SHA256SUMS").write_text(f"{release._digest(path)}  {path.name}\n", encoding="utf-8")
        self.candidate_rejected("duplicate paths")

    def test_symlink_ancestor(self):
        path = self.bundle("candidate-sdk-linux-modules")
        root = release._archive_root(path)
        with tarfile.open(path, "w:gz") as archive:
            archive.addfile(release._tar_member(root + "/lib", link="other"))
            release._tar_bytes(archive, root + "/lib/child", b"unsafe")
        (path.parent / "SHA256SUMS").write_text(f"{release._digest(path)}  {path.name}\n", encoding="utf-8")
        self.candidate_rejected("traverses a symlink")

    def test_windows_paths(self):
        path = self.bundle("candidate-sdk-windows-modules")
        root = release._archive_root(path)
        for files in (("include/A.h", "include/a.h"), ("include/NUL.txt",), ("include/path. ",)):
            with self.subTest(files=files):
                with zipfile.ZipFile(path, "w") as archive:
                    for name in files:
                        archive.writestr(root + "/" + name, b"unsafe")
                (path.parent / "SHA256SUMS").write_text(f"{release._digest(path)}  {path.name}\n", encoding="utf-8")
                self.candidate_rejected("unsupported Windows paths")

    def test_no_overwrite(self):
        marker = self.packages / "SDK-MANIFESTS.json"
        before = marker.read_bytes()
        with patch.object(release, "source_snapshot", side_effect=self.snapshot):
            with self.assertRaisesRegex(RuntimeError, "empty real output"):
                release.assemble_release(self.candidates, self.directory, self.packages, SHA, VERSIONS)
        self.assertEqual(marker.read_bytes(), before)

    def test_docs_failure(self):
        self.api.jobs[10][0]["conclusion"] = "failure"
        self.rejected("Documentation checks did not succeed")

    def test_docs_wrong_sha(self):
        self.api.runs[0]["head_sha"] = OTHER
        self.rejected("Timed out")

    def test_docs_latest(self):
        older = copy.deepcopy(self.api.runs[0])
        older["id"] = 9
        self.api.runs.append(older)
        self.api.jobs[9] = copy.deepcopy(self.api.jobs[10])
        self.api.jobs[10][0]["conclusion"] = "failure"
        self.rejected("Documentation checks did not succeed")

    def test_docs_stable_ignored(self):
        self.api.jobs[10].append({"name": "Publish stable documentation ref", "status": "completed", "conclusion": "failure"})
        self.assertEqual(self.publish(), "published")

    def test_tag_conflict(self):
        self.api.tag = {"ref": "refs/tags/" + TAG, "object": {"type": "commit", "sha": OTHER}}
        self.rejected("different commit")

    def test_annotated_tag(self):
        self.api.tag = {"ref": "refs/tags/" + TAG, "object": {"type": "tag", "sha": OTHER}}
        self.api.annotated_tags[OTHER] = {"object": {"type": "commit", "sha": SHA}}
        self.assertEqual(self.publish(), "published")
        self.assertFalse(any(path == "/git/refs" for method, path, body in self.api.writes))

    def test_published_retry(self):
        self.publish()
        self.api.writes.clear()
        self.assertEqual(self.publish(), "unchanged")
        self.assertEqual(self.api.writes, [])

    def test_asset_conflict(self):
        self.publish()
        self.api.writes.clear()
        self.api.release["draft"] = True
        self.api.assets["SDK-MANIFESTS.json"]["digest"] = "sha256:" + "0" * 64
        self.rejected("existing release asset differs")

    def test_published_missing_asset(self):
        self.publish()
        self.api.writes.clear()
        del self.api.assets["SDK-MANIFESTS.json"]
        self.rejected("complete checked SDK set")

    def test_resume_upload(self):
        self.api.fail_upload = 2
        with self.assertRaisesRegex(RuntimeError, "Simulated upload failure"):
            self.publish()
        self.assertTrue(self.api.release["draft"])
        first = self.api.uploaded[0][0]
        self.assertFalse(any(method == "PATCH" for method, path, body in self.api.writes))
        self.assertEqual(self.publish(), "published")
        self.assertEqual(sum(name == first for name, data in self.api.uploaded), 1)
        self.assertEqual(len(self.api.uploaded), 6)

    def failed_starter(self):
        self.api.fail_upload = 2
        self.api.leave_starter = True
        with self.assertRaisesRegex(RuntimeError, "Simulated upload failure"):
            self.publish()
        self.api.writes.clear()
        return next(asset for asset in self.api.assets.values() if asset["state"] == "starter")

    def test_resume_starter(self):
        starter = self.failed_starter()
        first = self.api.uploaded[0][0]
        self.assertEqual(self.publish(), "published")
        self.assertIn(("DELETE", f"/releases/assets/{starter['id']}", None), self.api.writes)
        self.assertEqual(sum(name == first for name, data in self.api.uploaded), 1)
        self.assertEqual(sum(method == "DELETE" for method, path, body in self.api.writes), 1)
        self.assertEqual(len(self.api.uploaded), 6)
        self.assertTrue(all(asset["state"] == "uploaded" for asset in self.api.assets.values()))

    def test_nonempty_starter(self):
        self.failed_starter()["size"] = 1
        self.rejected("existing release asset differs")

    def test_invalid_starter(self):
        self.failed_starter()["id"] = 0
        self.rejected("existing release asset differs")

    def test_published_starter(self):
        self.failed_starter()
        self.api.release["draft"] = False
        self.rejected("existing release asset differs")

    def test_starter_completed(self):
        starter = self.failed_starter()
        self.api.asset_on_get = {**starter, "state": "uploaded", "size": 100}
        self.rejected("unfinished release asset changed")

    def test_draft_published(self):
        self.failed_starter()
        self.api.release_on_get = {**self.api.release, "draft": False}
        self.rejected("no longer the matching stable draft")

    def test_starter_stale(self):
        self.failed_starter()
        self.api.main_revisions = [SHA, SHA, OTHER]
        self.assertEqual(self.publish(), "stale")
        self.assertEqual(self.api.writes, [])

    def test_starter_tag_changed(self):
        self.failed_starter()
        with patch.object(release, "_tag_revision", side_effect=[SHA, OTHER]):
            self.rejected("tag changed before retrying")

    def test_published_prerelease(self):
        self.publish()
        self.api.writes.clear()
        self.api.release["prerelease"] = True
        self.rejected("stable prerelease setting")

    def test_draft_prerelease(self):
        self.failed_starter()
        self.api.release["prerelease"] = True
        self.rejected("stable prerelease setting")

    def test_missing_prerelease(self):
        self.failed_starter()
        del self.api.release["prerelease"]
        self.rejected("stable prerelease setting")

    def test_scope(self):
        self.assertEqual(release.publication_revision(ENVIRONMENT), SHA)
        for key, value in (("GITHUB_ACTIONS", "false"), ("GITHUB_REPOSITORY", "fork/cheryl-engine"),
                           ("GITHUB_REF", "refs/heads/develop"), ("GITHUB_EVENT_NAME", "pull_request"),
                           ("GITHUB_EVENT_NAME", "workflow_dispatch")):
            with self.subTest(key=key, value=value):
                self.assertIsNone(release.publication_revision({**ENVIRONMENT, key: value}))
        with self.assertRaisesRegex(RuntimeError, "differs"):
            release.publication_revision(ENVIRONMENT, OTHER)

    def test_disabled_cli(self):
        output = io.StringIO()
        with patch.dict(release.os.environ, {}, clear=True), redirect_stdout(output):
            self.assertEqual(release.main(["--packages", str(self.packages)]), 0)
        self.assertIn("disabled", output.getvalue())

    def test_local_cli_scope(self):
        with patch.dict(release.os.environ, {}, clear=True), redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                release.main(["--packages", str(self.packages), "--local-validation"])
        self.assertEqual(raised.exception.code, 2)


class SourceSnapshotTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.source = self.directory / "source"
        self.source.mkdir()
        self.git(self.source, "init", "--quiet")

    def git(self, source, *arguments):
        return subprocess.check_output(["git", "-C", str(source), "-c", "user.name=cppcooper",
                                        "-c", "user.email=cppcooper@users.noreply.github.com", *arguments],
                                       stderr=subprocess.PIPE).decode().strip()

    def commit(self, source, filename, contents):
        path = source / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")
        self.git(source, "add", "--", filename)
        self.git(source, "commit", "--quiet", "-m", "Adds fixture")
        return self.git(source, "rev-parse", "HEAD")

    def test_pinned_recursive_objects(self):
        child = self.source / "extern/child"
        nested = child / "nested"
        nested.mkdir(parents=True)
        self.git(child, "init", "--quiet")
        self.git(nested, "init", "--quiet")
        nested_sha = self.commit(nested, "nested.txt", "nested pinned\n")
        self.git(child, "update-index", "--add", "--cacheinfo", "160000," + nested_sha + ",nested")
        child_sha = self.commit(child, "child.txt", "child pinned\n")
        self.git(self.source, "update-index", "--add", "--cacheinfo", "160000," + child_sha + ",extern/child")
        sha = self.commit(self.source, "tracked.txt", "root pinned\n")
        (self.source / "tracked.txt").write_text("uncommitted change\n", encoding="utf-8")
        (child / "child.txt").write_text("generated replacement\n", encoding="utf-8")
        (self.source / "TRANSIENT-HANDOFF.md").write_text("private handoff\n", encoding="utf-8")
        destination = self.directory / "cheryl-src-v4.0.0.tar.gz"
        identity = release.source_snapshot(self.source, sha, destination)
        self.assertEqual(identity, {"revision": sha, "submodules": {"extern/child": child_sha,
                                                                  "extern/child/nested": nested_sha}})
        with tarfile.open(destination, "r:gz") as archive:
            root = release._archive_root(destination)
            self.assertEqual(archive.extractfile(root + "/tracked.txt").read(), b"root pinned\n")
            self.assertEqual(archive.extractfile(root + "/extern/child/child.txt").read(), b"child pinned\n")
            self.assertEqual(archive.extractfile(root + "/extern/child/nested/nested.txt").read(), b"nested pinned\n")
            self.assertNotIn(root + "/TRANSIENT-HANDOFF.md", archive.getnames())
            self.assertFalse(any(".git/" in name for name in archive.getnames()))
        first = destination.read_bytes()
        release.source_snapshot(self.source, sha, destination)
        self.assertEqual(first, destination.read_bytes())

    def test_wrong_revision(self):
        self.commit(self.source, "tracked.txt", "pinned\n")
        with self.assertRaisesRegex(RuntimeError, "pinned revision"):
            release.source_snapshot(self.source, SHA, self.directory / "source.tar.gz")

    def test_wrong_submodule(self):
        child = self.source / "child"
        child.mkdir()
        self.git(child, "init", "--quiet")
        pin = self.commit(child, "tracked.txt", "pinned\n")
        self.git(self.source, "update-index", "--add", "--cacheinfo", "160000," + pin + ",child")
        sha = self.commit(self.source, "tracked.txt", "root\n")
        self.commit(child, "tracked.txt", "different commit\n")
        with self.assertRaisesRegex(RuntimeError, "pinned revision"):
            release.source_snapshot(self.source, sha, self.directory / "source.tar.gz")


if __name__ == "__main__":
    unittest.main()
