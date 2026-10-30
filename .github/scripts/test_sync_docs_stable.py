#!/usr/bin/env python3
"""Check stable publication decisions with every GitHub API call mocked."""

from __future__ import annotations

from contextlib import redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import unittest
from unittest.mock import Mock, patch
from urllib.error import HTTPError, URLError


spec = importlib.util.spec_from_file_location(
    "sync_docs_stable", Path(__file__).with_name("sync-docs-stable.py")
)
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)

CHECKED = "a" * 40
OLDER = "b" * 40
NEWER = "c" * 40
VALID_ENV = {
    "GITHUB_ACTIONS": "true",
    "GITHUB_REPOSITORY": "cppcooper/cheryl-engine",
    "GITHUB_EVENT_NAME": "push",
    "GITHUB_REF": "refs/heads/main",
    "GITHUB_SHA": CHECKED,
    "CHERYL_DOCS_PUBLISH_STABLE": "true",
    "CHERYL_DOCS_CHECKS_RESULT": "success",
    "GITHUB_TOKEN": "test-token",
}


def ref(branch, revision):
    return {"ref": f"refs/heads/{branch}", "object": {"type": "commit", "sha": revision}}


def ahead():
    return {"status": "ahead", "merge_base_commit": {"sha": OLDER}}


class StableTests(unittest.TestCase):
    def setUp(self):
        network = patch.object(sync, "urlopen")
        self.urlopen = network.start()
        self.addCleanup(network.stop)

    def client(self, responses):
        client = Mock(spec=sync.GitHubClient)
        client.request.side_effect = responses
        return client

    def writes(self, client):
        return [call for call in client.request.call_args_list if call.args[0] != "GET"]

    def http_error(self, code):
        response = io.BytesIO(b"fixture response")
        error = HTTPError("https://api.github.com", code, "fixture error", {}, response)
        self.addCleanup(error.close)
        return error, response

    def test_gates(self):
        overrides = (
            {"GITHUB_ACTIONS": "false"},
            {"GITHUB_REPOSITORY": "someone/cheryl-engine"},
            {"GITHUB_EVENT_NAME": "pull_request"},
            {"GITHUB_EVENT_NAME": "pull_request_target"},
            {"GITHUB_EVENT_NAME": "workflow_dispatch"},
            {"GITHUB_REF": "refs/heads/develop"},
            {"GITHUB_REF": "refs/tags/v1.0.0"},
            {"CHERYL_DOCS_PUBLISH_STABLE": ""},
            {"CHERYL_DOCS_PUBLISH_STABLE": "false"},
            {"CHERYL_DOCS_PUBLISH_STABLE": "TRUE"},
            {"CHERYL_DOCS_CHECKS_RESULT": "failure"},
            {"CHERYL_DOCS_CHECKS_RESULT": "cancelled"},
            {"CHERYL_DOCS_CHECKS_RESULT": "skipped"},
        )
        environments = [{**VALID_ENV, **change} for change in overrides]
        environments.extend(
            {name: value for name, value in VALID_ENV.items() if name != omitted}
            for omitted in (
                "GITHUB_ACTIONS", "GITHUB_REPOSITORY", "GITHUB_EVENT_NAME",
                "GITHUB_REF", "CHERYL_DOCS_PUBLISH_STABLE", "CHERYL_DOCS_CHECKS_RESULT",
            )
        )
        for environment in environments:
            with self.subTest(environment=environment), patch.dict(os.environ, environment, clear=True):
                with patch.object(sync, "GitHubClient") as factory, redirect_stdout(io.StringIO()):
                    self.assertEqual(sync.main(), 0)
                    factory.assert_not_called()
        self.urlopen.assert_not_called()

    def test_bad_revision(self):
        for revision in (None, "", "short", "A" * 40, CHECKED + "0"):
            with self.subTest(revision=revision), self.assertRaises(RuntimeError):
                sync.publication_revision({**VALID_ENV, "GITHUB_SHA": revision})
        self.urlopen.assert_not_called()

    def test_missing_token(self):
        environment = {name: value for name, value in VALID_ENV.items() if name != "GITHUB_TOKEN"}
        with patch.dict(os.environ, environment, clear=True), self.assertRaises(RuntimeError):
            sync.main()
        self.urlopen.assert_not_called()

    def test_stale_main(self):
        client = self.client([ref("main", NEWER)])
        self.assertEqual(sync.mirror_stable(client, CHECKED), "stale")
        client.request.assert_called_once_with("GET", "git/ref/heads/main")

    def test_stale_after_plan(self):
        for preparation in ([None], [ref("stable", OLDER), ahead()]):
            with self.subTest(preparation=preparation):
                client = self.client([ref("main", CHECKED), *preparation, ref("main", NEWER)])
                self.assertEqual(sync.mirror_stable(client, CHECKED), "stale")
                self.assertEqual(self.writes(client), [])

    def test_same_stable(self):
        client = self.client([ref("main", CHECKED), ref("stable", CHECKED)])
        self.assertEqual(sync.mirror_stable(client, CHECKED), "unchanged")
        self.assertEqual(self.writes(client), [])
        self.assertEqual(client.request.call_count, 2)

    def test_create(self):
        client = self.client([ref("main", CHECKED), None, ref("main", CHECKED), ref("stable", CHECKED)])
        self.assertEqual(sync.mirror_stable(client, CHECKED), "created")
        self.assertEqual(len(self.writes(client)), 1)
        client.request.assert_called_with("POST", "git/refs", {"ref": "refs/heads/stable", "sha": CHECKED})

    def test_fast_forward(self):
        client = self.client([
            ref("main", CHECKED), ref("stable", OLDER), ahead(),
            ref("main", CHECKED), ref("stable", CHECKED),
        ])
        self.assertEqual(sync.mirror_stable(client, CHECKED), "updated")
        self.assertEqual(len(self.writes(client)), 1)
        client.request.assert_called_with("PATCH", "git/refs/heads/stable", {"sha": CHECKED, "force": False})

    def test_non_ff(self):
        for status in ("behind", "diverged", "identical", "unknown"):
            with self.subTest(status=status):
                comparison = {"status": status, "merge_base_commit": {"sha": OLDER}}
                client = self.client([ref("main", CHECKED), ref("stable", OLDER), comparison])
                with self.assertRaises(RuntimeError):
                    sync.mirror_stable(client, CHECKED)
                self.assertEqual(self.writes(client), [])

    def test_malformed_refs(self):
        invalid = (
            None, [], {}, ref("wrong", CHECKED),
            {"ref": "refs/heads/main", "object": {"type": "tag", "sha": CHECKED}},
            ref("main", "short"),
        )
        for response in invalid:
            with self.subTest(response=response):
                client = self.client([response])
                with self.assertRaises(RuntimeError):
                    sync.mirror_stable(client, CHECKED)
                self.assertEqual(self.writes(client), [])
        for response in ({}, ref("main", CHECKED), ref("stable", "short")):
            with self.subTest(stable=response):
                client = self.client([ref("main", CHECKED), response])
                with self.assertRaises(RuntimeError):
                    sync.mirror_stable(client, CHECKED)
                self.assertEqual(self.writes(client), [])

    def test_malformed_compare(self):
        for comparison in (None, [], {}, {"status": "ahead"}, {"status": "ahead", "merge_base_commit": {"sha": NEWER}}):
            with self.subTest(comparison=comparison):
                client = self.client([ref("main", CHECKED), ref("stable", OLDER), comparison])
                with self.assertRaises(RuntimeError):
                    sync.mirror_stable(client, CHECKED)
                self.assertEqual(self.writes(client), [])

    def test_read_failure(self):
        preparations = (
            [], [ref("main", CHECKED)],
            [ref("main", CHECKED), ref("stable", OLDER)],
            [ref("main", CHECKED), ref("stable", OLDER), ahead()],
        )
        for preparation in preparations:
            with self.subTest(preparation=preparation):
                client = self.client([*preparation, RuntimeError("API failed")])
                with self.assertRaises(RuntimeError):
                    sync.mirror_stable(client, CHECKED)
                self.assertEqual(self.writes(client), [])
                self.assertEqual(client.request.call_count, len(preparation) + 1)

    def test_write_rejection(self):
        for preparation in ([None], [ref("stable", OLDER), ahead()]):
            with self.subTest(preparation=preparation):
                client = self.client([
                    ref("main", CHECKED), *preparation, ref("main", CHECKED),
                    RuntimeError("GitHub rejected the write"),
                ])
                with self.assertRaises(RuntimeError):
                    sync.mirror_stable(client, CHECKED)
                self.assertEqual(len(self.writes(client)), 1)
                self.assertEqual(client.request.call_count, len(preparation) + 3)

    def test_unconfirmed_write(self):
        client = self.client([ref("main", CHECKED), None, ref("main", CHECKED), ref("stable", NEWER)])
        with self.assertRaises(RuntimeError):
            sync.mirror_stable(client, CHECKED)
        self.assertEqual(len(self.writes(client)), 1)
        self.assertEqual(client.request.call_count, 4)

    def test_optional_404(self):
        client = sync.GitHubClient("test-token")
        error, response = self.http_error(404)
        self.urlopen.side_effect = error
        self.assertIsNone(client.request("GET", "git/ref/heads/stable", missing_ok=True))
        self.assertEqual(self.urlopen.call_count, 1)
        self.assertTrue(response.closed)
        for method, missing_ok, code in (("GET", False, 404), ("POST", True, 404), ("GET", True, 403)):
            with self.subTest(method=method, missing_ok=missing_ok, code=code):
                self.urlopen.reset_mock()
                error, response = self.http_error(code)
                self.urlopen.side_effect = error
                with self.assertRaises(RuntimeError):
                    client.request(method, "git/refs", missing_ok=missing_ok)
                self.assertEqual(self.urlopen.call_count, 1)
                self.assertTrue(response.closed)

    def test_transport_failure(self):
        client = sync.GitHubClient("test-token")
        for error in (URLError("offline"), TimeoutError("timed out")):
            with self.subTest(error=error):
                self.urlopen.reset_mock()
                self.urlopen.side_effect = error
                with self.assertRaises(RuntimeError):
                    client.request("GET", "git/ref/heads/main")
                self.assertEqual(self.urlopen.call_count, 1)

    def test_bad_api_json(self):
        client = sync.GitHubClient("test-token")
        for payload in (b"{", b"null", b"[]", b'"unexpected"'):
            with self.subTest(payload=payload):
                self.urlopen.reset_mock()
                self.urlopen.return_value.__enter__.return_value = io.BytesIO(payload)
                with self.assertRaises(RuntimeError):
                    client.request("GET", "git/ref/heads/main")
                self.assertEqual(self.urlopen.call_count, 1)

    def test_api_write(self):
        response = json.dumps(ref("stable", CHECKED)).encode("utf-8")
        self.urlopen.return_value.__enter__.return_value = io.BytesIO(response)
        client = sync.GitHubClient("test-token")
        client.request("PATCH", "git/refs/heads/stable", {"sha": CHECKED, "force": False})
        self.urlopen.assert_called_once()
        request = self.urlopen.call_args.args[0]
        self.assertEqual(request.full_url, "https://api.github.com/repos/cppcooper/cheryl-engine/git/refs/heads/stable")
        self.assertEqual(request.get_method(), "PATCH")
        self.assertEqual(json.loads(request.data), {"sha": CHECKED, "force": False})


if __name__ == "__main__":
    unittest.main()
