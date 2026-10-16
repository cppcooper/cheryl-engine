"""Check release acceptance and retry behavior without network or compilation."""

from contextlib import redirect_stdout
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
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
        self.packages = Path(temporary.name) / "packages"
        self.packages.mkdir()
        self.api = FakeApi()
        self.clock = Clock()
        self.metadata = {}
        for variant, fields in release.VARIANTS.items():
            metadata = dict(zip(("system", "linkage", "configuration", "compiler"), fields))
            metadata.update({"version": "4.0.0", "revision": SHA, "architecture": "AMD64",
                             "component_versions": {name: VERSIONS[release.COMPONENTS[name]]
                                                    for name in release.REQUIRED_COMPONENTS}})
            self.metadata[variant] = metadata
            self.bundle(variant)

    def bundle(self, variant, *, inside=None):
        directory = self.packages / variant
        directory.mkdir(exist_ok=True)
        for path in directory.iterdir():
            path.unlink()
        metadata = self.metadata[variant]
        platform = metadata["system"].lower()
        name = f"cheryl-4.0.0-sdk-{platform}-{metadata['linkage']}-{SHA[:12]}"
        contents = (json.dumps(metadata if inside is None else inside, sort_keys=True) + "\n").encode()
        if metadata["system"] == "Windows":
            path = directory / (name + ".zip")
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr(name + "/SDK-MANIFEST.json", contents)
        else:
            path = directory / (name + ".tar.gz")
            with tarfile.open(path, "w:gz") as archive:
                member = tarfile.TarInfo(name + "/SDK-MANIFEST.json")
                member.size = len(contents)
                archive.addfile(member, io.BytesIO(contents))
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
        shutil.rmtree(self.packages / "candidate-sdk-windows-modules")
        self.rejected("four expected")

    def test_checksum(self):
        path = self.bundle("candidate-sdk-linux-modules")
        path.write_bytes(path.read_bytes() + b"changed")
        self.rejected("checksum mismatch")

    def test_unsafe_name(self):
        (self.packages / "candidate-sdk-linux-modules/SHA256SUMS").write_text("a" * 64 + "  ../outside.tar.gz\n", encoding="utf-8")
        self.rejected("safe archive basename")

    def test_revision(self):
        self.metadata["candidate-sdk-linux-modules"]["revision"] = OTHER
        self.bundle("candidate-sdk-linux-modules")
        self.rejected("SDK revision or version")

    def test_component(self):
        self.metadata["candidate-sdk-linux-modules"]["component_versions"]["opengl"] = "2.0.0"
        self.bundle("candidate-sdk-linux-modules")
        self.rejected("component version")

    def test_missing_component(self):
        del self.metadata["candidate-sdk-linux-modules"]["component_versions"]["native_glfw"]
        self.bundle("candidate-sdk-linux-modules")
        self.rejected("missing required component")

    def test_archive_manifest(self):
        inside = copy.deepcopy(self.metadata["candidate-sdk-linux-modules"])
        inside["revision"] = OTHER
        self.bundle("candidate-sdk-linux-modules", inside=inside)
        self.rejected("outer manifest disagree")

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


if __name__ == "__main__":
    unittest.main()
