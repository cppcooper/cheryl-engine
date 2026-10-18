"""Collect installed Linux native-library notices for a relocatable SDK."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def package_owner(library, *, run=subprocess.run):
    """Ask the installed package database about both a link and its real file."""
    for path in dict.fromkeys((library, library.resolve())):
        for command in (["dpkg-query", "-S", str(path)], ["pacman", "-Qqo", str(path)]):
            try:
                result = run(command, capture_output=True, text=True, check=False)
            except FileNotFoundError:
                continue
            if result.returncode:
                continue
            if command[0] == "dpkg-query":
                owners = {line.partition(": ")[0] for line in result.stdout.splitlines() if ": " in line}
            else:
                owners = set(result.stdout.splitlines())
            owners = {name for name in owners if re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.+:_-]*", name)}
            if len(owners) > 1:
                raise ValueError(f"Ambiguous package ownership for {library.name}: {', '.join(sorted(owners))}")
            if owners:
                return owners.pop()
    return None


def _override_files(directory):
    if not directory.is_dir():
        return []
    files = []
    for path in sorted(directory.iterdir()):
        if re.match(r"(?i)^(copyright$|licen[cs]e|copying)", path.name):
            files.extend([path] if path.is_file() else sorted(child for child in path.rglob("*") if child.is_file()))
    return files


def _provider(library, package, native_prefix):
    if native_prefix is None:
        return None
    share = native_prefix / "share"
    if not share.is_dir():
        return None
    stem = re.sub(r"(?:\.so(?:\..*)?|\.dll|\.dylib)$", "", library.name, flags=re.IGNORECASE).removeprefix("lib")
    candidates = [package.split(":")[0]] if package else []
    candidates.extend((stem, "lib" + stem))
    for name in dict.fromkeys(candidates):
        directory = share / name
        if _override_files(directory):
            return directory
    # Common provider names omit ABI suffixes, e.g. icuuc -> icu,
    # brotlidec -> brotli, or glib-2.0 -> glib2.
    normalized = re.sub(r"[^a-z]", "", stem.lower())
    matches = []
    for directory in sorted(share.iterdir()):
        name = re.sub(r"[^a-z]", "", directory.name.lower().removeprefix("lib"))
        if name and (normalized == name or normalized.startswith(name)) and _override_files(directory):
            matches.append(directory)
    if len(matches) > 1:
        raise ValueError(f"Ambiguous native-prefix notices for {library.name}: {', '.join(p.name for p in matches)}")
    return matches[0] if matches else None


def _references(source, system_root):
    contents = source.read_text(encoding="utf-8", errors="replace")
    references = []
    for name in re.findall(r"/usr/share/common-licenses/([A-Za-z0-9.+_-]+)", contents):
        references.append(system_root / "usr/share/common-licenses" / name.rstrip("."))
    for name in re.findall(
            r"(?i)\b(?:see|file|in|available\s+(?:in|at))\s+(?:the\s+)?(?:file\s+)?[\"'`]?"
            r"((?:\.\.?/)*[A-Za-z0-9_./+-]*(?:COPYING|LICENSE|copyright|LGPL|GPL)[A-Za-z0-9_./+-]*)", contents):
        if not name.startswith("/"):
            references.append(source.parent / name)
    # Git source archives sometimes materialize a license symlink as its
    # relative target text, e.g. COPYING contains LICENSES/LGPL-2.1-or-later.txt.
    for name in re.findall(r"(?mi)^\s*((?:\.\.?/)*[A-Za-z0-9_./+-]*(?:LICENSE|COPYING|LGPL|GPL)"
                           r"[A-Za-z0-9_./+-]*)\s*$", contents):
        if not name.startswith("/"):
            references.append(source.parent / name)
    return [path for path in references if path.is_file()]


def _arch_license_files(package, system_root, *, run):
    try:
        result = run(["pacman", "-Qi", "--", package], capture_output=True, text=True, check=False,
                     env={**os.environ, "LC_ALL": "C"})
    except FileNotFoundError:
        return []
    if result.returncode:
        return []
    match = re.search(r"^Licenses\s*:[ \t]*([^\n]*(?:\n[ \t]+[^\n]*)*)", result.stdout, re.MULTILINE)
    if not match or not re.fullmatch(r"[A-Za-z0-9.+_()\s-]+", match[1]):
        return []
    files = []
    exception = False
    for name in re.findall(r"[A-Za-z0-9][A-Za-z0-9.+_-]*", match[1]):
        if name in {"AND", "OR"}:
            continue
        if name == "WITH":
            exception = True
            continue
        directory = system_root / "usr/share/licenses/spdx"
        if exception:
            directory /= "exceptions"
        path = directory / (name + ".txt")
        if not path.is_file():
            return []
        files.append(path)
        exception = False
    return files if not exception else []


def _notices(library, package, native_prefix, system_root, *, run=subprocess.run):
    provider = _provider(library, package, native_prefix)
    if provider:
        return package or provider.name, provider, _override_files(provider), "native-prefix"
    if package:
        name = package.split(":")[0]
        doc = system_root / "usr/share/doc" / name / "copyright"
        licenses = system_root / "usr/share/licenses" / name
        files = ([doc] if doc.is_file() else [])
        files.extend(sorted(path for path in licenses.rglob("*") if path.is_file()) if licenses.is_dir() else [])
        # Arch keeps standard license texts centrally instead of duplicating
        # them for each package. Include every declared alternative and exception.
        if not files:
            files = _arch_license_files(package, system_root, run=run)
        if files:
            return package, None, files, "installed-package"
    owner = package or "unknown package"
    raise ValueError(
        f"No native notices for {library.name} ({owner}); supply --native-prefix with "
        f"share/{package.split(':')[0] if package else library.name.split('.so')[0].removeprefix('lib')}/"
        "copyright, LICENSE*, or COPYING*")


def collect_notices(libraries, destination, *, native_prefix=None, system_root=Path("/"),
                    run=subprocess.run, query_packages=None):
    """Collect notices, with filesystem/package-query injection for isolated checks."""
    destination, system_root = Path(destination), Path(system_root)
    native_prefix = Path(native_prefix) if native_prefix is not None else None
    query_packages = sys.platform != "win32" if query_packages is None else query_packages
    output = {}
    records, skipped = [], []
    for library in sorted(set(map(Path, libraries)), key=str):
        if not library.is_file():
            raise ValueError(f"Native library is missing: {library.name}")
        if re.match(r"(?i)^(?:lib)?(?:cheryl[._-]|cengine[._-]|"
                    r"cm(?:startup|workers|nativeglfw|opengl|tgui|rmlui|miniaudio|audiominiaudio)[._-]|"
                    r"glfw[\d._-]|tgui[._-]|rmlui[._-])", library.name):
            skipped.append(library.name)
            continue
        provider = _provider(library, None, native_prefix)
        package = provider.name if provider else package_owner(library, run=run) if query_packages else None
        package, provider, seeds, origin = _notices(library, package, native_prefix, system_root, run=run)
        directory = package.split(":")[0]
        pending = [(path, None) for path in seeds]
        seen, notice_names = set(), []
        for path, referenced in pending:
            resolved = path.resolve()
            if resolved in seen:
                continue
            seen.add(resolved)
            if referenced:
                relative = Path(directory) / "references" / path.name
            elif provider:
                relative = Path(directory) / path.relative_to(provider)
            elif path.name == "copyright" and path.parent.parent.name == "doc":
                relative = Path(directory) / "copyright"
            elif path.is_relative_to(system_root / "usr/share/licenses/spdx"):
                relative = Path(directory) / "licenses" / path.relative_to(system_root / "usr/share/licenses/spdx")
            else:
                relative = Path(directory) / "licenses" / path.relative_to(system_root / "usr/share/licenses" / directory)
            data = path.read_bytes()
            if relative in output and output[relative] != data:
                raise ValueError(f"Conflicting native notice contents: {relative.as_posix()}")
            output[relative] = data
            notice_names.append(relative.as_posix())
            pending.extend((child, True) for child in _references(path, system_root))
        records.append({"library": library.name, "package": package, "origin": origin, "notices": sorted(notice_names)})
    # Resolve every library before writing, so a missing notice does not leave
    # behind a partial provenance manifest that looks complete.
    for relative, contents in sorted(output.items()):
        target = destination / relative
        if target.exists() and target.read_bytes() != contents:
            raise ValueError(f"Native notice destination already differs: {relative.as_posix()}")
    for relative, contents in sorted(output.items()):
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(contents)
    provenance = {"schema": 1, "libraries": records, "already_in_sdk": sorted(skipped)}
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "NATIVE-NOTICES.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    return provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--native-prefix", type=Path)
    parser.add_argument("libraries", nargs="+", type=Path)
    args = parser.parse_args()
    try:
        result = collect_notices(args.libraries, args.destination, native_prefix=args.native_prefix)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
