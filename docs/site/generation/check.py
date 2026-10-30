#!/usr/bin/env python3
"""Check generated documentation links, extraction scope and source identity."""

from __future__ import annotations

import argparse
from functools import cache
from html.parser import HTMLParser
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET

# Set the cache location before importing repository-local modules.
if sys.pycache_prefix is None:
    sys.pycache_prefix = str(Path(__file__).resolve().parent / "generated/pycache")
os.environ["PYTHONPYCACHEPREFIX"] = sys.pycache_prefix

from build import GENERATED_ROOT, REPO_ROOT, header_inputs


class Page(HTMLParser):
    def __init__(self, path: Path):
        super().__init__(convert_charrefs=True)
        self.path = path
        self.anchors: set[str] = set()
        self.references: list[str] = []
        self.feed(path.read_text(encoding="utf-8"))

    def handle_starttag(self, tag, attributes):
        for name, value in attributes:
            if not value:
                continue
            if name == "id" or (tag == "a" and name == "name"):
                self.anchors.add(value)
            elif name in {"href", "src", "poster"} or (tag == "form" and name == "action"):
                self.references.append(value)
            elif name == "srcset" and not value.startswith("data:"):
                self.references.extend(item.strip().split()[0] for item in value.split(",") if item.strip())


def check(html: Path, xml: Path, *, require_clean: bool = False) -> dict:
    html, xml = html.resolve(), xml.resolve()
    errors: list[str] = []
    if not (html / "index.html").is_file():
        raise ValueError(f"Missing HTML entry point: {html / 'index.html'}")
    metadata = json.loads((html / "revision.json").read_text(encoding="utf-8"))
    if not isinstance(metadata, dict):
        raise ValueError("Revision metadata must be a JSON object.")
    commit = metadata.get("source_revision", "")
    if not isinstance(commit, str) or not re.fullmatch(r"[0-9a-fA-F]{40}|[0-9a-fA-F]{64}", commit):
        raise ValueError("Revision metadata needs a full Git commit ID.")
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True).strip()
    if commit != head:
        errors.append(f"Published source revision {commit!r} differs from Git HEAD {head}.")
    if not isinstance(metadata.get("dirty"), bool):
        errors.append("Revision metadata needs a boolean dirty marker.")
    elif require_clean and metadata["dirty"]:
        errors.append("Publication requires a clean source checkout.")
    expected = {path.resolve() for path in header_inputs()}
    if metadata.get("header_count") != len(expected):
        errors.append("Revision metadata does not match the public header inventory.")
    index = ET.parse(xml / "index.xml")
    files = set()
    private_members = set()
    for compound in index.findall("compound"):
        identifier = compound.get("refid")
        if not identifier:
            errors.append("An XML compound is missing its reference ID.")
            continue
        declaration = ET.parse(xml / (identifier + ".xml")).find("compounddef")
        if declaration is None:
            errors.append(f"Missing declaration for {compound.get('refid')}.")
            continue
        if declaration.get("prot") == "private":
            errors.append(f"Private compound included in API index: {compound.findtext('name')}")
        if compound.get("kind") == "file":
            location = declaration.find("location")
            if location is None or not location.get("file"):
                errors.append(f"Missing file location for {compound.findtext('name')}.")
            else:
                files.add((REPO_ROOT / location.get("file")).resolve())
        for member in declaration.findall(".//memberdef"):
            if member.get("prot") == "private":
                private_members.add(member.get("id"))
    for missing in sorted(expected - files):
        errors.append(f"Public header missing from XML: {missing.relative_to(REPO_ROOT)}")
    for extra in sorted(files - expected):
        errors.append(f"Unexpected extracted header: {extra}")

    pages = {path.resolve(): Page(path) for path in html.rglob("*.html")}
    names = {}
    for path in pages:
        relative = path.relative_to(html).as_posix()
        key = relative.casefold()
        if key in names:
            errors.append(f"HTML filenames collide on a case-insensitive filesystem: {names[key]} and {relative}")
        names[key] = relative
    references = 0
    source_objects = set()
    source_prefix = f"https://github.com/cppcooper/cheryl-engine/"

    # Shared navigation repeats the same destinations on every page. Output is
    # fixed during this check; scope caches to this invocation so a later check
    # still observes changed or removed files.
    parse_target = cache(urlsplit)

    @cache
    def local_target(parent: Path, target: str) -> tuple[Path, bool, bool]:
        candidate = (parent / unquote(target)).resolve()
        if candidate.is_dir():
            candidate /= "index.html"
        return candidate, candidate.is_relative_to(html), candidate.is_file()

    for path, page in pages.items():
        for private in page.anchors & private_members:
            errors.append(f"Private member rendered in {path.relative_to(html)}: {private}")
        for target in page.references:
            references += 1
            parsed = parse_target(target)
            if target.startswith(source_prefix):
                match = re.fullmatch(r"(blob|tree)/([0-9a-fA-F]{40}|[0-9a-fA-F]{64})(?:/(.*))?", parsed.path.removeprefix("/cppcooper/cheryl-engine/"))
                if match:
                    if match[2].lower() != commit:
                        errors.append(f"Source link uses a different commit: {target}")
                    source_objects.add((match[1], unquote(match[3] or "")))
                elif parsed.path.startswith(("/cppcooper/cheryl-engine/blob/", "/cppcooper/cheryl-engine/tree/")):
                    errors.append(f"Unversioned repository source link: {target}")
            if parsed.scheme or parsed.netloc:
                continue
            if parsed.path.startswith("/"):
                errors.append(f"Root-relative link breaks version/project prefixes: {path.relative_to(html)} -> {target}")
                continue
            candidate, inside, exists = (local_target(path.parent, parsed.path)
                                         if parsed.path else (path, True, True))
            if not inside:
                errors.append(f"Link escapes the HTML tree: {path.relative_to(html)} -> {target}")
            elif not exists:
                errors.append(f"Missing local target: {path.relative_to(html)} -> {target}")
            elif parsed.fragment and candidate.suffix == ".html":
                fragment = unquote(parsed.fragment).split(":~:text=", 1)[0]
                if fragment and fragment not in pages[candidate].anchors:
                    errors.append(f"Missing local fragment: {path.relative_to(html)} -> {target}")
    # Stylesheets also reference fonts and images that are absent from HTML tags.
    for stylesheet in html.rglob("*.css"):
        for target in re.findall(r"url\(\s*['\"]?([^)'\"\s]+)", stylesheet.read_text(encoding="utf-8")):
            parsed = parse_target(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            candidate = (stylesheet.parent / unquote(parsed.path)).resolve()
            if parsed.path.startswith("/") or not candidate.is_relative_to(html) or not candidate.is_file():
                errors.append(f"Missing or prefix-dependent CSS asset: {stylesheet.relative_to(html)} -> {target}")
    for kind, relative in sorted(source_objects):
        result = subprocess.run(["git", "cat-file", "-t", commit + ":" + relative],
                                cwd=REPO_ROOT, capture_output=True, text=True)
        expected_kind = "tree" if kind == "tree" else "blob"
        if result.returncode or result.stdout.strip() != expected_kind:
            errors.append(f"Missing {expected_kind} source object at {commit}: {relative}")
    if errors:
        raise ValueError("\n".join(errors))
    return {"html_pages": len(pages), "references": references,
            "public_headers": len(expected), "source_objects": len(source_objects),
            "source_revision": commit, "dirty": metadata.get("dirty")}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--html", type=Path, default=GENERATED_ROOT / "html")
    parser.add_argument("--xml", type=Path, default=GENERATED_ROOT / "xml")
    parser.add_argument("--require-clean", action="store_true",
                        help="Reject dirty source metadata (automatic on Actions and Read the Docs).")
    args = parser.parse_args()
    try:
        publication = any(os.environ.get(name, "").lower() == "true"
                          for name in ("GITHUB_ACTIONS", "READTHEDOCS"))
        print(json.dumps(check(args.html, args.xml, require_clean=args.require_clean or publication), indent=2))
        return 0
    except (OSError, ValueError, ET.ParseError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Documentation check failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
