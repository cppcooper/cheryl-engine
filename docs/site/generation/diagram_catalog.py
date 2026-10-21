"""Validate curated Mermaid metadata and compile relocatable explorer assets."""

from __future__ import annotations

from collections import defaultdict
from copy import deepcopy
import hashlib
from html import unescape
import json
from pathlib import Path
import re

IDENTIFIER = re.compile(r"[a-z][a-z0-9-]*")


def frame_version(directory: Path) -> str:
    content = b"".join(
        (directory / name).read_bytes()
        for name in ("frame.html", "explorer.css", "explorer.mjs", "shell.js")
    )
    return hashlib.sha256(content).hexdigest()[:12]


def version_frame_assets(directory: Path) -> None:
    """Version the iframe's own assets as Sphinx does for the parent page."""
    frame = directory / "frame.html"
    text = frame.read_text(encoding="utf-8")
    for name in ("explorer.css", "explorer.mjs"):
        version = hashlib.sha256((directory / name).read_bytes()).hexdigest()[:12]
        text = re.sub(re.escape(name) + r"(?:\?v=[a-f0-9]+)?", name + "?v=" + version, text)
    frame.write_text(text, encoding="utf-8")


def load_catalog(source: Path) -> dict:
    catalog = json.loads((source / "catalog.json").read_text(encoding="utf-8"))
    if catalog.get("schema_version") != 1:
        raise ValueError("Unsupported Engine diagram catalog schema")
    seen = set()
    symbols = catalog["symbols"]
    for diagram in catalog["diagrams"]:
        identity = diagram["id"]
        if not IDENTIFIER.fullmatch(identity) or identity in seen:
            raise ValueError(f"Invalid or duplicate Engine diagram ID: {identity}")
        seen.add(identity)
        keys = diagram["nodes"]
        if len(set(keys)) != len(keys) or any(key not in symbols for key in keys):
            raise ValueError(f"Unknown or duplicate node in Engine diagram {identity}")
        for edge in diagram["edges"]:
            if edge["source"] not in keys or edge["target"] not in keys:
                raise ValueError(f"Unknown edge endpoint in Engine diagram {identity}")
        text = (source / (identity + ".mmd")).read_text(encoding="utf-8")
        declared = [int(value) for value in re.findall(r"^\s*k(\d+)[\[(]", text, re.MULTILINE)]
        if declared != list(range(len(keys))):
            raise ValueError(f"Engine diagram {identity} node order differs from its catalog")
        edges = [
            (int(start), int(end), arrow == "-.->")
            for start, arrow, end in re.findall(
                r"^\s*k(\d+)\s+(-->|-\.->)\s+k(\d+)\s*$", text, re.MULTILINE
            )
        ]
        expected = [
            (keys.index(edge["source"]), keys.index(edge["target"]), bool(edge.get("optional")))
            for edge in diagram["edges"]
        ]
        if edges != expected:
            raise ValueError(f"Engine diagram {identity} arrows differ from its catalog")
        if re.search(r"--?>\s*\||--\s+uses\b", text):
            raise ValueError(f"Engine diagram {identity} must not label dependency arrows")
        labels = dict(re.findall(r'^\s*k(\d+)[\[(].*?"(.*?)"', text, re.MULTILINE))
        for key, operations in diagram.get("operations", {}).items():
            if key not in keys or symbols[key].get("kind") in {"role", "system"}:
                raise ValueError(f"Unknown operation owner in Engine diagram {identity}: {key}")
            names = set()
            for operation in operations:
                name = operation["name"]
                if name in names or not name.startswith("CE::") or operation["role"] not in {"call", "override"}:
                    raise ValueError(f"Invalid or duplicate application operation in {identity}: {name}")
                names.add(name)
                path = Path(operation["file"])
                if path.is_absolute() or ".." in path.parts or operation["line"] < 1:
                    raise ValueError(f"Invalid operation source in {identity}: {name}")
                if operation["label"] not in unescape(labels[str(keys.index(key))]):
                    raise ValueError(f"Engine diagram {identity} omits operation label: {name}")
    for symbol in symbols.values():
        if symbol["label"].startswith("CE::") or "std::" in symbol["label"]:
            raise ValueError(f"Unqualified Engine diagram label policy: {symbol['label']}")
    return catalog


def api_targets(objects) -> dict[str, tuple[str, str]]:
    """Resolve exact names only; overloads and ambiguous aliases stay source links."""
    candidates = defaultdict(set)
    for name, _, kind, document, anchor, _ in objects:
        if kind in {"class", "struct", "union", "enum", "type", "concept", "function"}:
            candidates[re.sub(r"\s+", "", name)].add((document, anchor))
    return {name: next(iter(targets)) for name, targets in candidates.items() if len(targets) == 1}


def write_catalog(source: Path, destination: Path, objects, get_target_uri, source_repository: str) -> None:
    catalog = load_catalog(source)
    targets = api_targets(objects)
    output = deepcopy(catalog)
    for diagram in output["diagrams"]:
        diagram["source"] = (source / (diagram["id"] + ".mmd")).read_text(encoding="utf-8")
        diagram["nodes"] = [deepcopy(catalog["symbols"][key]) for key in diagram["nodes"]]
        operations = diagram.pop("operations", {})
        for node in diagram["nodes"]:
            if node["key"] in operations:
                node["operations"] = operations[node["key"]]
            for item in [node, *node.get("operations", [])]:
                name = re.sub(r"\s+", "", item.get("name", ""))
                target = targets.get(name)
                if target:
                    document, anchor = target
                    item["api"] = get_target_uri(document) + ("#" + anchor if anchor else "")
    del output["symbols"]
    output["metadata"] = {
        "docsRoot": "../../",
        "revisionUrl": "../../revision.json",
        "mermaid": catalog["mermaid"],
        "source_repository": source_repository,
    }
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "catalog.json").write_text(json.dumps(output, separators=(",", ":")) + "\n", encoding="utf-8")
    for diagram in catalog["diagrams"]:
        (destination / (diagram["id"] + ".mmd")).write_bytes(
            (source / (diagram["id"] + ".mmd")).read_bytes()
        )
