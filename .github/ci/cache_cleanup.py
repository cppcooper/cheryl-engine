"""Plan or apply bounded GitHub Actions cache maintenance; dry run by default."""

import argparse
from datetime import datetime, timedelta, timezone
import json
import math
import os
from pathlib import Path
import re
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import HTTPRedirectHandler, Request, build_opener


GIB = 1024 ** 3
SNAPSHOT = re.compile(r"^((?:ccache-v1|dependencies-v1|pip-v1)-.+)-[0-9]+-[0-9]+$")
PULL_REF = re.compile(r"^refs/pull/([1-9][0-9]*)/merge$")
KNOWN_PREFIXES = ("ccache-v1-", "dependencies-v1-", "pip-v1-", "docs-tools-v1-",
                  "windows-tools-v1-", "submodule-git-v1-", "apt-debs-v1-")
MAINTENANCE_PATH = ".github/workflows/cache-maintenance.yml"


def lineage(key):
    match = SNAPSHOT.fullmatch(key)
    return match.group(1) if match else None


def timestamp(value):
    if not isinstance(value, str):
        raise ValueError("Cache timestamps must be ISO 8601 strings")
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None:
        raise ValueError("Cache timestamps must include a timezone")
    return parsed


def plan_caches(caches, *, default_branch, closed_prs, active_refs, now,
                budget_bytes=8 * GIB, grace_seconds=900, closed_pr=None, unresolved_refs=frozenset()):
    """Return a deletion plan without mutating caches or accessing GitHub.

    Active/unresolved refs and recently created or accessed entries are protected.
    Exact-PR mode never compacts or trims entries outside that confirmed closed PR.
    """
    if not isinstance(default_branch, str) or not default_branch or now.tzinfo is None:
        raise ValueError("A default branch and timezone-aware current time are required")
    if type(budget_bytes) is not int or budget_bytes < 0 or grace_seconds < 0:
        raise ValueError("The cache budget and grace period must be nonnegative")
    if closed_pr is not None and (type(closed_pr) is not int or closed_pr <= 0):
        raise ValueError("A pull request number must be positive")

    entries = []
    seen = set()
    protected = set()
    cutoff = now - timedelta(seconds=grace_seconds)
    for cache in caches:
        identifier = cache.get("id")
        if type(identifier) is not int or identifier <= 0 or identifier in seen:
            raise ValueError("Cache IDs must be unique positive integers")
        seen.add(identifier)
        if type(cache.get("size_in_bytes")) is not int or cache["size_in_bytes"] < 0:
            raise ValueError("Cache sizes must be nonnegative integers")
        if any(not isinstance(cache.get(key), str) or not cache[key] for key in ("key", "ref", "version")):
            raise ValueError("Each cache needs a key, ref and version")
        created, accessed = timestamp(cache["created_at"]), timestamp(cache["last_accessed_at"])
        record = (dict(cache), created, accessed, lineage(cache["key"]))
        entries.append(record)
        if cache["ref"] in active_refs or cache["ref"] in unresolved_refs or max(created, accessed) >= cutoff:
            protected.add(identifier)

    before = sum(cache["size_in_bytes"] for cache, *_ in entries)
    result = {"before_bytes": before, "after_bytes": before,
              "protected_bytes": sum(cache["size_in_bytes"] for cache, *_ in entries if cache["id"] in protected),
              "protected_ids": sorted(protected), "deletions": []}
    selected = set()

    def remove(cache, reason):
        if cache["id"] not in protected and cache["id"] not in selected:
            selected.add(cache["id"])
            result["deletions"].append({**cache, "reason": reason})
            result["after_bytes"] -= cache["size_in_bytes"]

    for cache, *_ in entries:
        match = PULL_REF.fullmatch(cache["ref"])
        number = int(match.group(1)) if match else None
        if number in closed_prs and (closed_pr is None or number == closed_pr):
            remove(cache, "closed pull request")
    if closed_pr is not None:
        return result

    newest = {}
    baseline = {}
    for record in entries:
        cache, created, _, logical = record
        if logical is not None:
            bucket = (cache["ref"], cache["version"], logical)
            if bucket not in newest or (created, cache["id"]) > (newest[bucket][1], newest[bucket][0]["id"]):
                newest[bucket] = record
        if cache["ref"] == "refs/heads/" + default_branch and cache["key"].startswith(KNOWN_PREFIXES):
            bucket = (cache["version"], logical or cache["key"])
            if bucket not in baseline or (created, cache["id"]) > (baseline[bucket][1], baseline[bucket][0]["id"]):
                baseline[bucket] = record
    for cache, _, _, logical in entries:
        if logical is not None and newest[(cache["ref"], cache["version"], logical)][0]["id"] != cache["id"]:
            remove(cache, "superseded snapshot")

    baseline_ids = {record[0]["id"] for record in baseline.values()}
    for cache, _, _, _ in sorted(entries, key=lambda record: (
            record[0]["id"] in baseline_ids, record[2], record[1], record[0]["id"])):
        if result["after_bytes"] <= budget_bytes:
            break
        remove(cache, "default-branch baseline" if cache["id"] in baseline_ids else "least recently used")
    return result


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, request, file, code, message, headers, new_url):
        return None


class GitHubApi:
    def __init__(self, repository, token, api_url="https://api.github.com"):
        if not isinstance(repository, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository) or any(
                part in {".", ".."} for part in repository.split("/")):
            raise ValueError("GITHUB_REPOSITORY must be owner/repository")
        if not isinstance(api_url, str) or api_url.rstrip("/") != "https://api.github.com":
            raise ValueError("Cache maintenance supports the GitHub.com HTTPS API only")
        if not isinstance(token, str) or not token or any(character.isspace() for character in token):
            raise ValueError("A nonempty GitHub token without whitespace is required")
        self.base = api_url.rstrip("/") + "/repos/" + repository
        self.token = token
        self.opener = build_opener(NoRedirect())

    def request(self, method, path, *, missing_ok=False, **query):
        if method not in {"GET", "DELETE"} or (path and not path.startswith("/")) or any(
                value in path for value in ("//", "?", "#")) or ".." in path.split("/"):
            raise ValueError("Expected a repository-relative GitHub cache API request")
        url = self.base + path + ("?" + urlencode(query) if query else "")
        request = Request(url, method=method, headers={
            "Accept": "application/vnd.github+json", "Authorization": "Bearer " + self.token,
            "X-GitHub-Api-Version": "2026-03-10", "User-Agent": "cheryl-cache-maintenance",
        })
        try:
            with self.opener.open(request, timeout=30) as response:
                data = response.read()
        except HTTPError as error:
            if error.code == 404 and missing_ok:
                return None
            raise RuntimeError(f"GitHub API {method} {path} returned HTTP {error.code}") from None
        except URLError:
            raise RuntimeError(f"GitHub API {method} {path} could not be reached") from None
        value = json.loads(data) if data else {}
        if not isinstance(value, dict):
            raise ValueError("The GitHub API returned an unexpected response")
        return value

    def pages(self, path, field, **query):
        entries = []
        for page in range(1, 1001):
            value = self.request("GET", path, per_page=100, page=page, **query)
            batch = value.get(field)
            if not isinstance(batch, list):
                raise ValueError(f"The GitHub API response is missing {field}")
            entries.extend(batch)
            if len(batch) < 100:
                return entries
        raise RuntimeError("GitHub API pagination exceeded the maintenance limit")


def active_refs(api, caches):
    refs = set()
    current_run = os.environ.get("GITHUB_RUN_ID")
    for status in ("queued", "in_progress", "waiting", "pending", "requested"):
        for run in api.pages("/actions/runs", "workflow_runs", status=status):
            if (str(run["id"]) == current_run or run.get("path", "").split("@", 1)[0] == MAINTENANCE_PATH
                    or run.get("name") == "Maintain Actions caches"):
                continue
            if run.get("head_branch"):
                refs.add("refs/heads/" + run["head_branch"])
                refs.add("refs/tags/" + run["head_branch"])
            pulls = run.get("pull_requests", [])
            if not pulls and (run.get("event") == "pull_request" or not run.get("head_branch")):
                # Fork runs can omit PR metadata; other runs can omit their ref.
                # Keep every possible scope when the API cannot identify one.
                refs.update(cache["ref"] for cache in caches)
            for pull in pulls:
                refs.add(f"refs/pull/{int(pull['number'])}/merge")
                if pull.get("base", {}).get("ref"):
                    refs.add("refs/heads/" + pull["base"]["ref"])
    return refs


def pull_states(api, caches, closed_pr=None):
    numbers = {closed_pr} if closed_pr is not None else {
        int(match.group(1)) for cache in caches if (match := PULL_REF.fullmatch(cache["ref"]))
    }
    closed, unresolved = set(), set()
    for number in sorted(numbers):
        pull = api.request("GET", f"/pulls/{number}", missing_ok=True)
        if pull is None:
            unresolved.add(f"refs/pull/{number}/merge")
        elif pull.get("state") == "closed":
            closed.add(number)
        elif pull.get("state") != "open":
            raise ValueError("The GitHub API returned an unexpected pull request state")
    return closed, unresolved


def apply_plan(api, plan, *, apply=False):
    result = {"dry_run": not apply, "deleted_ids": [], "missing_ids": []}
    if apply:
        for cache in plan["deletions"]:
            response = api.request("DELETE", f"/actions/caches/{cache['id']}", missing_ok=True)
            result["missing_ids" if response is None else "deleted_ids"].append(cache["id"])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="Delete the planned caches; otherwise report only")
    parser.add_argument("--budget-gib", type=float, default=8, help="Repository cache target in GiB (default: 8)")
    parser.add_argument("--closed-pr", type=int, help="Only remove caches belonging to this confirmed closed PR")
    arguments = parser.parse_args()
    if not math.isfinite(arguments.budget_gib) or arguments.budget_gib < 0:
        parser.error("The cache budget must be finite and nonnegative")
    if arguments.closed_pr is not None and arguments.closed_pr <= 0:
        parser.error("The pull request number must be positive")
    budget_bytes = int(arguments.budget_gib * GIB)

    api = GitHubApi(os.environ.get("GITHUB_REPOSITORY"), os.environ.get("GITHUB_TOKEN"),
                    os.environ.get("GITHUB_API_URL", "https://api.github.com"))
    repository = api.request("GET", "")
    caches = api.pages("/actions/caches", "actions_caches", sort="created_at", direction="asc")
    closed, unresolved = pull_states(api, caches, arguments.closed_pr)
    if arguments.closed_pr is not None and arguments.closed_pr not in closed:
        print("The requested pull request is not confirmed closed; no caches selected.")
        return 0
    # Check live workflow admission after gathering cache/PR metadata. A new run
    # can still race deletion; cache maintenance offers no atomic restore lease.
    running = active_refs(api, caches)
    plan = plan_caches(caches, default_branch=repository["default_branch"], closed_prs=closed,
                       active_refs=running, unresolved_refs=unresolved, now=datetime.now(timezone.utc),
                       budget_bytes=budget_bytes, closed_pr=arguments.closed_pr)
    print("Applying cache maintenance." if arguments.apply else "Dry run; pass --apply to delete these caches.")
    for cache in plan["deletions"]:
        print(json.dumps({key: cache[key] for key in ("id", "key", "ref", "size_in_bytes", "reason")}))
    summary = (f"Caches: {len(caches)}; selected: {len(plan['deletions'])}; "
               f"storage: {plan['before_bytes'] / GIB:.2f} GiB ({plan['before_bytes']} bytes) -> "
               f"{plan['after_bytes'] / GIB:.2f} GiB ({plan['after_bytes']} bytes); "
               f"budget: {budget_bytes / GIB:.2f} GiB ({budget_bytes} bytes); "
               f"protected: {plan['protected_bytes'] / GIB:.2f} GiB ({plan['protected_bytes']} bytes)")
    print(summary)
    if arguments.closed_pr is None and plan["after_bytes"] > budget_bytes:
        print("The target cannot be reached while active, recent or unresolved caches remain protected.")
    result = apply_plan(api, plan, apply=arguments.apply)
    if arguments.apply:
        print(f"Deleted: {len(result['deleted_ids'])}; already absent: {len(result['missing_ids'])}")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a", encoding="utf-8") as output:
            output.write("### Cache maintenance" + (" (dry run)" if result["dry_run"] else "") + "\n\n" + summary + "\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError, RuntimeError) as error:
        print(f"Cache maintenance failed: {error}", file=sys.stderr)
        raise SystemExit(1)
