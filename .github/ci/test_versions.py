"""Check version policies and reviewed PR changes using fake Git and API state."""

import base64
from contextlib import redirect_stdout
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location("versions", Path(__file__).with_name("versions.py"))
versions = importlib.util.module_from_spec(spec)
spec.loader.exec_module(versions)

A, B, C, MAIN, STABLE, BOT, GENERATED = (character * 40 for character in "1234abe")
TEXT = """# Independent component versions.
set(VERSION_PROJECT_CHERYL 4.0.0)
set(VERSION_PROJECT_ENGINE 4.0.0)
set(VERSION_PROJECT_MODULE_NATIVE_GLFW 1.0.0)
set(VERSION_PROJECT_MODULE_DEBUG_TERMINAL_LINUX 1.0.0)
set(VERSION_PROJECT_MODULE_OPENGL 1.0.0)
set(VERSION_PROJECT_MODULE_UI_TGUI 1.0.0)
set(VERSION_PROJECT_MODULE_UI_RMLUI 1.0.0)
set(VERSION_PROJECT_MODULE_AUDIO_MINIAUDIO 1.0.0)
set(VERSION_PROJECT_APP_DEMO 1.0.0)
set(UNRELATED value)
"""


def state(source=A, values=None, kind="maintenance"):
    return {"format": 1, "kind": kind, "source_revision": source,
            "versions": values or versions.read_versions(TEXT), "stable_tag": "v4.0.0", "stable_revision": STABLE}


class FakeGit:
    def __init__(self):
        self.refs = {"HEAD": B, "refs/remotes/origin/develop": B,
                     "refs/remotes/origin/main": STABLE, "refs/tags/v4.0.0": STABLE}
        self.parents_by_revision = {A: [MAIN], B: [A], STABLE: [MAIN, A], MAIN: []}
        self.files = {revision: {versions.VERSION_FILE: TEXT, "source.cpp": revision} for revision in (A, B, MAIN, STABLE)}
        self.paths = {B: {"projects/engine/src/example.cpp"}}

    def resolve(self, ref):
        return self.refs[ref]

    def text(self, revision, path, *, optional=False):
        if optional:
            return self.files[revision].get(path)
        return self.files[revision][path]

    def parents(self, revision):
        return self.parents_by_revision[revision]

    def ancestor(self, before, after):
        pending, seen = [after], set()
        while pending:
            current = pending.pop()
            if current == before:
                return True
            if current not in seen:
                seen.add(current)
                pending.extend(self.parents_by_revision.get(current, []))
        return False

    def integrations(self, before, after):
        commits = []
        while after != before:
            commits.append(after)
            after = self.parents(after)[0]
        return commits[::-1]

    def changed(self, before, after):
        return self.paths[after]


class FakeApi:
    def __init__(self, git):
        self.git = git
        self.releases = [{"tag_name": "v4.0.0", "draft": False, "prerelease": False, "published_at": "2026-10-20"}]
        self.refs, self.pulls, self.calls, self.commits, self.tree_files, self.commit_trees = {}, [], [], [], {}, {}

    def pages(self, path, field=None, **query):
        self.calls.append(("GET", path, None, query))
        if path == "/releases":
            return self.releases
        if path == "/pulls":
            return [pull for pull in self.pulls if pull["base"]["ref"] == query["base"]
                    and pull["head"]["ref"] == query["head"].split(":", 1)[1]]
        raise AssertionError(path)

    def request(self, method, path, body=None, **query):
        self.calls.append((method, path, body, query))
        if method == "GET" and path.startswith("/git/ref/heads/"):
            revision = self.refs.get(path.removeprefix("/git/ref/heads/"))
            return {"object": {"sha": revision}} if revision else None
        if method == "GET" and path.startswith("/contents/"):
            text = self.git.files[query["ref"]].get(path.removeprefix("/contents/"))
            if text is None and query.get("missing_ok"):
                return None
            return {"encoding": "base64", "content": base64.b64encode(text.encode()).decode()}
        if method == "GET" and path.startswith("/git/commits/"):
            revision = path.rsplit("/", 1)[1]
            return {"tree": {"sha": self.commit_trees.get(revision, revision)}}
        if method == "GET" and path.startswith("/git/trees/"):
            revision = path.rsplit("/", 1)[1]
            files = self.tree_files.get(revision, self.git.files.get(revision))
            return {"truncated": False, "tree": [
                {"path": name, "mode": "100644", "type": "blob", "sha": hashlib.sha1(text.encode()).hexdigest()}
                for name, text in files.items()]}
        if method == "POST" and path == "/git/trees":
            files = dict(self.git.files[body["base_tree"]])
            files.update({entry["path"]: entry["content"] for entry in body["tree"]})
            self.tree_files[C] = files
            return {"sha": C}
        if method == "POST" and path == "/git/commits":
            self.commits.append(body)
            self.commit_trees[GENERATED] = body["tree"]
            self.git.files[GENERATED] = self.tree_files[body["tree"]]
            return {"sha": GENERATED}
        if method == "POST" and path == "/git/refs":
            self.refs[body["ref"].removeprefix("refs/heads/")] = body["sha"]
            return body
        if method == "PATCH" and path.startswith("/git/refs/heads/"):
            self.refs[path.removeprefix("/git/refs/heads/")] = body["sha"]
            return body
        if method == "POST" and path == "/pulls":
            pull = {**body, "number": len(self.pulls) + 1,
                    "head": {"ref": body["head"], "repo": {"full_name": versions.REPOSITORY}},
                    "base": {"ref": body["base"]}}
            self.pulls.append(pull)
            return pull
        if method == "PATCH" and path.startswith("/pulls/"):
            pull = next(pull for pull in self.pulls if pull["number"] == int(path.rsplit("/", 1)[1]))
            pull.update(body)
            return pull
        raise AssertionError((method, path))


class PolicyTests(unittest.TestCase):
    def test_versions(self):
        text = TEXT.replace("4.0.0)", '"4.0.0") # preserve comment', 1).replace("\n", "\r\n")
        values = versions.read_versions(text)
        self.assertEqual(values[versions.ENGINE], "4.0.0")
        changed = versions.replace_versions(text, {versions.PROJECT: "4.1.0"})
        self.assertEqual(changed, text.replace('"4.0.0"', '"4.1.0"', 1))
        self.assertEqual(versions.read_versions(changed)[versions.ENGINE], "4.0.0")

    def test_invalid(self):
        for value in ("1.0", "1.0.0-alpha", "01.0.0", "-1.0.0", None):
            with self.subTest(value=value), self.assertRaises(ValueError):
                versions.version_tuple(value)
        with self.assertRaises(ValueError):
            versions.read_versions(TEXT + "set(VERSION_PROJECT_ENGINE 5.0.0)\n")
        with self.assertRaises(ValueError):
            versions.replace_versions(TEXT, {"VERSION_PROJECT_UNKNOWN": "1.0.0"})

    def test_aliases(self):
        text = ("set(VERSION_PROJECT_ENGINE 5.0.0)\n"
                "set(VERSION_PROJECT_MODULE_STARTUP ${VERSION_PROJECT_ENGINE})\n"
                'set(VERSION_PROJECT_MODULE_WORKERS "${VERSION_PROJECT_MODULE_STARTUP}") # preserve alias\n')
        values = versions.read_versions(text)
        self.assertEqual(values, {"VERSION_PROJECT_ENGINE": "5.0.0", "VERSION_PROJECT_MODULE_STARTUP": "5.0.0",
                                  "VERSION_PROJECT_MODULE_WORKERS": "5.0.0"})
        changed = versions.replace_versions(text, {versions.ENGINE: "5.0.1"})
        self.assertEqual(changed, text.replace("5.0.0", "5.0.1"))
        self.assertTrue(all(value == "5.0.1" for value in versions.read_versions(changed).values()))

    def test_invalid_aliases(self):
        for value in ("${VERSION_PROJECT_UNKNOWN}", "${VERSION_PROJECT_MODULE_STARTUP}", "${ENV}",
                      "$ENV{VERSION_PROJECT_ENGINE}", "${VERSION_PROJECT_ENGINE}.0", "${${VERSION_PROJECT_ENGINE}}"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                versions.read_versions("set(VERSION_PROJECT_ENGINE 5.0.0)\n"
                                       f"set(VERSION_PROJECT_MODULE_STARTUP {value})\n")
        with self.assertRaisesRegex(ValueError, "previously declared"):
            versions.read_versions("set(VERSION_PROJECT_MODULE_STARTUP ${VERSION_PROJECT_ENGINE})\n"
                                   "set(VERSION_PROJECT_ENGINE 5.0.0)\n")

    def test_repository_versions(self):
        text = (versions.ROOT / versions.VERSION_FILE).read_text(encoding="utf-8")
        values = versions.read_versions(text)
        self.assertIn(versions.PROJECT, values)
        self.assertIn(versions.ENGINE, values)
        self.assertEqual(values["VERSION_PROJECT_MODULE_STARTUP"], values[versions.ENGINE])
        self.assertEqual(values["VERSION_PROJECT_MODULE_WORKERS"], values[versions.ENGINE])
        self.assertEqual(versions.replace_versions(text, {}), text)

    def test_owners(self):
        expected = {versions.ENGINE, "VERSION_PROJECT_MODULE_NATIVE_GLFW", "VERSION_PROJECT_MODULE_UI_RMLUI"}
        self.assertEqual(versions.affected_components({"cmake/CherylOptions.cmake", "extern/glfw", "extern/rmlui"}), expected)
        self.assertEqual(versions.affected_components({"projects/engine/tests/unit/src/test.cpp", "docs/site/runtime/logging.md",
                                                       "docs/site/modules/rmlui.md", ".github/workflows/ci.yml"}), set())
        self.assertEqual(versions.affected_components({"projects/apps/first-game/src/first-game.cpp"}), set())

    def test_release_target(self):
        for current, stable, expected in (("4.0.9", "4.0.0", "4.1.0"), ("4.3.2", "4.0.0", "4.3.0"),
                                          ("5.0.2", "4.0.0", "5.0.0")):
            with self.subTest(current=current):
                self.assertEqual(versions.release_version(current, stable), expected)
        with self.assertRaises(ValueError):
            versions.release_version("3.0.0", "4.0.0")


class AutomationTests(unittest.TestCase):
    def setUp(self):
        self.git = FakeGit()
        self.api = FakeApi(self.git)
        self.automation = versions.Automation(self.api, self.git)

    def pending(self, source=A, values=None, kind="maintenance", branch="ci/version-update"):
        value = state(source, values, kind)
        self.git.files[BOT] = {**self.git.files[source], versions.STATE_FILE: json.dumps(value),
                               versions.VERSION_FILE: versions.replace_versions(self.git.text(source, versions.VERSION_FILE), value["versions"])}
        self.api.refs[branch] = BOT
        return value

    def test_frozen(self):
        for releases in ([], [{**self.api.releases[0], "draft": True}], [{**self.api.releases[0], "prerelease": True}]):
            with self.subTest(releases=releases), redirect_stdout(io.StringIO()):
                self.api.releases = releases
                self.assertIsNone(self.automation.maintain())
                self.assertFalse(self.api.commits)
                self.assertFalse(self.api.pulls)

    def test_bootstrap(self):
        initial = self.automation.initial_state(self.automation.stable_release(), B)
        self.assertEqual(initial["source_revision"], A)
        self.assertNotEqual(initial["source_revision"], STABLE)

    def test_maintenance(self):
        pull = self.automation.maintain()
        values = versions.read_versions(self.git.text(GENERATED, versions.VERSION_FILE))
        self.assertEqual(values[versions.PROJECT], "4.0.1")
        self.assertEqual(values[versions.ENGINE], "4.0.1")
        self.assertEqual(values["VERSION_PROJECT_MODULE_NATIVE_GLFW"], "1.0.0")
        self.assertEqual(pull["head"]["ref"], "ci/version-update")
        self.assertEqual(pull["base"]["ref"], "develop")
        self.assertIn("Approve", pull["body"])
        self.assertEqual(self.api.commits[0]["parents"], [B])
        self.assertFalse(any("dispatch" in path for _, path, _, _ in self.api.calls))

    def test_repeat(self):
        self.automation.maintain()
        self.automation.maintain()
        self.assertEqual(len(self.api.commits), 1)
        self.assertEqual(len(self.api.pulls), 1)

    def test_accumulate(self):
        values = versions.read_versions(TEXT)
        values[versions.PROJECT] = values[versions.ENGINE] = "4.0.1"
        self.pending(B, values)
        self.git.refs["HEAD"] = self.git.refs["refs/remotes/origin/develop"] = C
        self.git.parents_by_revision[C] = [B]
        self.git.files[C] = {**self.git.files[B], "source.cpp": C}
        self.git.paths[C] = {"projects/modules/platform/native-glfw/src/window.cpp"}
        self.automation.maintain()
        actual = versions.read_versions(self.git.text(GENERATED, versions.VERSION_FILE))
        self.assertEqual(actual[versions.PROJECT], "4.0.2")
        self.assertEqual(actual[versions.ENGINE], "4.0.1")
        self.assertEqual(actual["VERSION_PROJECT_MODULE_NATIVE_GLFW"], "1.0.1")
        self.assertEqual(self.api.commits[0]["parents"], [BOT, C])
        update = next(body for method, path, body, _ in self.api.calls if method == "PATCH" and path.startswith("/git/refs/"))
        self.assertFalse(update["force"])

    def test_manual(self):
        self.git.files[B][versions.VERSION_FILE] = versions.replace_versions(TEXT, {versions.PROJECT: "5.0.0", versions.ENGINE: "7.0.0"})
        self.git.paths[B].add(versions.VERSION_FILE)
        planned = self.automation.planned_versions(state(), B)
        self.assertEqual(planned[versions.PROJECT], "5.0.0")
        self.assertEqual(planned[versions.ENGINE], "7.0.0")

    def test_version_only(self):
        self.git.paths[B] = set(versions.OWNED_FILES)
        self.git.files[B][versions.VERSION_FILE] = versions.replace_versions(TEXT, {versions.PROJECT: "4.0.1"})
        self.assertEqual(self.automation.planned_versions(state(), B)[versions.PROJECT], "4.0.1")
        self.assertEqual(self.automation.planned_versions(state(), B)[versions.ENGINE], "4.0.0")

    def test_old_generated_merge(self):
        older = versions.read_versions(TEXT)
        older[versions.PROJECT] = older[versions.ENGINE] = "4.0.1"
        newer = {**older, versions.PROJECT: "4.0.2", versions.ENGINE: "4.0.2"}
        self.git.parents_by_revision[C] = [B]
        self.git.files[C] = {**self.git.files[B], versions.VERSION_FILE: versions.replace_versions(TEXT, older),
                             versions.STATE_FILE: json.dumps(state(A, older))}
        self.git.paths[C] = set(versions.OWNED_FILES)
        self.assertEqual(self.automation.planned_versions(state(B, newer), C), newer)

    def test_docs(self):
        self.git.paths[B] = {"docs/site/development/building.md"}
        planned = self.automation.planned_versions(state(), B)
        self.assertEqual(planned[versions.PROJECT], "4.0.1")
        self.assertEqual(planned[versions.ENGINE], "4.0.0")

    def test_protected_bot(self):
        self.api.refs["ci/version-update"] = BOT
        self.git.files[BOT] = dict(self.git.files[A])
        with self.assertRaisesRegex(ValueError, "no owned"):
            self.automation.maintain()
        self.pending()
        self.git.files[BOT]["unrelated.cpp"] = "user work"
        with self.assertRaisesRegex(ValueError, "outside generated"):
            self.automation.maintain()
        self.assertFalse(self.api.commits)

    def test_rollback(self):
        saved = state(source="f" * 40)
        self.git.files[B][versions.STATE_FILE] = json.dumps(saved)
        with self.assertRaisesRegex(ValueError, "rollback"):
            self.automation.maintain()
        self.assertFalse(self.api.commits)

    def test_stale_pending(self):
        pending = {"state": self.pending(A)}
        saved = state(source=B)
        saved["versions"][versions.PROJECT] = "4.1.0"
        self.git.files[B][versions.STATE_FILE] = json.dumps(saved)
        actual = self.automation.current_state(self.automation.stable_release(), B, pending)
        self.assertEqual(actual["source_revision"], B)
        self.assertEqual(actual["versions"][versions.PROJECT], "4.1.0")

    def test_release_bootstrap(self):
        self.api.releases = []
        existing = self.api.request("POST", "/pulls", {"head": "develop", "base": "main", "title": "Release 4.0.0",
                                                      "body": "User release notes", "draft": True})
        result = self.automation.prepare_release()
        self.assertEqual(result["release"]["number"], existing["number"])
        self.assertEqual(result["release"]["body"], "User release notes")
        self.assertTrue(result["release"]["draft"])
        self.assertFalse(self.api.commits)

    def test_prepare_release(self):
        self.git.files[B][versions.VERSION_FILE] = versions.replace_versions(TEXT, {versions.PROJECT: "4.0.9", versions.ENGINE: "4.0.7"})
        result = self.automation.prepare_release()
        actual = versions.read_versions(self.git.text(GENERATED, versions.VERSION_FILE))
        self.assertEqual(actual[versions.PROJECT], "4.1.0")
        self.assertEqual(actual[versions.ENGINE], "4.0.7")
        self.assertEqual(result["preparation"]["base"]["ref"], "develop")
        self.assertEqual(result["release"]["title"], "Release 4.1.0")
        self.assertTrue(result["release"]["draft"])

    def test_pending_release(self):
        planned = versions.read_versions(TEXT)
        planned[versions.PROJECT] = planned[versions.ENGINE] = "4.0.1"
        self.pending(B, planned)
        with self.assertRaisesRegex(ValueError, "Merge the development version-update PR"):
            self.automation.prepare_release()
        self.assertFalse(self.api.commits)
        self.assertFalse(self.api.pulls)

    def test_tag_history(self):
        self.git.refs["refs/remotes/origin/main"] = MAIN
        with self.assertRaisesRegex(ValueError, "not part of main"):
            self.automation.maintain()
        self.assertFalse(self.api.commits)

    def test_pr_permissions(self):
        with patch.object(self.api, "request", side_effect=RuntimeError("HTTP 403")):
            with self.assertRaisesRegex(RuntimeError, "Allow GitHub Actions to create and approve pull requests"):
                self.automation.ensure_pull("ci/version-update", "develop", "Updates versions", versions.MARKER)


class ReleasePolicyTests(unittest.TestCase):
    def setUp(self):
        self.git = FakeGit()
        self.pull = {"title": "Release 4.0.0", "base": {"ref": "main"},
                     "head": {"ref": "develop", "sha": B, "repo": {"full_name": versions.REPOSITORY}}}

    def test_release(self):
        self.assertEqual(versions.check_release({"pull_request": self.pull}, self.git), "4.0.0")

    def test_development(self):
        self.pull["base"]["ref"] = "develop"
        self.pull["title"] = "Ordinary development"
        self.assertIsNone(versions.check_release({"pull_request": self.pull}, self.git))

    def test_title(self):
        self.pull["title"] = "Release 4.1.0"
        with self.assertRaisesRegex(ValueError, "title"):
            versions.check_release({"pull_request": self.pull}, self.git)

    def test_branch(self):
        self.pull["head"]["repo"]["full_name"] = "someone/cheryl-engine"
        with self.assertRaisesRegex(ValueError, "repository's develop"):
            versions.check_release({"pull_request": self.pull}, self.git)

    def test_patch(self):
        self.git.files[B][versions.VERSION_FILE] = versions.replace_versions(TEXT, {versions.PROJECT: "4.0.1"})
        self.pull["title"] = "Release 4.0.1"
        with self.assertRaisesRegex(ValueError, "zero patch"):
            versions.check_release({"pull_request": self.pull}, self.git)


if __name__ == "__main__":
    unittest.main()
