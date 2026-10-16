"""Check shared dependency publication provenance and snapshot retirement."""

import copy
import importlib.util
from pathlib import Path
import unittest


spec = importlib.util.spec_from_file_location("vcpkg_publication", Path(__file__).with_name("vcpkg_publication.py"))
publication = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publication)

REPOSITORY = publication.REPOSITORY
SHA = "a" * 40
EVENT = {"repository": {"full_name": REPOSITORY, "default_branch": "main"},
         "workflow_run": {"id": 10, "run_attempt": 2, "head_sha": SHA}}
ENVIRONMENT = {"GITHUB_REPOSITORY": REPOSITORY, "GITHUB_EVENT_NAME": "workflow_run",
               "GITHUB_REF": "refs/heads/main", "GITHUB_RUN_ID": "100", "GITHUB_RUN_ATTEMPT": "1"}


def artifact(identifier=1, name="windows-modules", attempt=2):
    return {"id": identifier, "name": f"vcpkg-cache-X64-{name}-{attempt}",
            "expired": False, "size_in_bytes": 100}


def job(name="windows-modules", conclusion="success"):
    return {"name": name, "conclusion": "failure",
            "steps": [{"name": "Prepare dependencies", "conclusion": conclusion},
                      {"name": "Build and test", "conclusion": "failure"}]}


def cache(identifier, run, *, version="archive-v1", ref="refs/heads/main", created="2026-10-16T12:00:00Z"):
    return {"id": identifier, "key": f"{publication.PREFIX}{run}-1", "ref": ref,
            "version": version, "created_at": created}


class FakeApi:
    repository = REPOSITORY

    def __init__(self):
        self.run = {"id": 10, "run_attempt": 2, "workflow_id": 20, "path": ".github/workflows/ci.yml",
                    "head_repository": {"full_name": REPOSITORY}, "head_sha": SHA, "status": "completed",
                    "event": "pull_request", "pull_requests": [{"number": 21}]}
        self.workflow = {"id": 20}
        self.pull = {"head": {"repo": {"full_name": REPOSITORY}}}
        self.artifacts = [artifact()]
        self.jobs = {1: [job()], 2: [job()]}
        self.caches = [cache(1, 90), cache(2, 100)]
        self.requests = []
        self.deleted = []

    def request(self, method, path, body=None, **query):
        self.requests.append((method, path, query))
        if method == "GET" and path == "/actions/runs/10/attempts/2":
            return copy.deepcopy(self.run)
        if method == "GET" and path == "/actions/workflows/ci.yml":
            return copy.deepcopy(self.workflow)
        if method == "GET" and path == "/pulls/21":
            return copy.deepcopy(self.pull)
        if method == "DELETE" and path.startswith("/actions/caches/"):
            self.deleted.append(int(path.rsplit("/", 1)[1]))
            return None
        raise AssertionError(f"Unexpected API request: {method} {path}")

    def pages(self, path, field=None, **query):
        self.requests.append(("PAGES", path, query))
        if path == "/actions/runs/10/artifacts" and field == "artifacts":
            return copy.deepcopy(self.artifacts)
        if path in {"/actions/runs/10/attempts/1/jobs", "/actions/runs/10/attempts/2/jobs"} and field == "jobs":
            return copy.deepcopy(self.jobs[int(path.split("/")[-2])])
        if path == "/actions/caches" and field == "actions_caches":
            self.assert_cache_query(query)
            return copy.deepcopy(self.caches)
        raise AssertionError(f"Unexpected pages request: {path}")

    def assert_cache_query(self, query):
        if query != {"ref": "refs/heads/main", "key": publication.PREFIX}:
            raise AssertionError(f"Unexpected cache query: {query}")


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.api = FakeApi()
        self.event = copy.deepcopy(EVENT)
        self.environment = dict(ENVIRONMENT)

    def admit(self):
        return publication.admit(self.api, self.event, self.environment)

    def retire(self):
        return publication.retire(self.api, self.event, self.environment)

    def test_prepared(self):
        result = self.admit()
        self.assertEqual(result["allowed"], "true")
        self.assertEqual(result["artifact-ids"], "1")
        self.assertEqual(result["source-run-id"], "10")
        self.assertEqual(self.api.deleted, [])

    def test_push(self):
        for event in ("push", "workflow_dispatch"):
            with self.subTest(event=event):
                self.api.run["event"] = event
                self.assertEqual(self.admit()["allowed"], "true")

    def test_default_branch(self):
        for name, value in (("GITHUB_REF", "refs/heads/develop"), ("GITHUB_EVENT_NAME", "pull_request"),
                            ("GITHUB_REPOSITORY", "other/repo")):
            with self.subTest(name=name):
                self.environment = {**ENVIRONMENT, name: value}
                self.assertEqual(self.admit()["allowed"], "false")
                self.assertEqual(self.api.requests, [])

    def test_repository(self):
        self.event["repository"]["full_name"] = "other/repo"
        self.assertEqual(self.admit()["allowed"], "false")
        self.assertEqual(self.api.requests, [])

    def test_forks(self):
        self.api.run["head_repository"]["full_name"] = "fork/cheryl-engine"
        self.assertEqual(self.admit()["allowed"], "false")
        self.api.run["head_repository"]["full_name"] = REPOSITORY
        self.api.pull["head"]["repo"]["full_name"] = "fork/cheryl-engine"
        self.assertEqual(self.admit()["allowed"], "false")

    def test_missing_pr(self):
        self.api.run["pull_requests"] = []
        self.assertEqual(self.admit()["allowed"], "false")

    def test_other_event(self):
        self.api.run["event"] = "pull_request_target"
        self.assertEqual(self.admit()["allowed"], "false")

    def test_run_identity(self):
        for field, value in (("id", 11), ("run_attempt", 1), ("workflow_id", 21), ("workflow_id", None),
                             ("head_sha", "b" * 40), ("head_sha", None), ("status", "in_progress"),
                             ("path", ".github/workflows/other.yml")):
            with self.subTest(field=field, value=value):
                saved = self.api.run[field]
                self.api.run[field] = value
                with self.assertRaises(ValueError):
                    self.admit()
                self.api.run[field] = saved

    def test_missing_workflow(self):
        self.api.workflow.clear()
        with self.assertRaises(ValueError):
            self.admit()

    def test_trigger_id(self):
        for value in (None, True, 0, "10"):
            with self.subTest(value=value):
                self.event["workflow_run"]["id"] = value
                with self.assertRaises(ValueError):
                    self.admit()
        self.assertEqual(self.api.requests, [])

    def test_reruns(self):
        self.api.artifacts = [artifact(3, attempt=2), artifact(1, attempt=1)]
        result = self.admit()
        self.assertEqual(result["artifact-ids"], "3")
        paths = {path for _, path, _ in self.api.requests}
        self.assertIn("/actions/runs/10/attempts/1/jobs", paths)
        self.assertIn("/actions/runs/10/attempts/2/jobs", paths)

    def test_rerun_fallback(self):
        self.api.artifacts = [artifact(3, attempt=2), artifact(1, attempt=1)]
        self.api.jobs[2] = [job(conclusion="failure")]
        self.assertEqual(self.admit()["artifact-ids"], "1")

    def test_total_download(self):
        self.api.artifacts = [{**artifact(n, name=f"windows-{n}"), "size_in_bytes": publication.ARTIFACT_LIMIT}
                              for n in range(1, 5)]
        self.api.jobs[2] = [job(f"windows-{n}") for n in range(1, 5)]
        with self.assertRaisesRegex(ValueError, "total download"):
            self.admit()

    def test_unsupported_artifacts(self):
        for replacement in ({"expired": True}, {"expired": None}, {"name": "vcpkg-cache-X64-windows-modules-3"},
                            {"name": "vcpkg-cache-ARM64-windows-modules-2"}, {"name": "other-artifact"}):
            with self.subTest(replacement=replacement):
                self.api.artifacts = [{**artifact(), **replacement}]
                self.assertEqual(self.admit()["allowed"], "false")

    def test_artifact_limits(self):
        for replacement in ({"size_in_bytes": 0}, {"size_in_bytes": publication.ARTIFACT_LIMIT + 1},
                            {"size_in_bytes": True}, {"id": -1}):
            with self.subTest(replacement=replacement):
                self.api.artifacts = [{**artifact(), **replacement}]
                with self.assertRaises(ValueError):
                    self.admit()

    def test_duplicate_artifacts(self):
        self.api.artifacts *= 2
        with self.assertRaisesRegex(ValueError, "unique"):
            self.admit()

    def test_many_artifacts(self):
        self.api.artifacts = [artifact(n) for n in range(1, publication.CONTRIBUTION_LIMIT + 2)]
        with self.assertRaisesRegex(ValueError, "Too many"):
            self.admit()

    def test_preparation_failed(self):
        for conclusion in ("failure", "cancelled", "skipped", None):
            with self.subTest(conclusion=conclusion):
                self.api.jobs[2] = [job(conclusion=conclusion)]
                self.assertEqual(self.admit()["allowed"], "false")

    def test_wrong_job(self):
        self.api.jobs[2] = [job("other-job")]
        self.assertEqual(self.admit()["allowed"], "false")

    def test_ambiguous_job(self):
        self.api.jobs[2] *= 2
        self.assertEqual(self.admit()["allowed"], "false")

    def test_retire(self):
        self.assertEqual(self.retire(), {"removed": "1"})
        self.assertEqual(self.api.deleted, [1])

    def test_save_first(self):
        self.api.caches.pop()
        with self.assertRaisesRegex(ValueError, "saved before"):
            self.retire()
        self.assertEqual(self.api.deleted, [])

    def test_retire_scopes(self):
        self.api.caches.extend([cache(3, 95, ref="refs/heads/develop"), cache(4, 95, version="other-path"),
                                cache(5, 101, created="2026-10-16T13:00:00Z"),
                                {**cache(6, 95), "key": "other-cache-95-1"}])
        self.retire()
        self.assertEqual(self.api.deleted, [1])

    def test_retire_branch(self):
        self.environment["GITHUB_REF"] = "refs/heads/develop"
        with self.assertRaises(ValueError):
            self.retire()
        self.assertEqual(self.api.requests, [])

    def test_bad_snapshot(self):
        for value in ("", "0", "-1", "1\nextra=bad"):
            with self.subTest(value=value):
                self.environment["GITHUB_RUN_ID"] = value
                with self.assertRaises(ValueError):
                    self.retire()
        self.assertEqual(self.api.deleted, [])

    def test_retire_metadata(self):
        for replacement in ({"version": ""}, {"created_at": None}, {"created_at": "2026-10-16"}, {"id": 0}):
            with self.subTest(replacement=replacement):
                self.api.caches = [cache(1, 90), {**cache(2, 100), **replacement}]
                with self.assertRaises(ValueError):
                    self.retire()
                self.assertEqual(self.api.deleted, [])

    def test_validate_before_delete(self):
        self.api.caches.insert(1, {**cache(3, 95), "created_at": "bad"})
        with self.assertRaises(ValueError):
            self.retire()
        self.assertEqual(self.api.deleted, [])


if __name__ == "__main__":
    unittest.main()
