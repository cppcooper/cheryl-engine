"""Prepare reviewed component version changes without writing protected branches."""

import argparse
import base64
import json
import os
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
REPOSITORY = "cppcooper/cheryl-engine"
VERSION_FILE = "cmake/CherylVersions.cmake"
STATE_FILE = ".github/ci/version-state.json"
OWNED_FILES = {VERSION_FILE, STATE_FILE}
PROJECT = "VERSION_PROJECT_CHERYL"
ENGINE = "VERSION_PROJECT_ENGINE"
MARKER = "<!-- cheryl-version-automation -->"
SEMVER = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)")
SHA = re.compile(r"[0-9a-f]{40}")
VERSION_SETTING = re.compile(
    r'(?m)^(?P<prefix>[ \t]*set\([ \t]*(?P<name>VERSION_PROJECT_[A-Z0-9_]+)[ \t]+)'
    r'(?P<quote>"?)(?P<version>[^"\s)]+)(?P=quote)(?P<suffix>[ \t]*\)[^\r\n]*)(?P<cr>\r?)$'
)
VERSION_ALIAS = re.compile(r"\$\{(VERSION_PROJECT_[A-Z0-9_]+)\}")
COMPONENT_PATHS = {
    "projects/engine/": ENGINE,
    "projects/modules/platform/native-glfw/": "VERSION_PROJECT_MODULE_NATIVE_GLFW",
    "projects/modules/platform/debug-terminal-linux/": "VERSION_PROJECT_MODULE_DEBUG_TERMINAL_LINUX",
    "projects/modules/graphics/opengl/": "VERSION_PROJECT_MODULE_OPENGL",
    "projects/modules/ui/tgui/": "VERSION_PROJECT_MODULE_UI_TGUI",
    "projects/modules/ui/rmlui/": "VERSION_PROJECT_MODULE_UI_RMLUI",
    "projects/modules/audio/miniaudio/": "VERSION_PROJECT_MODULE_AUDIO_MINIAUDIO",
    "projects/apps/demo/": "VERSION_PROJECT_APP_DEMO",
}
EXTERNAL_OWNERS = {
    "backward-cpp": ENGINE, "cli11": ENGINE, "ctti": ENGINE, "glm": ENGINE,
    "json": ENGINE, "spdlog": ENGINE, "stb": ENGINE,
    "glfw": "VERSION_PROJECT_MODULE_NATIVE_GLFW", "gainput": "VERSION_PROJECT_MODULE_NATIVE_GLFW",
    "glad": "VERSION_PROJECT_MODULE_OPENGL", "tgui": "VERSION_PROJECT_MODULE_UI_TGUI",
    "rmlui": "VERSION_PROJECT_MODULE_UI_RMLUI", "miniaudio": "VERSION_PROJECT_MODULE_AUDIO_MINIAUDIO",
}


def version_tuple(value):
    match = SEMVER.fullmatch(value) if isinstance(value, str) else None
    if not match:
        raise ValueError(f"Expected a three-part semantic version, got {value!r}")
    return tuple(map(int, match.groups()))


def read_versions(text):
    values = {}
    for match in VERSION_SETTING.finditer(text):
        name, value = match.group("name", "version")
        if name in values:
            raise ValueError(f"Duplicate version setting: {name}")
        alias = VERSION_ALIAS.fullmatch(value)
        if alias:
            referenced = alias[1]
            if referenced not in values:
                raise ValueError(f"Version alias must reference a previously declared version: {name}")
            value = values[referenced]
        version_tuple(value)
        values[name] = value
    return values


def replace_versions(text, values):
    existing = read_versions(text)
    for name, value in values.items():
        if name not in existing:
            raise ValueError(f"Unknown version setting: {name}")
        version_tuple(value)

    def replace(match):
        value = values.get(match.group("name"), match.group("version"))
        return (match.group("prefix") + match.group("quote") + value + match.group("quote")
                + match.group("suffix") + match.group("cr"))

    return VERSION_SETTING.sub(replace, text)


def patch_version(value):
    major, minor, patch = version_tuple(value)
    return f"{major}.{minor}.{patch + 1}"


def release_version(current, stable):
    major, minor, _ = version_tuple(current)
    stable_major, stable_minor, _ = version_tuple(stable)
    if (major, minor) < (stable_major, stable_minor):
        raise ValueError("The project version is behind the published stable release")
    if (major, minor) > (stable_major, stable_minor):
        return f"{major}.{minor}.0"
    return f"{major}.{minor + 1}.0"


def affected_components(paths):
    affected = set()
    for path in paths:
        if path in OWNED_FILES or path.endswith((".md", ".rst")) or "/tests/" in path or "/docs/" in path:
            continue
        for prefix, component in COMPONENT_PATHS.items():
            if path.startswith(prefix):
                affected.add(component)
        if path.startswith("cmake/"):
            affected.add(ENGINE)
        if path == "assets" or path.startswith("assets/"):
            affected.add(ENGINE)
        if path.startswith("extern/"):
            owner = EXTERNAL_OWNERS.get(path.split("/", 2)[1])
            if owner:
                affected.add(owner)
    return affected


def commit_id(value):
    if not isinstance(value, str) or not SHA.fullmatch(value):
        raise ValueError("Expected a full Git commit ID")
    return value


def validate_state(value):
    if not isinstance(value, dict) or value.get("format") != 1 or value.get("kind") not in {"maintenance", "release"}:
        raise ValueError("Invalid version-maintenance state")
    commit_id(value.get("source_revision"))
    commit_id(value.get("stable_revision"))
    if not isinstance(value.get("stable_tag"), str) or not value["stable_tag"].startswith("v"):
        raise ValueError("Invalid stable tag in version-maintenance state")
    version_tuple(value["stable_tag"][1:])
    values = value.get("versions")
    if not isinstance(values, dict) or PROJECT not in values or ENGINE not in values:
        raise ValueError("Version-maintenance state is missing component versions")
    for name, version in values.items():
        if not re.fullmatch(r"VERSION_PROJECT_[A-Z0-9_]+", name):
            raise ValueError("Invalid component name in version-maintenance state")
        version_tuple(version)
    return value


class Git:
    def __init__(self, source=ROOT):
        self.source = Path(source)

    def output(self, *arguments):
        return subprocess.check_output(["git", *arguments], cwd=self.source, text=True)

    def resolve(self, ref):
        return commit_id(self.output("rev-parse", "--verify", ref + "^{commit}").strip())

    def text(self, revision, path, *, optional=False):
        commit_id(revision)
        try:
            return self.output("show", f"{revision}:{path}")
        except subprocess.CalledProcessError:
            if optional:
                return None
            raise

    def ancestor(self, before, after):
        result = subprocess.run(["git", "merge-base", "--is-ancestor", commit_id(before), commit_id(after)],
                                cwd=self.source, capture_output=True, text=True)
        if result.returncode not in (0, 1):
            raise RuntimeError("Cannot establish version-maintenance Git ancestry")
        return result.returncode == 0

    def parents(self, revision):
        return self.output("rev-list", "--parents", "-n", "1", commit_id(revision)).split()[1:]

    def integrations(self, before, after):
        return self.output("rev-list", "--first-parent", "--reverse", f"{commit_id(before)}..{commit_id(after)}").split()

    def changed(self, before, after):
        return set(filter(None, self.output("diff", "--no-renames", "--name-only", "-z",
                                            commit_id(before), commit_id(after)).split("\0")))


class Automation:
    def __init__(self, api, git):
        self.api = api
        self.git = git

    def stable_release(self):
        releases = []
        for release in self.api.pages("/releases"):
            tag = release.get("tag_name", "")
            if (not release.get("draft") and not release.get("prerelease") and release.get("published_at")
                    and isinstance(tag, str) and tag.startswith("v") and SEMVER.fullmatch(tag[1:])
                    and version_tuple(tag[1:]) >= (4, 0, 0)):
                releases.append(release)
        if not releases:
            return None
        release = max(releases, key=lambda entry: version_tuple(entry["tag_name"][1:]))
        revision = self.git.resolve("refs/tags/" + release["tag_name"])
        if not self.git.ancestor(revision, self.git.resolve("refs/remotes/origin/main")):
            raise ValueError("The published stable tag is not part of main; review release history before versioning")
        return {"tag": release["tag_name"], "revision": revision}

    def contents(self, revision, path, *, optional=False):
        result = self.api.request("GET", "/contents/" + path, ref=commit_id(revision), missing_ok=optional)
        if result is None and optional:
            return None
        if not isinstance(result, dict) or result.get("encoding") != "base64":
            raise ValueError(f"Expected a repository file: {path}")
        return base64.b64decode(result["content"]).decode("utf-8")

    def tree(self, revision):
        commit = self.api.request("GET", "/git/commits/" + commit_id(revision))
        tree = self.api.request("GET", "/git/trees/" + commit["tree"]["sha"], recursive=1)
        if tree.get("truncated"):
            raise ValueError("Cannot validate a truncated version branch tree")
        return {entry["path"]: (entry["mode"], entry["type"], entry["sha"])
                for entry in tree["tree"] if entry["path"] not in OWNED_FILES and entry["type"] != "tree"}

    def pending(self, branch, head, kind):
        ref = self.api.request("GET", "/git/ref/heads/" + branch, missing_ok=True)
        if ref is None:
            return None
        revision = commit_id(ref["object"]["sha"])
        raw_state = self.contents(revision, STATE_FILE, optional=True)
        if raw_state is None:
            raise ValueError(f"Refusing to overwrite {branch}: it has no owned version-maintenance state")
        state = validate_state(json.loads(raw_state))
        if state["kind"] != kind or not self.git.ancestor(state["source_revision"], head):
            raise ValueError(f"Refusing to overwrite {branch}: its version state does not match develop history")
        source = state["source_revision"]
        source_text = self.git.text(source, VERSION_FILE)
        if (self.contents(revision, VERSION_FILE) != replace_versions(source_text, state["versions"])
                or self.tree(revision) != self.tree(source)):
            raise ValueError(f"Refusing to overwrite {branch}: it contains changes outside generated version state")
        return {"revision": revision, "state": state}

    def initial_state(self, stable, head):
        revision = stable["revision"]
        parents = self.git.parents(revision)
        if len(parents) == 1:
            from release import release_pull
            pull = release_pull(self.api, revision, stable["tag"][1:])
            if pull is None:
                raise ValueError("Cannot identify the develop release PR for the squashed stable release")
            # The main squash omits develop's ancestry even after it is merged back.
            revision = pull["head"]["sha"]
        elif not self.git.ancestor(revision, head):
            if len(parents) < 2 or not self.git.ancestor(parents[1], head):
                raise ValueError("Cannot identify the develop revision promoted by the stable release; review branch ancestry")
            revision = parents[1]
        if not self.git.ancestor(revision, head):
            raise ValueError("The develop revision promoted by the stable release is outside develop history")
        return {"format": 1, "kind": "maintenance", "source_revision": revision,
                "versions": read_versions(self.git.text(revision, VERSION_FILE)),
                "stable_tag": stable["tag"], "stable_revision": stable["revision"]}

    def current_state(self, stable, head, pending):
        raw = self.git.text(head, STATE_FILE, optional=True)
        state = validate_state(json.loads(raw)) if raw else self.initial_state(stable, head)
        if not self.git.ancestor(state["source_revision"], head):
            raise ValueError("The saved version watermark is outside develop history; review the rollback before continuing")
        if pending:
            candidate = pending["state"]
            if state["source_revision"] != candidate["source_revision"] and self.git.ancestor(
                    state["source_revision"], candidate["source_revision"]):
                state = candidate
            elif not self.git.ancestor(candidate["source_revision"], state["source_revision"]):
                raise ValueError("Pending and saved version watermarks diverge; review the version PR")
        if state["stable_tag"] == stable["tag"] and state["stable_revision"] != stable["revision"]:
            raise ValueError("The published stable tag moved after version maintenance began")
        return state

    def planned_versions(self, state, head):
        values = dict(state["versions"])
        for revision in self.git.integrations(state["source_revision"], head):
            parents = self.git.parents(revision)
            if not parents:
                raise ValueError("An integration commit has no parent")
            before = read_versions(self.git.text(parents[0], VERSION_FILE))
            after = read_versions(self.git.text(revision, VERSION_FILE))
            paths = self.git.changed(parents[0], revision)
            if not paths - OWNED_FILES and STATE_FILE in paths:
                raw = self.git.text(revision, STATE_FILE, optional=True)
                generated = validate_state(json.loads(raw)) if raw else None
                if generated and generated["versions"] == after:
                    # A delayed generated PR must not roll back already accumulated bumps.
                    if generated["kind"] == "release" and version_tuple(after[PROJECT]) > version_tuple(values[PROJECT]):
                        values[PROJECT] = after[PROJECT]
                    continue
            manual = {name: value for name, value in after.items() if before.get(name) != value}
            values.update(manual)
            if not paths - OWNED_FILES:
                continue
            for name in {PROJECT, *affected_components(paths)} - manual.keys():
                if name not in values:
                    raise ValueError(f"Missing component version: {name}")
                values[name] = patch_version(values[name])
        return values

    def ensure_pull(self, branch, base, title, body, *, draft=False):
        pulls = self.api.pages("/pulls", state="open", base=base, head=REPOSITORY.split("/")[0] + ":" + branch)
        matches = [pull for pull in pulls if pull["head"]["ref"] == branch
                   and pull["head"].get("repo", {}).get("full_name") == REPOSITORY and pull["base"]["ref"] == base]
        if len(matches) > 1:
            raise ValueError(f"More than one open version PR for {branch}")
        if matches:
            pull = matches[0]
            # Keep user release notes and the existing draft state.
            update = {"title": title}
            if branch.startswith("ci/"):
                update["body"] = body
            return self.api.request("PATCH", f"/pulls/{pull['number']}", update)
        try:
            return self.api.request("POST", "/pulls", {"head": branch, "base": base, "title": title,
                                                        "body": body, "draft": draft})
        except RuntimeError as error:
            raise RuntimeError("Could not create the version PR. Check Settings > Actions > General > Workflow permissions > "
                               "Allow GitHub Actions to create and approve pull requests, and grant this workflow "
                               "contents/pull-requests write permissions.") from error

    def propose(self, branch, kind, head, stable, values, pending, title):
        source_text = self.git.text(head, VERSION_FILE)
        if values == read_versions(source_text):
            return None
        state = {"format": 1, "kind": kind, "source_revision": head, "versions": values,
                 "stable_tag": stable["tag"], "stable_revision": stable["revision"]}
        files = {VERSION_FILE: replace_versions(source_text, values), STATE_FILE: json.dumps(state, indent=2, sort_keys=True) + "\n"}
        if pending:
            current = self.api.request("GET", "/git/ref/heads/" + branch)
            if current["object"]["sha"] != pending["revision"]:
                raise ValueError("The version branch advanced during planning; rerun maintenance")
            if all(self.contents(pending["revision"], path) == text for path, text in files.items()):
                return self.ensure_pull(branch, "develop", title, MARKER + "\n\nGenerated version changes for review. "
                                        "Approve the GitHub Actions workflows on this PR so its required checks can run.")
        source_commit = self.api.request("GET", "/git/commits/" + head)
        tree = self.api.request("POST", "/git/trees", {"base_tree": source_commit["tree"]["sha"], "tree": [
            {"path": path, "mode": "100644", "type": "blob", "content": text} for path, text in files.items()]})
        parents = [pending["revision"], head] if pending and pending["revision"] != head else [head]
        identity = {"name": "cppcooper", "email": "cppcooper@users.noreply.github.com"}
        commit = self.api.request("POST", "/git/commits", {"message": "Updates reviewed component versions", "tree": tree["sha"],
                                                          "parents": parents, "author": identity, "committer": identity})
        if pending:
            self.api.request("PATCH", "/git/refs/heads/" + branch, {"sha": commit["sha"], "force": False})
        else:
            self.api.request("POST", "/git/refs", {"ref": "refs/heads/" + branch, "sha": commit["sha"]})
        return self.ensure_pull(branch, "develop", title, MARKER + "\n\nGenerated version changes for review. "
                                "Approve the GitHub Actions workflows on this PR so its required checks can run.")

    def maintain(self):
        stable = self.stable_release()
        if stable is None:
            print("Versions remain frozen until the first published stable 4.0.0 release on main.")
            return None
        head = self.git.resolve("HEAD")
        pending = self.pending("ci/version-update", head, "maintenance")
        state = self.current_state(stable, head, pending)
        values = self.planned_versions(state, head)
        return self.propose("ci/version-update", "maintenance", head, stable, values, pending,
                            "Updates development component versions")

    def prepare_release(self):
        head = self.git.resolve("HEAD")
        values = read_versions(self.git.text(head, VERSION_FILE))
        stable = self.stable_release()
        preparation = None
        if stable is None:
            target = "4.0.0"
            if values.get(PROJECT) != target:
                raise ValueError("The first release is frozen at 4.0.0; review the CMake project version")
        else:
            maintenance = self.pending("ci/version-update", head, "maintenance")
            current = self.current_state(stable, head, maintenance)
            if self.planned_versions(current, head) != values:
                raise ValueError("Merge the development version-update PR before preparing the release")
            target = release_version(values[PROJECT], stable["tag"][1:])
            pending = self.pending("ci/release-version", head, "release")
            values[PROJECT] = target
            preparation = self.propose("ci/release-version", "release", head, stable, values, pending,
                                       "Prepares Release " + target + " versions")
        release = self.ensure_pull("develop", "main", "Release " + target,
                                   "Prepare the release from develop after its version changes and required checks pass.", draft=True)
        return {"preparation": preparation, "release": release}


def check_release(event, git):
    pull = event.get("pull_request") if isinstance(event, dict) else None
    if not isinstance(pull, dict):
        raise ValueError("Release policy requires a pull request event")
    if pull.get("base", {}).get("ref") != "main":
        return None
    head = pull.get("head", {})
    if head.get("ref") != "develop" or head.get("repo", {}).get("full_name") != REPOSITORY:
        raise ValueError("Stable release PRs must use this repository's develop branch")
    value = read_versions(git.text(commit_id(head.get("sha")), VERSION_FILE)).get(PROJECT)
    if value is None or version_tuple(value)[2] != 0 or pull.get("title") != "Release " + value:
        raise ValueError("Release PR title must match the CMake project version with a zero patch: Release X.Y.0")
    return value


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("maintain")
    commands.add_parser("prepare-release")
    check = commands.add_parser("check-release")
    check.add_argument("--event", type=Path, default=os.environ.get("GITHUB_EVENT_PATH"))
    args = parser.parse_args(arguments)
    git = Git(args.source)
    if args.command == "check-release":
        if not args.event:
            parser.error("check-release requires --event or GITHUB_EVENT_PATH")
        result = check_release(json.loads(Path(args.event).read_text(encoding="utf-8")), git)
    else:
        events = {"push", "workflow_dispatch"} if args.command == "maintain" else {"workflow_dispatch"}
        if (os.environ.get("GITHUB_REPOSITORY") != REPOSITORY or os.environ.get("GITHUB_REF") != "refs/heads/develop"
                or os.environ.get("GITHUB_EVENT_NAME") not in events):
            raise ValueError("Version changes require the canonical develop push or manual release-preparation workflow")
        if git.resolve("HEAD") != git.resolve("refs/remotes/origin/develop"):
            raise ValueError("The checked source is no longer the current develop revision; rerun maintenance")
        from github_api import GitHubApi
        automation = Automation(GitHubApi(REPOSITORY, os.environ.get("GITHUB_TOKEN")), git)
        result = automation.maintain() if args.command == "maintain" else automation.prepare_release()
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"Version maintenance failed: {error}") from error
