"""Admit trusted vcpkg contributions and retire superseded global snapshots."""

import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import re

from github_api import GitHubApi
from vcpkg_cache import TAR_LIMIT


REPOSITORY = "cppcooper/cheryl-engine"
ARTIFACT = re.compile(r"vcpkg-cache-X64-([A-Za-z0-9_.-]+)-([1-9][0-9]*)")
PREFIX = "vcpkg-v1-Windows-X64-msvc-"
SNAPSHOT = re.compile(re.escape(PREFIX) + r"[1-9][0-9]*-[1-9][0-9]*")
CONTRIBUTION_LIMIT = 32
ARTIFACT_LIMIT = TAR_LIMIT + 1024 ** 2
DOWNLOAD_LIMIT = 3 * 1024 ** 3


def positive(value):
    if type(value) is not int or value <= 0:
        raise ValueError("Expected a positive GitHub ID or run attempt")
    return value


def publication_branch(api, event, environment):
    repository = event.get("repository") or {}
    branch = repository.get("default_branch")
    if (api.repository != REPOSITORY or repository.get("full_name") != REPOSITORY
            or environment.get("GITHUB_REPOSITORY") != REPOSITORY
            or environment.get("GITHUB_EVENT_NAME") != "workflow_run"
            or not isinstance(branch, str) or environment.get("GITHUB_REF") != "refs/heads/" + branch):
        return None
    return branch


def admit(api, event, environment):
    if publication_branch(api, event, environment) is None:
        return {"allowed": "false", "reason": "Publication requires the default-branch workflow"}
    trigger = event.get("workflow_run") or {}
    identifier, attempt = positive(trigger.get("id")), positive(trigger.get("run_attempt"))
    run = api.request("GET", f"/actions/runs/{identifier}/attempts/{attempt}")
    workflow = api.request("GET", "/actions/workflows/ci.yml")
    if (run.get("id") != identifier or run.get("run_attempt") != attempt
            or positive(run.get("workflow_id")) != positive(workflow.get("id"))
            or not isinstance(run.get("head_sha"), str) or not re.fullmatch(r"[0-9a-f]{40}", run["head_sha"])
            or run["head_sha"] != trigger.get("head_sha")
            or run.get("path", "").split("@", 1)[0] != ".github/workflows/ci.yml"
            or run.get("status") != "completed"):
        raise ValueError("The completed source run does not match canonical CI")
    if (run.get("head_repository") or {}).get("full_name") != REPOSITORY:
        return {"allowed": "false", "reason": "Fork runs cannot publish shared dependency binaries"}
    if run.get("event") == "pull_request":
        pulls = run.get("pull_requests") or []
        if not pulls:
            return {"allowed": "false", "reason": "Cannot establish the source PR repository"}
        for entry in pulls:
            pull = api.request("GET", f"/pulls/{positive(entry.get('number'))}")
            if ((pull.get("head") or {}).get("repo") or {}).get("full_name") != REPOSITORY:
                return {"allowed": "false", "reason": "Fork PRs cannot publish shared dependency binaries"}
    elif run.get("event") not in {"push", "workflow_dispatch"}:
        return {"allowed": "false", "reason": "Unsupported source workflow event"}

    artifacts = []
    for artifact in api.pages(f"/actions/runs/{identifier}/artifacts", field="artifacts"):
        name = artifact.get("name", "")
        match = ARTIFACT.fullmatch(name) if isinstance(name, str) else None
        if not match or artifact.get("expired") is not False or int(match[2]) > attempt:
            continue
        positive(artifact.get("id"))
        size = artifact.get("size_in_bytes")
        if type(size) is not int or not 0 < size <= ARTIFACT_LIMIT:
            raise ValueError("A dependency contribution exceeds its artifact size limit")
        artifacts.append((artifact, match[1], int(match[2])))
    if len(artifacts) > CONTRIBUTION_LIMIT:
        raise ValueError("Too many dependency contributions in one CI run")
    if len({artifact["id"] for artifact, *_ in artifacts}) != len(artifacts):
        raise ValueError("Dependency contribution IDs must be unique")
    jobs = {}
    accepted = {}
    for artifact, name, source_attempt in artifacts:
        if source_attempt not in jobs:
            jobs[source_attempt] = api.pages(f"/actions/runs/{identifier}/attempts/{source_attempt}/jobs", field="jobs")
        prepared = [job for job in jobs[source_attempt] if job.get("name") == name
                    and any(step.get("name") == "Prepare dependencies" and step.get("conclusion") == "success"
                            for step in job.get("steps", []))]
        if len(prepared) == 1:
            candidate = (source_attempt, artifact["id"], artifact["size_in_bytes"])
            if name not in accepted or candidate[:2] > accepted[name][:2]:
                accepted[name] = candidate
    if sum(size for _, _, size in accepted.values()) > DOWNLOAD_LIMIT:
        raise ValueError("Prepared dependency artifacts exceed the total download limit")
    return {"allowed": str(bool(accepted)).lower(), "source-run-id": str(identifier),
            "artifact-ids": ",".join(map(str, sorted(identifier for _, identifier, _ in accepted.values()))),
            "reason": "Prepared dependency contributions" if accepted else "No successful dependency preparations"}


def timestamp(value):
    if not isinstance(value, str):
        raise ValueError("Cache creation timestamps must be ISO 8601 strings")
    result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if result.tzinfo is None:
        raise ValueError("Cache creation timestamps must include a timezone")
    return result


def retire(api, event, environment):
    branch = publication_branch(api, event, environment)
    if branch is None:
        raise ValueError("Snapshot retirement requires default-branch publication")
    current = PREFIX + environment["GITHUB_RUN_ID"] + "-" + environment["GITHUB_RUN_ATTEMPT"]
    if not SNAPSHOT.fullmatch(current):
        raise ValueError("Invalid current shared-cache snapshot key")
    caches = api.pages("/actions/caches", field="actions_caches", ref="refs/heads/" + branch, key=PREFIX)
    matching = [cache for cache in caches if cache.get("key") == current
                and cache.get("ref") == "refs/heads/" + branch]
    if len(matching) != 1:
        raise ValueError("The new shared snapshot must be saved before retiring old snapshots")
    saved = matching[0]
    positive(saved.get("id"))
    if not isinstance(saved.get("version"), str) or not saved["version"]:
        raise ValueError("The saved snapshot has no cache version")
    created = timestamp(saved.get("created_at"))
    removed = []
    for cache in caches:
        if (cache.get("ref") == saved["ref"] and cache.get("version") == saved["version"]
                and SNAPSHOT.fullmatch(cache.get("key", "")) and cache["key"] != current
                and timestamp(cache.get("created_at")) <= created):
            removed.append(positive(cache.get("id")))
    for identifier in removed:
        api.request("DELETE", f"/actions/caches/{identifier}", missing_ok=True)
    return {"removed": str(len(removed))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("admit", "retire"))
    args = parser.parse_args()
    api = GitHubApi(os.environ.get("GITHUB_REPOSITORY"), os.environ.get("GITHUB_TOKEN"))
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8"))
    if args.operation == "admit":
        result = admit(api, event, os.environ)
    else:
        result = retire(api, event, os.environ)
    print(json.dumps(result, sort_keys=True))
    if os.environ.get("GITHUB_OUTPUT"):
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
            for name, value in result.items():
                output.write(f"{name}={value}\n")


if __name__ == "__main__":
    main()
