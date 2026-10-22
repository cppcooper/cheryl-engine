"""Assemble release archives and publish an explicitly approved release merge."""

import argparse
from contextlib import contextmanager
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import shutil
import stat
import subprocess
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
    "startup": "VERSION_PROJECT_MODULE_STARTUP",
    "workers": "VERSION_PROJECT_MODULE_WORKERS",
    "native_glfw": "VERSION_PROJECT_MODULE_NATIVE_GLFW",
    "debug_terminal_linux": "VERSION_PROJECT_MODULE_DEBUG_TERMINAL_LINUX",
    "opengl": "VERSION_PROJECT_MODULE_OPENGL",
    "ui_tgui": "VERSION_PROJECT_MODULE_UI_TGUI",
    "ui_rmlui": "VERSION_PROJECT_MODULE_UI_RMLUI",
    "audio_miniaudio": "VERSION_PROJECT_MODULE_AUDIO_MINIAUDIO",
}
REQUIRED_COMPONENTS = set(COMPONENTS) - {"debug_terminal_linux"}
FEATURES = {"startup", "startup_opengl", "workers", "native_glfw", "native_input", "opengl",
            "tgui", "rmlui", "miniaudio", "hwloc", "json"}
MANIFEST_LIMIT = 1024 * 1024
SIDECARS = {"SDK-MANIFESTS.json", "SHA256SUMS"}


def release_names(version):
    return {"Linux": f"cheryl-SDK-v{version}-linux.tar.gz",
            "Windows": f"cheryl-SDK-v{version}-windows.zip",
            "static": f"cheryl-SDK-v{version}-static.tar.gz",
            "source": f"cheryl-src-v{version}.tar.gz"}


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


def _small_file(path):
    if path.stat().st_size > MANIFEST_LIMIT:
        raise RuntimeError("Release metadata exceeds the release metadata limit")
    return path.read_bytes()


def _safe_path(name):
    if (not isinstance(name, str) or not name or "\\" in name or ":" in name
            or any(ord(character) < 32 for character in name)
            or name.startswith("/") or any(part in {"", ".", ".."} for part in name.split("/"))):
        raise RuntimeError("A release archive contains an unsafe path")
    return name


def _archive_root(path):
    suffix = ".zip" if path.name.endswith(".zip") else ".tar.gz"
    return path.name[:-len(suffix)]


def _members(archive):
    return archive.infolist() if isinstance(archive, zipfile.ZipFile) else archive.getmembers()


def _member_name(member):
    return (member.filename if isinstance(member, zipfile.ZipInfo) else member.name).rstrip("/")


def _open_archive(path):
    archive = zipfile.ZipFile(path) if path.name.endswith(".zip") else tarfile.open(path, "r:gz")
    try:
        root, seen, links, folded = _archive_root(path), set(), {}, set()
        for member in _members(archive):
            name = _safe_path(_member_name(member))
            if name != root and not name.startswith(root + "/"):
                raise RuntimeError("A release archive has an unexpected root directory")
            if name in seen:
                raise RuntimeError("A release archive contains duplicate paths")
            seen.add(name)
            if isinstance(member, zipfile.ZipInfo):
                if name.casefold() in folded or any(
                        part.endswith((".", " ")) or re.fullmatch(r"(?i)(?:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?", part)
                        for part in name.split("/")):
                    raise RuntimeError("A release ZIP contains colliding or unsupported Windows paths")
                folded.add(name.casefold())
                mode = stat.S_IFMT(member.external_attr >> 16)
                if mode not in {0, stat.S_IFREG, stat.S_IFDIR} or member.flag_bits & 1:
                    raise RuntimeError("A release ZIP contains an unsupported member")
            elif member.issym():
                target = member.linkname
                if (not target or "\\" in target or ":" in target or target.startswith("/")
                        or any(ord(character) < 32 for character in target)):
                    raise RuntimeError("A release archive contains an unsafe symlink")
                resolved = posixpath.normpath(posixpath.join(posixpath.dirname(name), target))
                if resolved != root and not resolved.startswith(root + "/"):
                    raise RuntimeError("A release archive symlink escapes its root")
                links[name] = resolved
            elif not member.isfile() and not member.isdir():
                raise RuntimeError("A release archive contains an unsupported member")
        for name in seen:
            if any(parent.as_posix() in links for parent in PurePosixPath(name).parents):
                raise RuntimeError("A release archive path traverses a symlink")
        for name in links:
            target, followed = name, set()
            while target in links:
                if target in followed:
                    raise RuntimeError("A release archive contains a cyclic symlink")
                followed.add(target)
                target = links[target]
        return archive
    except BaseException:
        archive.close()
        raise


def _contents(archive, member):
    return archive.open(member) if isinstance(archive, zipfile.ZipFile) else archive.extractfile(member)


def _archive_metadata(path, prefix="", filename="SDK-MANIFEST.json"):
    name = "/".join(filter(None, (_archive_root(path), prefix, filename)))
    with _open_archive(path) as archive:
        members = [member for member in _members(archive) if _member_name(member) == name]
        if (len(members) != 1 or (members[0].is_dir() if isinstance(members[0], zipfile.ZipInfo)
                                 else not members[0].isfile())
                or (members[0].file_size if isinstance(members[0], zipfile.ZipInfo) else members[0].size) > MANIFEST_LIMIT):
            raise RuntimeError("The release archive must contain one bounded manifest at its root")
        with _contents(archive, members[0]) as source:
            return _json(source.read(MANIFEST_LIMIT + 1))


def _sdk_metadata(metadata, expected, revision, versions, *, local_validation=False):
    actual = tuple(metadata.get(field) for field in ("system", "linkage", "configuration", "compiler"))
    matched = (actual == expected if not local_validation else
               actual[:2] == expected[:2] and actual[3] == expected[3] and actual[2] in {"Release", "RelWithDebInfo"})
    if not matched or metadata.get("architecture") not in ("AMD64", "x86_64", "x64"):
        raise RuntimeError("SDK metadata does not match its accepted CI variant")
    if metadata.get("revision") != revision or metadata.get("version") != project_version(versions):
        raise RuntimeError("SDK revision or version differs from the checked release source")
    components = metadata.get("component_versions")
    if not isinstance(components, dict) or not REQUIRED_COMPONENTS.issubset(components):
        raise RuntimeError("SDK metadata is missing required component versions")
    for component, value in components.items():
        variable = COMPONENTS.get(component)
        if variable is None or variable not in versions or value != versions[variable]:
            raise RuntimeError(f"SDK component version differs from CMake: {component}")
    features = metadata.get("features")
    if (not isinstance(features, dict) or set(features) != FEATURES
            or any(value not in ("ON", "OFF") for value in features.values())
            or any(features[name] != "ON" for name in FEATURES - ({"hwloc"} if local_validation else set()))):
        raise RuntimeError("SDK metadata is missing required release features")
    _source_identity({"revision": revision, "submodules": metadata.get("submodules")})


def _source_identity(identity):
    if (not isinstance(identity, dict) or set(identity) != {"revision", "submodules"}
            or not isinstance(identity.get("revision"), str) or not REVISION.fullmatch(identity["revision"])
            or not isinstance(identity.get("submodules"), dict)):
        raise RuntimeError("The release source identity is invalid")
    for path, revision in identity["submodules"].items():
        _safe_path(path)
        if not isinstance(revision, str) or not REVISION.fullmatch(revision):
            raise RuntimeError("The release source submodule revision is invalid")
    return identity


def validate_packages(packages, revision, versions, *, local_validation=False):
    packages = Path(packages)
    project_version(versions)
    if packages.is_symlink() or not packages.is_dir() or {path.name for path in packages.iterdir()} != set(VARIANTS):
        raise RuntimeError("The accepted artifact must contain exactly the four expected SDK variants")
    archives, manifests = {}, {}
    for variant, expected in VARIANTS.items():
        directory = packages / variant
        if directory.is_symlink() or not directory.is_dir():
            raise RuntimeError("An SDK variant must be a real directory")
        checksum, manifest_path = directory / "SHA256SUMS", directory / "SDK-MANIFEST.json"
        _regular(checksum)
        _regular(manifest_path)
        text = _small_file(checksum).decode("utf-8")
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9][A-Za-z0-9._+-]*)(?:\r?\n)?", text)
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
        metadata = _json(_small_file(manifest_path))
        _sdk_metadata(metadata, expected, revision, versions, local_validation=local_validation)
        if _archive_metadata(archive) != metadata:
            raise RuntimeError("The SDK archive and its outer manifest disagree")
        if name in archives:
            raise RuntimeError("SDK release archive names must be unique")
        archives[name] = {"path": archive, "digest": digest}
        manifests[name] = metadata
    return archives, manifests


@contextmanager
def _tar_writer(path):
    with path.open("wb") as destination:
        with gzip.GzipFile(filename="", mode="wb", fileobj=destination, mtime=0, compresslevel=6) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                yield archive


def _tar_member(name, *, size=0, mode=0o644, link=None, directory=False):
    member = tarfile.TarInfo(name)
    member.mode, member.size = mode, size
    if link is not None:
        member.type, member.linkname, member.size = tarfile.SYMTYPE, link, 0
    elif directory:
        member.type = tarfile.DIRTYPE
    return member


def _json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def _tar_bytes(archive, name, data):
    archive.addfile(_tar_member(name, size=len(data)), io.BytesIO(data))


def _append_sdk(destination, path, root):
    original = _archive_root(path)
    with _open_archive(path) as archive:
        for member in sorted(_members(archive), key=_member_name):
            relative = _member_name(member)[len(original):].lstrip("/")
            if not relative:
                continue
            name = root + "/" + relative
            if isinstance(member, zipfile.ZipInfo):
                copied = _tar_member(name, size=member.file_size, directory=member.is_dir(),
                                     mode=0o755 if member.is_dir() else 0o644)
            else:
                copied = _tar_member(name, size=member.size, mode=member.mode & 0o777,
                                     link=member.linkname if member.issym() else None,
                                     directory=member.isdir())
            if copied.isfile():
                with _contents(archive, member) as contents:
                    destination.addfile(copied, contents)
            else:
                destination.addfile(copied)


def _shared_archive(source, destination):
    if source.name == destination.name:
        shutil.copyfile(source, destination)
        return
    root = _archive_root(destination)
    if destination.name.endswith(".tar.gz"):
        with _tar_writer(destination) as archive:
            _append_sdk(archive, source, root)
        return
    original = _archive_root(source)
    with _open_archive(source) as archive, zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED) as output:
        for member in sorted(_members(archive), key=_member_name):
            relative = _member_name(member)[len(original):].lstrip("/")
            if not relative:
                continue
            name = root + "/" + relative + ("/" if member.is_dir() else "")
            copied = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            copied.compress_type = zipfile.ZIP_DEFLATED
            copied.external_attr = ((stat.S_IFDIR | 0o755) if member.is_dir() else (stat.S_IFREG | 0o644)) << 16
            with output.open(copied, "w") as target:
                if not member.is_dir():
                    with _contents(archive, member) as contents:
                        shutil.copyfileobj(contents, target)


def _git(repo, *arguments):
    try:
        return subprocess.check_output(["git", "-C", str(repo), *arguments], stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"Cannot read the pinned source repository: {repo}") from error


def source_snapshot(source, revision, destination):
    """Read pinned Git objects; generated and untracked working files never enter the archive."""
    source = Path(source)
    if not isinstance(revision, str) or not REVISION.fullmatch(revision):
        raise RuntimeError("A source snapshot requires a full checked commit ID")
    repositories, pins = [], {}

    def collect(repo, commit, prefix=""):
        if repo.is_symlink() or _git(repo, "rev-parse", "HEAD").decode().strip() != commit:
            raise RuntimeError(f"Source checkout does not match its pinned revision: {prefix or '.'}")
        entries = []
        for entry in filter(None, _git(repo, "ls-tree", "-rz", commit).split(b"\0")):
            metadata, raw_name = entry.split(b"\t", 1)
            mode, kind, sha = metadata.decode().split()
            name = _safe_path(raw_name.decode("utf-8"))
            if not prefix and name == "SOURCE-MANIFEST.json":
                raise RuntimeError("The source tree reserves SOURCE-MANIFEST.json for snapshot identity")
            if mode == "160000" and kind == "commit":
                pins[prefix + name] = sha
                collect(repo / name, sha, prefix + name + "/")
            elif kind == "blob" and mode in {"100644", "100755", "120000"}:
                entries.append((prefix + name, mode, sha))
            else:
                raise RuntimeError("The pinned source contains an unsupported Git tree entry")
        repositories.append((repo, entries))

    collect(source, revision)
    identity = _source_identity({"revision": revision, "submodules": pins})
    root = _archive_root(destination)
    with _tar_writer(destination) as archive:
        for repo, entries in repositories:
            with subprocess.Popen(["git", "-C", str(repo), "cat-file", "--batch"],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as objects:
                try:
                    for name, mode, sha in entries:
                        objects.stdin.write((sha + "\n").encode("ascii"))
                        objects.stdin.flush()
                        header = objects.stdout.readline().decode("ascii").strip().split()
                        if len(header) != 3 or header[:2] != [sha, "blob"] or not header[2].isdigit():
                            raise RuntimeError("Git did not return the requested source blob")
                        size = int(header[2])
                        if mode == "120000":
                            if size > MANIFEST_LIMIT:
                                raise RuntimeError("A source symlink is too large")
                            link = objects.stdout.read(size).decode("utf-8")
                            archive.addfile(_tar_member(root + "/" + name, link=link, mode=0o777))
                        else:
                            archive.addfile(_tar_member(root + "/" + name, size=size,
                                                       mode=0o755 if mode == "100755" else 0o644), objects.stdout)
                        if objects.stdout.read(1) != b"\n":
                            raise RuntimeError("Git returned a truncated source blob")
                except BaseException:
                    objects.kill()
                    raise
                finally:
                    objects.stdin.close()
                if objects.wait() != 0:
                    raise RuntimeError("Git could not finish reading the pinned source")
        _tar_bytes(archive, root + "/SOURCE-MANIFEST.json", _json_bytes(identity))
    # Apply the same traversal and link rules to source archives as binary SDKs.
    with _open_archive(destination):
        pass
    return identity


def _static_dispatcher(filename):
    missing = ('set(Cheryl_FOUND FALSE)\n  set(Cheryl_NOT_FOUND_MESSAGE "This SDK supports Linux and Windows")'
               if filename == "CherylConfig.cmake" else "set(PACKAGE_VERSION_UNSUITABLE TRUE)")
    return ("if(CMAKE_SYSTEM_NAME STREQUAL \"Linux\")\n"
            "  set(_cheryl_platform linux)\n"
            "elseif(CMAKE_SYSTEM_NAME STREQUAL \"Windows\")\n"
            "  set(_cheryl_platform windows)\n"
            f"else()\n  {missing}\n  return()\nendif()\n"
            f'include("${{CMAKE_CURRENT_LIST_DIR}}/../../../${{_cheryl_platform}}/lib/cmake/Cheryl/{filename}")\n'
            "unset(_cheryl_platform)\n").encode("utf-8")


def assemble_release(packages, source, output, revision, versions, *, local_validation=False):
    archives, manifests = validate_packages(packages, revision, versions, local_validation=local_validation)
    version, output = project_version(versions), Path(output)
    names = release_names(version)
    if output.is_symlink() or (output.exists() and (not output.is_dir() or any(output.iterdir()))):
        raise RuntimeError("Release assembly needs an empty real output directory")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="cheryl-assembly-", dir=output.parent) as temporary:
        directory = Path(temporary)
        identity = source_snapshot(source, revision, directory / names["source"])
        sdks = {}
        with _tar_writer(directory / names["static"]) as static_archive:
            for variant, expected in VARIANTS.items():
                original = next(name for name, manifest in manifests.items()
                                if (manifest["system"], manifest["linkage"]) == expected[:2])
                metadata = manifests[original]
                if metadata["submodules"] != identity["submodules"]:
                    raise RuntimeError("An SDK's submodule pins differ from the release source snapshot")
                prefix = expected[0].lower() if expected[1] == "static" else ""
                final_name = names["static"] if prefix else names[expected[0]]
                if prefix:
                    _append_sdk(static_archive, archives[original]["path"], _archive_root(directory / final_name) + "/" + prefix)
                else:
                    _shared_archive(archives[original]["path"], directory / final_name)
                sdks[variant] = {"archive": final_name, "prefix": prefix, "manifest": metadata}
            for filename in ("CherylConfig.cmake", "CherylConfigVersion.cmake"):
                _tar_bytes(static_archive, _archive_root(directory / names["static"]) + "/lib/cmake/Cheryl/" + filename,
                           _static_dispatcher(filename))
        aggregate = {"schema": 2, "validation": "local" if local_validation else "ci",
                     "version": version, "revision": revision, "sdks": sdks,
                     "source": {"archive": names["source"], "manifest": identity}}
        (directory / "SDK-MANIFESTS.json").write_bytes(_json_bytes(aggregate))
        assets = {path.name: {"path": path, "digest": _digest(path)} for path in directory.iterdir()}
        (directory / "SHA256SUMS").write_text("".join(f"{value['digest']}  {name}\n" for name, value in sorted(assets.items())),
                                              encoding="utf-8")
        validate_release(directory, revision, versions, local_validation=local_validation)
        for path in directory.iterdir():
            path.rename(output / path.name)
    return output


def validate_release(packages, revision, versions, *, local_validation=False):
    packages, version = Path(packages), project_version(versions)
    names = release_names(version)
    expected = set(names.values()) | SIDECARS
    if packages.is_symlink() or not packages.is_dir() or {path.name for path in packages.iterdir()} != expected:
        raise RuntimeError("The release must contain exactly four named archives and two audit sidecars")
    for name in expected:
        _regular(packages / name)
    checksums = {}
    for line in _small_file(packages / "SHA256SUMS").decode("utf-8").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9][A-Za-z0-9._+-]*)", line)
        if not match or match[2] in checksums:
            raise RuntimeError("The release checksum file contains invalid or duplicate entries")
        checksums[match[2]] = match[1]
    if set(checksums) != expected - {"SHA256SUMS"}:
        raise RuntimeError("The release checksums must cover all four archives and SDK metadata")
    assets = {}
    for name in expected:
        digest = _digest(packages / name)
        if name != "SHA256SUMS" and digest != checksums[name]:
            raise RuntimeError(f"Release checksum mismatch: {name}")
        assets[name] = {"path": packages / name, "digest": digest}
    aggregate = _json(_small_file(packages / "SDK-MANIFESTS.json"))
    if (aggregate.get("schema") != 2 or aggregate.get("revision") != revision or aggregate.get("version") != version
            or not isinstance(aggregate.get("sdks"), dict) or set(aggregate["sdks"]) != set(VARIANTS)):
        raise RuntimeError("The assembled SDK metadata differs from the checked release source")
    if aggregate.get("validation") != ("local" if local_validation else "ci"):
        raise RuntimeError("Local validation archives cannot be published as CI release assets")
    source = aggregate.get("source")
    if not isinstance(source, dict) or set(source) != {"archive", "manifest"} or source["archive"] != names["source"]:
        raise RuntimeError("The assembled source metadata is invalid")
    identity = _source_identity(source["manifest"])
    if identity["revision"] != revision or _archive_metadata(packages / names["source"], filename="SOURCE-MANIFEST.json") != identity:
        raise RuntimeError("The source archive and its release identity disagree")
    for variant, expected_variant in VARIANTS.items():
        sdk = aggregate["sdks"][variant]
        prefix = expected_variant[0].lower() if expected_variant[1] == "static" else ""
        name = names["static"] if prefix else names[expected_variant[0]]
        if (not isinstance(sdk, dict) or set(sdk) != {"archive", "prefix", "manifest"}
                or sdk["archive"] != name or sdk["prefix"] != prefix or not isinstance(sdk["manifest"], dict)):
            raise RuntimeError("An assembled SDK has an unexpected archive or platform prefix")
        metadata = sdk["manifest"]
        _sdk_metadata(metadata, expected_variant, revision, versions, local_validation=local_validation)
        if metadata["submodules"] != identity["submodules"]:
            raise RuntimeError("An SDK's submodule pins differ from the release source snapshot")
        if _archive_metadata(packages / name, prefix) != metadata:
            raise RuntimeError("The SDK archive and its outer manifest disagree")
    return assets, aggregate


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
    assets, _ = validate_release(packages, revision, versions)
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
    parser.add_argument("--assemble-only", action="store_true")
    parser.add_argument("--source", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--local-validation", action="store_true")
    args = parser.parse_args(argv)
    if args.assemble_only:
        if args.output is None or args.revision is None:
            parser.error("--assemble-only requires --output and --revision")
        from versions import read_versions
        versions = read_versions(_git(args.source, "show", args.revision + ":cmake/CherylVersions.cmake").decode("utf-8"))
        assemble_release(args.packages, args.source, args.output, args.revision, versions,
                         local_validation=args.local_validation)
        print(f"Assembled four release archives in {args.output}")
        return 0
    if args.output is not None or args.local_validation:
        parser.error("--output and --local-validation are only valid with --assemble-only")
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
