"""Publish checked SDKs after an explicit develop-to-main release merge."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import tarfile
import tempfile
import time
import zipfile


REPOSITORY = "cppcooper/cheryl-engine"
ROOT = Path(__file__).resolve().parents[2]
REVISION = re.compile(r"[0-9a-f]{40}")
VERSION = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)")
VARIANTS = {
    "candidate-sdk-linux-modules": ("Linux", "static", "Release", "GNU"),
    "candidate-sdk-linux-modules-shared": ("Linux", "shared", "Release", "GNU"),
    "candidate-sdk-windows-modules": ("Windows", "static", "RelWithDebInfo", "MSVC"),
    "candidate-sdk-windows-modules-shared": ("Windows", "shared", "RelWithDebInfo", "MSVC"),
}
COMPONENTS = {
    "engine": "VERSION_PROJECT_ENGINE",
    "startup": "VERSION_PROJECT_ENGINE",
    "native_glfw": "VERSION_PROJECT_MODULE_NATIVE_GLFW",
    "debug_terminal_linux": "VERSION_PROJECT_MODULE_DEBUG_TERMINAL_LINUX",
    "opengl": "VERSION_PROJECT_MODULE_OPENGL",
    "opengl_startup": "VERSION_PROJECT_MODULE_OPENGL",
    "ui_tgui": "VERSION_PROJECT_MODULE_UI_TGUI",
    "ui_rmlui": "VERSION_PROJECT_MODULE_UI_RMLUI",
    "audio_miniaudio": "VERSION_PROJECT_MODULE_AUDIO_MINIAUDIO",
    "worker_topology_hwloc": "VERSION_PROJECT_ENGINE",
    "worker_topology_json": "VERSION_PROJECT_ENGINE",
}
REQUIRED_COMPONENTS = set(COMPONENTS) - {"debug_terminal_linux", "worker_topology_json"}
MANIFEST_LIMIT = 1024 * 1024


def publication_revision(environment, requested=None):
    required = {
        "GITHUB_ACTIONS": "true", "GITHUB_REPOSITORY": REPOSITORY,
        "GITHUB_EVENT_NAME": "push", "GITHUB_REF": "refs/heads/main",
    }
    if any(environment.get(name) != value for name, value in required.items()):
        return None
    revision = environment.get("GITHUB_SHA", "")
    if not isinstance(revision, str) or not REVISION.fullmatch(revision):
        raise RuntimeError("A release requires the full checked main commit ID")
    if requested is not None and requested != revision:
        raise RuntimeError("The requested release revision differs from the checked main push")
    return revision


def project_version(versions):
    version = versions.get("VERSION_PROJECT_CHERYL")
    if not isinstance(version, str) or not VERSION.fullmatch(version) or version.rsplit(".", 1)[1] != "0":
        raise RuntimeError("A stable release needs an explicit x.y.0 Cheryl project version in CMake")
    return version


def release_pull(api, revision, version):
    matches = []
    for pull in api.pages(f"/commits/{revision}/pulls"):
        if not isinstance(pull, dict):
            raise RuntimeError("GitHub returned invalid associated pull request data")
        head, base = pull.get("head") or {}, pull.get("base") or {}
        if (base.get("ref") != "main" or head.get("ref") != "develop"
                or (head.get("repo") or {}).get("full_name") != REPOSITORY
                or (base.get("repo") or {}).get("full_name") != REPOSITORY
                or pull.get("state") != "closed" or pull.get("merge_commit_sha") != revision
                or not pull.get("merged_at")):
            continue
        title = pull.get("title", "")
        if not isinstance(title, str) or not re.fullmatch(r"Release " + VERSION.pattern, title):
            continue
        if title != f"Release {version}":
            raise RuntimeError("The merged release PR title does not match the CMake project version")
        matches.append(pull)
    if not matches:
        return None
    if len(matches) != 1:
        raise RuntimeError("The main commit has more than one matching release PR")
    pull = matches[0]
    commit = api.request("GET", f"/commits/{revision}")
    parents = commit.get("parents") if isinstance(commit, dict) else None
    head = pull["head"].get("sha")
    if (not isinstance(commit, dict) or commit.get("sha") != revision
            or not isinstance(parents, list) or len(parents) != 2
            or not isinstance(head, str) or not REVISION.fullmatch(head)
            or any(not isinstance(parent, dict) or not isinstance(parent.get("sha"), str)
                   or not REVISION.fullmatch(parent["sha"]) for parent in parents)
            or parents[1].get("sha") != head):
        raise RuntimeError("A release must be an ordinary merge with the develop PR head as its second parent")
    return pull


def current_main(api, revision):
    response = api.request("GET", "/git/ref/heads/main")
    value = response.get("object") if isinstance(response, dict) else None
    if (not isinstance(response, dict) or response.get("ref") != "refs/heads/main"
            or not isinstance(value, dict) or value.get("type") != "commit"
            or not isinstance(value.get("sha"), str) or not REVISION.fullmatch(value["sha"])):
        raise RuntimeError("GitHub did not return the canonical main commit")
    return value["sha"] == revision


def wait_for_documentation(api, revision, *, sleep=time.sleep, monotonic=time.monotonic, timeout=600):
    deadline = monotonic() + timeout
    while True:
        runs = [run for run in api.pages("/actions/workflows/documentation.yml/runs", field="workflow_runs",
                                        branch="main", event="push", head_sha=revision)
                if isinstance(run, dict) and run.get("head_sha") == revision
                and run.get("head_branch") == "main" and run.get("event") == "push"
                and (run.get("head_repository") or {}).get("full_name") == REPOSITORY]
        if runs:
            latest = max(runs, key=lambda run: run.get("id", 0))
            identifier = latest.get("id")
            if type(identifier) is not int or identifier <= 0:
                raise RuntimeError("The documentation workflow run has an invalid ID")
            # GitHub's default latest-job filter excludes prior attempts.
            jobs = [job for job in api.pages(f"/actions/runs/{identifier}/jobs", field="jobs", filter="latest")
                    if isinstance(job, dict) and job.get("name") == "Documentation checks"]
            if len(jobs) > 1:
                raise RuntimeError("The latest documentation attempt has ambiguous check jobs")
            if jobs and jobs[0].get("status") == "completed":
                if jobs[0].get("conclusion") == "success":
                    return
                raise RuntimeError("Documentation checks did not succeed for the release commit")
            if not jobs and latest.get("status") == "completed":
                raise RuntimeError("The documentation workflow completed without its required check job")
        remaining = deadline - monotonic()
        if remaining <= 0:
            raise RuntimeError("Timed out waiting for documentation checks on the release commit")
        sleep(min(15, remaining))


def _regular(path):
    if not stat.S_ISREG(path.lstat().st_mode):
        raise RuntimeError(f"Expected a regular release input: {path.name}")


def _digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def _json(contents):
    if len(contents) > MANIFEST_LIMIT:
        raise RuntimeError("SDK manifest exceeds the release metadata limit")
    value = json.loads(contents)
    if not isinstance(value, dict):
        raise RuntimeError("SDK metadata must be a JSON object")
    return value


def _archive_metadata(path):
    suffix = ".zip" if path.name.endswith(".zip") else ".tar.gz"
    name = path.name[:-len(suffix)] + "/SDK-MANIFEST.json"
    if suffix == ".zip":
        with zipfile.ZipFile(path) as archive:
            members = [member for member in archive.infolist() if member.filename == name]
            if len(members) != 1 or members[0].is_dir() or members[0].file_size > MANIFEST_LIMIT:
                raise RuntimeError("The SDK archive must contain one bounded manifest at its root")
            return _json(archive.read(members[0]))
    with tarfile.open(path, "r:gz") as archive:
        members = [member for member in archive.getmembers() if member.name == name]
        if len(members) != 1 or not members[0].isfile() or members[0].size > MANIFEST_LIMIT:
            raise RuntimeError("The SDK archive must contain one bounded manifest at its root")
        with archive.extractfile(members[0]) as source:
            return _json(source.read(MANIFEST_LIMIT + 1))


def validate_packages(packages, revision, versions):
    packages = Path(packages)
    version = project_version(versions)
    if {path.name for path in packages.iterdir()} != set(VARIANTS):
        raise RuntimeError("The accepted artifact must contain exactly the four expected SDK variants")
    archives, manifests = {}, {}
    for variant, expected in VARIANTS.items():
        directory = packages / variant
        if directory.is_symlink() or not directory.is_dir():
            raise RuntimeError("An SDK variant must be a real directory")
        checksum, manifest_path = directory / "SHA256SUMS", directory / "SDK-MANIFEST.json"
        _regular(checksum)
        _regular(manifest_path)
        text = checksum.read_text(encoding="utf-8")
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9][A-Za-z0-9._+-]*)\n?", text)
        if not match:
            raise RuntimeError("Each SDK checksum file must name one safe archive basename")
        digest, name = match.groups()
        archive = directory / name
        suffix = ".zip" if expected[0] == "Windows" else ".tar.gz"
        if not name.endswith(suffix) or {path.name for path in directory.iterdir()} != {name, "SHA256SUMS", "SDK-MANIFEST.json"}:
            raise RuntimeError("An SDK bundle has unexpected files or archive format")
        _regular(archive)
        if _digest(archive) != digest:
            raise RuntimeError(f"SDK checksum mismatch: {name}")
        metadata = _json(manifest_path.read_bytes())
        actual = tuple(metadata.get(field) for field in ("system", "linkage", "configuration", "compiler"))
        if actual != expected or metadata.get("architecture") not in {"AMD64", "x86_64", "x64"}:
            raise RuntimeError("SDK metadata does not match its accepted CI variant")
        if metadata.get("revision") != revision or metadata.get("version") != version:
            raise RuntimeError("SDK revision or version differs from the checked release source")
        components = metadata.get("component_versions")
        if not isinstance(components, dict) or not REQUIRED_COMPONENTS.issubset(components):
            raise RuntimeError("SDK metadata is missing required component versions")
        for component, value in components.items():
            variable = COMPONENTS.get(component)
            if variable is None or variable not in versions or value != versions[variable]:
                raise RuntimeError(f"SDK component version differs from CMake: {component}")
        if _archive_metadata(archive) != metadata:
            raise RuntimeError("The SDK archive and its outer manifest disagree")
        if name in archives:
            raise RuntimeError("SDK release archive names must be unique")
        archives[name] = {"path": archive, "digest": digest}
        manifests[name] = metadata
    return archives, manifests


def _tag_revision(api, tag):
    response = api.request("GET", f"/git/ref/tags/{tag}", missing_ok=True)
    if response is None:
        return None
    if not isinstance(response, dict) or response.get("ref") != "refs/tags/" + tag:
        raise RuntimeError("GitHub returned an unexpected release tag")
    value = response.get("object")
    seen = set()
    for _ in range(8):
        if (not isinstance(value, dict) or not isinstance(value.get("sha"), str)
                or not REVISION.fullmatch(value["sha"]) or value["sha"] in seen):
            raise RuntimeError("The release tag has an invalid target")
        if value.get("type") == "commit":
            return value["sha"]
        if value.get("type") != "tag":
            raise RuntimeError("The release tag does not identify a commit")
        seen.add(value["sha"])
        annotated = api.request("GET", f"/git/tags/{value['sha']}")
        value = annotated.get("object") if isinstance(annotated, dict) else None
    raise RuntimeError("The release tag has too many nested annotated tags")


def _release(api, tag):
    matches = [release for release in api.pages("/releases")
               if isinstance(release, dict) and release.get("tag_name") == tag]
    if len(matches) > 1:
        raise RuntimeError("GitHub returned multiple releases for one tag")
    return matches[0] if matches else None


def _release_id(release):
    identifier = release.get("id") if isinstance(release, dict) else None
    if type(identifier) is not int or identifier <= 0:
        raise RuntimeError("GitHub returned an invalid release ID")
    return identifier


def _check_asset(asset, name, expected):
    if (not isinstance(asset, dict) or asset.get("name") != name or asset.get("state") != "uploaded"
            or asset.get("digest") != "sha256:" + expected["digest"]
            or asset.get("size") != expected["path"].stat().st_size):
        raise RuntimeError(f"An existing release asset differs from the checked SDK: {name}")


def _empty_starter(asset):
    return (isinstance(asset, dict) and asset.get("state") == "starter"
            and type(asset.get("id")) is int and asset["id"] > 0
            and type(asset.get("size")) is int and asset["size"] == 0)


def _assets(api, release, expected, *, allow_starters=False):
    assets = {}
    for asset in api.pages(f"/releases/{_release_id(release)}/assets"):
        name = asset.get("name") if isinstance(asset, dict) else None
        if name not in expected or name in assets:
            raise RuntimeError("The release contains unexpected or duplicate assets")
        if not (allow_starters and release.get("draft") is True and _empty_starter(asset)):
            _check_asset(asset, name, expected[name])
        assets[name] = asset
    return assets


def publish_release(api, packages, revision, versions, *, sleep=time.sleep, monotonic=time.monotonic, docs_timeout=600):
    if api.repository != REPOSITORY or not isinstance(revision, str) or not REVISION.fullmatch(revision):
        raise RuntimeError("Releases require the canonical repository and a full checked commit ID")
    version = project_version(versions)
    if release_pull(api, revision, version) is None:
        return "not-release"
    if not current_main(api, revision):
        return "stale"
    archives, manifests = validate_packages(packages, revision, versions)
    wait_for_documentation(api, revision, sleep=sleep, monotonic=monotonic, timeout=docs_timeout)
    tag = "v" + version
    target = _tag_revision(api, tag)
    if target is not None and target != revision:
        raise RuntimeError("The release tag already identifies a different commit; it will not be moved")
    release = _release(api, tag)
    if release is not None and type(release.get("draft")) is not bool:
        raise RuntimeError("GitHub returned an invalid release publication state")
    if release is not None and release.get("prerelease") is not False:
        raise RuntimeError("An existing release conflicts with the stable prerelease setting")
    with tempfile.TemporaryDirectory(prefix="cheryl-release-") as temporary:
        directory = Path(temporary)
        aggregate = directory / "SDK-MANIFESTS.json"
        aggregate.write_text(json.dumps({"version": version, "revision": revision, "sdks": manifests},
                                        indent=2, sort_keys=True) + "\n", encoding="utf-8")
        assets = {**archives, aggregate.name: {"path": aggregate, "digest": _digest(aggregate)}}
        sums = directory / "SHA256SUMS"
        sums.write_text("".join(f"{value['digest']}  {name}\n" for name, value in sorted(assets.items())), encoding="utf-8")
        assets[sums.name] = {"path": sums, "digest": _digest(sums)}
        existing = _assets(api, release, assets, allow_starters=True) if release is not None else {}
        if release is not None and release.get("draft") is not True:
            if target != revision or set(existing) != set(assets):
                raise RuntimeError("The published release does not match the complete checked SDK set")
            return "unchanged"
        if not current_main(api, revision):
            return "stale"
        if target is None:
            created = api.request("POST", "/git/refs", {"ref": "refs/tags/" + tag, "sha": revision})
            if (not isinstance(created, dict) or created.get("ref") != "refs/tags/" + tag
                    or (created.get("object") or {}).get("sha") != revision
                    or (created.get("object") or {}).get("type") != "commit"):
                raise RuntimeError("GitHub did not confirm the release tag at the accepted commit")
        if release is None:
            if not current_main(api, revision):
                return "stale"
            release = api.request("POST", "/releases", {"tag_name": tag, "target_commitish": revision,
                                  "name": f"Cheryl {version}", "draft": True, "prerelease": False,
                                  "generate_release_notes": True})
            _release_id(release)
            if (release.get("tag_name") != tag or release.get("draft") is not True
                    or release.get("prerelease") is not False):
                raise RuntimeError("GitHub did not create the expected draft release")
        for name, value in sorted(assets.items()):
            asset = existing.get(name)
            if asset is not None and asset.get("state") == "uploaded":
                continue
            if asset is not None:
                # Failed uploads can leave a zero-byte starter. Recheck its
                # current state before removing only that unfinished draft asset.
                latest_release = api.request("GET", f"/releases/{_release_id(release)}")
                if (not isinstance(latest_release, dict) or latest_release.get("id") != release["id"]
                        or latest_release.get("tag_name") != tag or latest_release.get("draft") is not True
                        or latest_release.get("prerelease") is not False):
                    raise RuntimeError("The release is no longer the matching stable draft")
                latest_asset = api.request("GET", f"/releases/assets/{asset['id']}")
                if (not _empty_starter(latest_asset) or latest_asset.get("id") != asset["id"]
                        or latest_asset.get("name") != name):
                    raise RuntimeError("The unfinished release asset changed; it will not be deleted")
                if _tag_revision(api, tag) != revision:
                    raise RuntimeError("The release tag changed before retrying an upload")
                if not current_main(api, revision):
                    return "stale"
                if api.request("DELETE", f"/releases/assets/{asset['id']}") is not None:
                    raise RuntimeError("GitHub did not confirm removal of the unfinished upload")
            if not current_main(api, revision):
                return "stale"
            _check_asset(api.upload(release["upload_url"], value["path"]), name, value)
        if set(_assets(api, release, assets)) != set(assets):
            raise RuntimeError("The draft release is missing checked assets")
        latest_release = api.request("GET", f"/releases/{_release_id(release)}")
        if (not isinstance(latest_release, dict) or latest_release.get("id") != release["id"]
                or latest_release.get("tag_name") != tag or latest_release.get("draft") is not True
                or latest_release.get("prerelease") is not False):
            raise RuntimeError("The release is no longer the matching stable draft")
        if not current_main(api, revision):
            return "stale"
        if _tag_revision(api, tag) != revision:
            raise RuntimeError("The release tag changed before publication")
        result = api.request("PATCH", f"/releases/{_release_id(release)}", {"draft": False, "make_latest": "true"})
        if (not isinstance(result, dict) or result.get("id") != release["id"]
                or result.get("tag_name") != tag or result.get("draft") is not False
                or result.get("prerelease") is not False):
            raise RuntimeError("GitHub did not confirm release publication")
    return "published"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--revision")
    args = parser.parse_args(argv)
    revision = publication_revision(os.environ, args.revision)
    if revision is None:
        print("Release publication is disabled outside canonical main pushes.")
        return 0
    from github_api import GitHubApi
    from versions import read_versions
    versions = read_versions((ROOT / "cmake/CherylVersions.cmake").read_text(encoding="utf-8"))
    result = publish_release(GitHubApi(REPOSITORY, os.environ.get("GITHUB_TOKEN", "")), args.packages, revision, versions)
    print(f"Release publication: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
