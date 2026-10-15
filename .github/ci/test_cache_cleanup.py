"""Check cache retention decisions and API behavior without network access."""

from contextlib import redirect_stderr, redirect_stdout
from datetime import datetime, timedelta, timezone
import importlib.util
import io
import os
from pathlib import Path
import unittest
from unittest.mock import Mock, call, patch
from urllib.error import HTTPError


spec = importlib.util.spec_from_file_location(
    "cache_cleanup", Path(__file__).with_name("cache_cleanup.py")
)
cleanup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cleanup)

NOW = datetime(2026, 10, 15, 12, tzinfo=timezone.utc)
GIB = 1024 ** 3
COMPILER = "ccache-v1-ubuntu-24.04-X64-linux-gcc-hwloc-Release"
LAYERED = COMPILER.replace("ccache-v1-", "ccache-v2-")


def cache(identifier, key=None, *, ref="refs/heads/develop", version="v1",
          size=100, age=21600, accessed=3600):
    def timestamp(seconds):
        return (NOW - timedelta(seconds=seconds)).isoformat().replace("+00:00", "Z")

    return {
        "id": identifier,
        "key": key or f"{COMPILER}-{identifier}-1",
        "ref": ref,
        "version": version,
        "size_in_bytes": size,
        "created_at": timestamp(age),
        "last_accessed_at": timestamp(accessed),
    }


def plan(caches, **options):
    return cleanup.plan_caches(caches, **{
        "default_branch": "develop",
        "closed_prs": set(),
        "open_prs": set(),
        "active_refs": set(),
        "now": NOW,
        **options,
    })


def removed(result):
    return {row["id"]: row["reason"] for row in result["deletions"]}


class PlanTests(unittest.TestCase):
    def test_empty(self):
        result = plan([])
        self.assertEqual(removed(result), {})
        self.assertEqual(result["before_bytes"], 0)
        self.assertEqual(result["after_bytes"], 0)

    def test_lineages(self):
        for key, expected in (
            (f"{COMPILER}-12345-2", COMPILER),
            (f"{LAYERED}-main-12345-2", f"{LAYERED}-main"),
            ("dependencies-v1-Windows-X64-modules-abcd-12345-2",
             "dependencies-v1-Windows-X64-modules-abcd"),
            ("pip-v1-Windows-X64-cpython-312-abcd-12345-2",
             "pip-v1-Windows-X64-cpython-312-abcd"),
        ):
            with self.subTest(key=key):
                self.assertEqual(cleanup.lineage(key), expected)
        for key in (
            "windows-tools-v1-Windows-X64-baseline-ccache-4.14.0",
            "submodule-git-v1-Linux-X64-12345-2",
            "apt-debs-v1-Linux-X64-12345-2",
            "docs-tools-v1-Linux-X64-12345-2",
            "unrelated-12345-2",
            f"{COMPILER}-12345-retry",
        ):
            with self.subTest(key=key):
                self.assertIsNone(cleanup.lineage(key))

    def test_latest(self):
        older = cache(99, age=18000, accessed=1800)
        newer = cache(1, age=7200, accessed=3600)
        result = plan([older, newer])
        self.assertEqual(removed(result), {99: "superseded snapshot"})
        self.assertEqual(result["after_bytes"], newer["size_in_bytes"])

    def test_tie(self):
        result = plan([cache(1), cache(2)])
        self.assertEqual(removed(result), {1: "superseded snapshot"})

    def test_isolation(self):
        rows = [
            cache(1),
            cache(2, ref="refs/heads/main"),
            cache(3, version="v2"),
            cache(4, key="ccache-v1-ubuntu-24.04-X64-linux-clang-debug-Debug-4-1"),
        ]
        self.assertEqual(removed(plan(rows)), {})

    def test_immutable(self):
        rows = [cache(1, key="apt-debs-v1-Linux-X64-123-1"),
                cache(2, key="apt-debs-v1-Linux-X64-456-1")]
        self.assertEqual(removed(plan(rows)), {})

    def test_closed(self):
        rows = [cache(1, ref="refs/pull/3/merge"),
                cache(2, ref="refs/pull/30/merge"),
                cache(3, ref="refs/heads/pull/3/merge"),
                cache(4), cache(5)]
        result = plan(rows, closed_prs={3}, closed_pr=3)
        self.assertEqual(removed(result), {1: "closed pull request"})
        self.assertEqual(result["before_bytes"], 500)
        self.assertEqual(result["after_bytes"], 400)

    def test_closed_only(self):
        rows = [cache(1, ref="refs/pull/3/merge"),
                cache(2, ref="refs/pull/4/merge", size=10 * GIB)]
        result = plan(rows, closed_prs={3, 4}, closed_pr=3)
        self.assertEqual(removed(result), {1: "closed pull request"})

    def test_unconfirmed(self):
        result = plan([cache(1, ref="refs/pull/3/merge")], closed_pr=3)
        self.assertEqual(removed(result), {})

    def test_open(self):
        self.assertEqual(removed(plan([cache(1, ref="refs/pull/3/merge")])), {})

    def test_budget(self):
        rows = [cache(1, key="other-old", ref="refs/pull/1/merge",
                      size=3 * GIB, accessed=18000),
                cache(2, key="other-middle", ref="refs/pull/2/merge",
                      size=3 * GIB, accessed=10800),
                cache(3, key="other-new", ref="refs/pull/3/merge",
                      size=3 * GIB, accessed=3600)]
        result = plan(rows)
        self.assertEqual(removed(result), {1: "least recently used"})
        self.assertEqual(result["after_bytes"], 6 * GIB)

    def test_baseline(self):
        rows = [cache(1, size=5 * GIB, accessed=18000),
                cache(2, ref="refs/pull/3/merge", size=4 * GIB, accessed=3600)]
        result = plan(rows)
        self.assertEqual(removed(result), {2: "least recently used"})
        self.assertEqual(result["after_bytes"], 5 * GIB)

    def test_large_baseline(self):
        result = plan([cache(1, size=9 * GIB)])
        self.assertEqual(removed(result), {1: "develop baseline"})
        self.assertEqual(result["after_bytes"], 0)

    def test_both_branches(self):
        rows = [cache(1, ref="refs/heads/main", size=3 * GIB, accessed=18000),
                cache(2, size=3 * GIB, accessed=14400),
                cache(3, key="ordinary", ref="refs/heads/feature", size=3 * GIB)]
        result = plan(rows)
        self.assertEqual(removed(result), {3: "least recently used"})
        self.assertEqual(result["after_bytes"], 6 * GIB)

    def test_open_priority(self):
        rows = [cache(1, ref="refs/pull/3/merge", size=5 * GIB, accessed=18000),
                cache(2, key="ordinary", ref="refs/heads/feature", size=4 * GIB)]
        for open_prs, expected in (({3}, {2}), (set(), {1})):
            with self.subTest(open_prs=open_prs):
                self.assertEqual(set(removed(plan(rows, open_prs=open_prs))), expected)

    def test_open_compaction(self):
        rows = [cache(1, ref="refs/pull/3/merge", age=18000),
                cache(2, ref="refs/pull/3/merge", age=7200)]
        result = plan(rows, open_prs={3})
        self.assertEqual(removed(result), {1: "superseded snapshot"})

    def test_open_noncompiler(self):
        rows = [cache(1, key="dependencies-v1-Windows-X64-modules-hash-1-1",
                      ref="refs/pull/3/merge", size=5 * GIB, accessed=18000),
                cache(2, key="ordinary", ref="refs/heads/feature", size=4 * GIB)]
        result = plan(rows, open_prs={3})
        self.assertEqual(removed(result), {1: "least recently used"})

    def test_tiers(self):
        rows = [cache(1, ref="refs/heads/main", size=3 * GIB, accessed=18000),
                cache(2, size=3 * GIB, accessed=14400),
                cache(3, ref="refs/pull/3/merge", size=3 * GIB, accessed=10800),
                cache(4, key="ordinary", ref="refs/heads/feature", size=GIB)]
        for budget, expected in ((9, [4]), (8, [4, 3]), (5, [4, 3, 2]), (2, [4, 3, 2, 1])):
            with self.subTest(budget=budget):
                result = plan(rows, open_prs={3}, budget_bytes=budget * GIB)
                self.assertEqual([row["id"] for row in result["deletions"]], expected)
                self.assertLessEqual(result["after_bytes"], budget * GIB)
        result = plan(rows, open_prs={3}, budget_bytes=2 * GIB)
        self.assertEqual(removed(result), {4: "least recently used", 3: "open pull request baseline",
                                           2: "develop baseline", 1: "main baseline"})

    def test_tool_baseline(self):
        for branch in ("develop", "main"):
            with self.subTest(branch=branch):
                rows = [cache(1, key="docs-tools-v1-Linux-X64-identity", ref=f"refs/heads/{branch}",
                              size=5 * GIB, accessed=18000),
                        cache(2, ref="refs/pull/3/merge", size=4 * GIB)]
                result = plan(rows, default_branch=branch, open_prs={3})
                self.assertEqual(removed(result), {2: "open pull request baseline"})

    def test_layered_compaction(self):
        rows = [cache(1, key=f"{LAYERED}-main-1-1", ref="refs/heads/main", age=18000),
                cache(2, key=f"{LAYERED}-main-2-1", ref="refs/heads/main", age=7200),
                cache(3, key=f"{LAYERED}-develop-3-1")]
        self.assertEqual(removed(plan(rows)), {1: "superseded snapshot"})

    def test_migration(self):
        rows = [cache(1, ref="refs/heads/main", size=5 * GIB),
                cache(2, key=f"{LAYERED}-main-2-1", ref="refs/heads/main",
                      size=4 * GIB, accessed=18000)]
        self.assertEqual(removed(plan(rows)), {1: "least recently used"})
        self.assertEqual(removed(plan(rows, budget_bytes=10 * GIB)), {})

    def test_migration_scope(self):
        rows = [cache(1, ref="refs/heads/main", size=5 * GIB),
                cache(2, key=f"{LAYERED}-develop-2-1", size=4 * GIB, accessed=18000)]
        self.assertEqual(removed(plan(rows)), {2: "develop baseline"})

    def test_migration_profile(self):
        other = "ccache-v2-ubuntu-24.04-X64-linux-clang-debug-Debug"
        rows = [cache(1, ref="refs/heads/main", size=5 * GIB),
                cache(2, key=f"{other}-main-2-1", ref="refs/heads/main",
                      size=4 * GIB, accessed=18000)]
        self.assertEqual(removed(plan(rows)), {2: "main baseline"})

    def test_active(self):
        rows = [cache(1, size=5 * GIB), cache(2, size=4 * GIB)]
        result = plan(rows, active_refs={"refs/heads/develop"})
        self.assertEqual(removed(result), {})
        self.assertEqual(set(result["protected_ids"]), {1, 2})
        self.assertEqual(result["protected_bytes"], 9 * GIB)

    def test_recent(self):
        row = cache(1, ref="refs/pull/3/merge", age=300, accessed=60)
        result = plan([row], closed_prs={3})
        self.assertEqual(removed(result), {})
        self.assertIn(1, result["protected_ids"])

    def test_recent_access(self):
        result = plan([cache(1, ref="refs/pull/3/merge", accessed=60)], closed_prs={3})
        self.assertEqual(removed(result), {})
        self.assertIn(1, result["protected_ids"])

    def test_closed_active(self):
        result = plan([cache(1, ref="refs/pull/3/merge")], closed_prs={3},
                      closed_pr=3, active_refs={"refs/pull/3/merge"})
        self.assertEqual(removed(result), {})

    def test_unresolved(self):
        result = plan([cache(1, ref="refs/pull/3/merge", size=9 * GIB)],
                      closed_prs={3}, unresolved_refs={"refs/pull/3/merge"})
        self.assertEqual(removed(result), {})
        self.assertIn(1, result["protected_ids"])

    def test_copy(self):
        rows = [cache(1), cache(2)]
        original = [dict(row) for row in rows]
        plan(rows)
        self.assertEqual(rows, original)

    def test_invalid(self):
        for update in ({"id": "invalid"}, {"size_in_bytes": -1},
                       {"created_at": "invalid"}, {"last_accessed_at": "invalid"}):
            with self.subTest(update=update), self.assertRaises((ValueError, RuntimeError)):
                plan([{**cache(1), **update}])


class ApiTests(unittest.TestCase):
    def test_dry_run(self):
        api = Mock(spec=cleanup.GitHubApi)
        with redirect_stdout(io.StringIO()):
            result = cleanup.apply_plan(api, plan([cache(1), cache(2)]))
        api.request.assert_not_called()
        self.assertEqual(result["deleted_ids"], [])
        self.assertTrue(result["dry_run"])

    def test_noop(self):
        for rows in ([], [cache(1)]):
            with self.subTest(rows=rows):
                api = Mock(spec=cleanup.GitHubApi)
                with redirect_stdout(io.StringIO()):
                    result = cleanup.apply_plan(api, plan(rows), apply=True)
                api.request.assert_not_called()
                self.assertEqual(result["deleted_ids"], [])

    def test_delete(self):
        api = Mock(spec=cleanup.GitHubApi)
        api.request.return_value = {}
        with redirect_stdout(io.StringIO()):
            result = cleanup.apply_plan(api, plan([cache(1), cache(2)]), apply=True)
        api.request.assert_called_once_with("DELETE", "/actions/caches/1", missing_ok=True)
        self.assertEqual(result["deleted_ids"], [1])
        self.assertEqual(result["missing_ids"], [])
        self.assertFalse(result["dry_run"])

    def test_missing(self):
        api = Mock(spec=cleanup.GitHubApi)
        api.request.return_value = None
        with redirect_stdout(io.StringIO()):
            result = cleanup.apply_plan(api, plan([cache(1), cache(2)]), apply=True)
        self.assertEqual(result["deleted_ids"], [])
        self.assertEqual(result["missing_ids"], [1])

    def test_pages(self):
        api = cleanup.GitHubApi("cppcooper/cheryl-engine", "fixture-token")
        first = [{"id": identifier} for identifier in range(100)]
        last = [{"id": 100}]
        api.request = Mock(side_effect=[{"actions_caches": first}, {"actions_caches": last}])
        self.assertEqual(api.pages("/actions/caches", "actions_caches"), first + last)
        self.assertEqual(api.request.call_args_list, [
            call("GET", "/actions/caches", per_page=100, page=1),
            call("GET", "/actions/caches", per_page=100, page=2),
        ])

    def test_http_missing(self):
        api = cleanup.GitHubApi("cppcooper/cheryl-engine", "fixture-token")
        error = HTTPError("https://api.github.com", 404, "Not Found", {}, io.BytesIO())
        self.addCleanup(error.close)
        api.opener = Mock()
        api.opener.open.side_effect = error
        self.assertIsNone(api.request("DELETE", "/actions/caches/1", missing_ok=True))
        with self.assertRaisesRegex(RuntimeError, "HTTP 404"):
            api.request("DELETE", "/actions/caches/1")


class StateTests(unittest.TestCase):
    def runs(self, *records, status="in_progress"):
        api = Mock(spec=cleanup.GitHubApi)
        api.pages.side_effect = lambda path, field, **query: list(records) if query["status"] == status else []
        return api

    def test_pull_states(self):
        api = Mock(spec=cleanup.GitHubApi)
        api.request.side_effect = [None, {"state": "open"}]
        rows = [cache(1, ref="refs/pull/3/merge"), cache(2, ref="refs/pull/30/merge"),
                cache(3, ref="refs/pull/3/merge")]
        closed, unresolved = cleanup.pull_states(api, rows)
        self.assertEqual(closed, set())
        self.assertEqual(unresolved, {"refs/pull/3/merge"})
        self.assertEqual(api.request.call_args_list, [
            call("GET", "/pulls/3", missing_ok=True),
            call("GET", "/pulls/30", missing_ok=True),
        ])
        result = plan(rows, closed_prs=closed, unresolved_refs=unresolved)
        self.assertEqual(removed(result), {})
        self.assertEqual(set(result["protected_ids"]), {1, 3})

    def test_active_pull(self):
        api = self.runs({"id": 1, "head_branch": "feature", "event": "pull_request",
                         "pull_requests": [{"number": 3, "base": {"ref": "develop"}}]})
        with patch.dict(os.environ, {}, clear=True):
            refs = cleanup.active_refs(api, [])
        self.assertTrue({"refs/heads/feature", "refs/pull/3/merge", "refs/heads/develop"} <= refs)

    def test_empty_pulls(self):
        rows = [cache(1), cache(2, ref="refs/pull/3/merge"), cache(3, ref="refs/heads/main")]
        for metadata in ({}, {"pull_requests": []}):
            with self.subTest(metadata=metadata):
                api = self.runs({"id": 1, "event": "pull_request", "head_branch": "feature", **metadata})
                with patch.dict(os.environ, {}, clear=True):
                    refs = cleanup.active_refs(api, rows)
                self.assertTrue({row["ref"] for row in rows} <= refs)
                self.assertEqual(removed(plan(rows, active_refs=refs)), {})

    def test_active_tag(self):
        api = self.runs({"id": 1, "head_branch": "v4.0.0", "event": "push", "pull_requests": []})
        rows = [cache(1, ref="refs/tags/v4.0.0", size=9 * GIB)]
        with patch.dict(os.environ, {}, clear=True):
            refs = cleanup.active_refs(api, rows)
        self.assertIn("refs/tags/v4.0.0", refs)
        self.assertEqual(removed(plan(rows, active_refs=refs)), {})

    def test_missing_ref(self):
        rows = [cache(1), cache(2), cache(3, ref="refs/pull/3/merge")]
        api = self.runs({"id": 1, "event": "push", "head_branch": None, "pull_requests": []},
                        status="waiting")
        with patch.dict(os.environ, {}, clear=True):
            refs = cleanup.active_refs(api, rows)
        self.assertTrue({row["ref"] for row in rows} <= refs)
        self.assertEqual(removed(plan(rows, active_refs=refs)), {})

    def test_maintenance(self):
        api = self.runs(
            {"id": 1, "head_branch": "develop", "path": ".github/workflows/cache-maintenance.yml@develop"},
            {"id": 2, "head_branch": "main", "name": "Maintain Actions caches"},
            {"id": 3, "head_branch": "current-run"},
            status="queued",
        )
        with patch.dict(os.environ, {"GITHUB_RUN_ID": "3"}, clear=True):
            self.assertEqual(cleanup.active_refs(api, []), set())

    def test_unconfirmed_main(self):
        api = Mock(spec=cleanup.GitHubApi)
        api.request.side_effect = [{"default_branch": "develop"}, None]
        api.pages.return_value = [cache(1, ref="refs/pull/3/merge")]
        with patch.object(cleanup, "GitHubApi", return_value=api), \
             patch.object(cleanup.sys, "argv", ["cache_cleanup.py", "--apply", "--closed-pr", "3"]), \
             patch.dict(os.environ, {}, clear=True), redirect_stdout(io.StringIO()):
            self.assertEqual(cleanup.main(), 0)
        self.assertEqual(api.request.call_args_list, [
            call("GET", ""), call("GET", "/pulls/3", missing_ok=True),
        ])

    def test_open_main(self):
        api = Mock(spec=cleanup.GitHubApi)
        api.request.side_effect = [{"default_branch": "develop"}, {"state": "open"},
                                   {"state": "closed"}, None]
        rows = [cache(number, ref=f"refs/pull/{number}/merge") for number in (3, 4, 5)]
        api.pages.side_effect = lambda path, field, **query: rows if path == "/actions/caches" else []
        decision = {"before_bytes": 300, "after_bytes": 300, "protected_bytes": 100,
                    "deletions": [], "protected_ids": [5]}
        with patch.object(cleanup, "GitHubApi", return_value=api), \
             patch.object(cleanup, "plan_caches", return_value=decision) as select, \
             patch.object(cleanup.sys, "argv", ["cache_cleanup.py"]), \
             patch.dict(os.environ, {}, clear=True), redirect_stdout(io.StringIO()):
            self.assertEqual(cleanup.main(), 0)
        self.assertEqual(select.call_args.kwargs["open_prs"], {3})
        self.assertEqual(select.call_args.kwargs["closed_prs"], {4})
        self.assertEqual(select.call_args.kwargs["unresolved_refs"], {"refs/pull/5/merge"})
        self.assertTrue(all(invocation.args[0] == "GET" for invocation in api.request.call_args_list))

    def test_invalid_pr(self):
        for number in ("0", "-1", "invalid"):
            with self.subTest(number=number), patch.object(cleanup, "GitHubApi") as api, \
                 patch.object(cleanup.sys, "argv", ["cache_cleanup.py", "--closed-pr", number]), \
                 redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as stopped:
                cleanup.main()
            self.assertEqual(stopped.exception.code, 2)
            api.assert_not_called()


if __name__ == "__main__":
    unittest.main()
