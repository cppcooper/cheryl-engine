#!/usr/bin/env python3
"""Mirror a checked canonical main push to the stable documentation branch."""

from __future__ import annotations

import json
import os
import re
import sys
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


REPOSITORY = "cppcooper/cheryl-engine"
API_ROOT = f"https://api.github.com/repos/{REPOSITORY}"


def publication_revision(environment) -> str | None:
    required = {
        "GITHUB_ACTIONS": "true",
        "GITHUB_REPOSITORY": REPOSITORY,
        "GITHUB_EVENT_NAME": "push",
        "GITHUB_REF": "refs/heads/main",
        "CHERYL_DOCS_PUBLISH_STABLE": "true",
        "CHERYL_DOCS_CHECKS_RESULT": "success",
    }
    if any(environment.get(name) != value for name, value in required.items()):
        return None
    revision = environment.get("GITHUB_SHA", "")
    if not isinstance(revision, str) or not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise RuntimeError("The checked main revision must be a full Git commit ID.")
    return revision


class GitHubClient:
    def __init__(self, token: str):
        if not token:
            raise RuntimeError("GITHUB_TOKEN is required for an enabled stable sync.")
        self.token = token

    def request(self, method: str, path: str, body=None, *, missing_ok=False):
        data = None if body is None else json.dumps(body).encode("utf-8")
        request = Request(
            f"{API_ROOT}/{path}",
            data=data,
            method=method,
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {self.token}",
                "Content-Type": "application/json",
                "X-GitHub-Api-Version": "2026-03-10",
                "User-Agent": "cheryl-documentation-stable-sync",
            },
        )
        try:
            with urlopen(request, timeout=30) as response:
                result = json.load(response)
        except HTTPError as error:
            error.close()
            if method == "GET" and missing_ok and error.code == 404:
                return None
            raise RuntimeError(
                f"GitHub rejected {method} {path} (HTTP {error.code}); "
                "inspect repository rules and refs before retrying."
            ) from error
        except (URLError, OSError, ValueError) as error:
            raise RuntimeError(f"GitHub {method} {path} did not complete.") from error
        if not isinstance(result, dict):
            raise RuntimeError(f"GitHub returned an invalid response for {method} {path}.")
        return result


def ref_revision(response, branch: str) -> str:
    if not isinstance(response, dict) or response.get("ref") != f"refs/heads/{branch}":
        raise RuntimeError(f"GitHub did not return the expected {branch} ref.")
    object_data = response.get("object")
    if not isinstance(object_data, dict) or object_data.get("type") != "commit":
        raise RuntimeError(f"The {branch} ref does not identify a commit.")
    revision = object_data.get("sha")
    if not isinstance(revision, str) or not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise RuntimeError(f"The {branch} ref has an invalid commit ID.")
    return revision


def mirror_stable(client: GitHubClient, revision: str) -> str:
    main_path = "git/ref/heads/main"
    if ref_revision(client.request("GET", main_path), "main") != revision:
        return "stale"
    stable = client.request("GET", "git/ref/heads/stable", missing_ok=True)
    if stable is None:
        method, path = "POST", "git/refs"
        body = {"ref": "refs/heads/stable", "sha": revision}
        outcome = "created"
    else:
        stable_revision = ref_revision(stable, "stable")
        if stable_revision == revision:
            return "unchanged"
        comparison = client.request("GET", f"compare/{stable_revision}...{revision}")
        merge_base = comparison.get("merge_base_commit") if isinstance(comparison, dict) else None
        if (
            not isinstance(comparison, dict)
            or comparison.get("status") != "ahead"
            or not isinstance(merge_base, dict)
            or merge_base.get("sha") != stable_revision
        ):
            raise RuntimeError(
                "Stable cannot fast-forward to checked main; resolve the refs "
                "without deleting, resetting or forcing the stable branch."
            )
        method, path = "PATCH", "git/refs/heads/stable"
        body = {"sha": revision, "force": False}
        outcome = "updated"
    # Recheck after API preparation; GitHub also enforces fast-forward at the write.
    if ref_revision(client.request("GET", main_path), "main") != revision:
        return "stale"
    result = client.request(method, path, body)
    if ref_revision(result, "stable") != revision:
        raise RuntimeError("GitHub did not confirm stable at the checked main revision.")
    return outcome


def main() -> int:
    revision = publication_revision(os.environ)
    if revision is None:
        print("Stable documentation sync is disabled for this run.")
        return 0
    outcome = mirror_stable(GitHubClient(os.environ.get("GITHUB_TOKEN", "")), revision)
    if outcome == "stale":
        print("Main has advanced; this older checked run will not publish stable.")
    else:
        print(f"Stable documentation ref {outcome}: {revision}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"Stable documentation sync failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
