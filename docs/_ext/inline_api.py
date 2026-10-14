"""Link ordinary inline code to unambiguous public C++ declarations."""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
import hashlib
import json
import re
from time import perf_counter

from docutils import nodes
from sphinx import addnodes
from sphinx.util import logging
from sphinx.util.nodes import make_refnode


LOGGER = logging.getLogger(__name__)
IDENTIFIER = re.compile(r"~?[A-Za-z_]\w*")
KINDS = {"class", "union", "function", "member", "type", "concept", "enum", "enumerator"}
TYPE_KINDS = {"class", "union", "type", "concept", "enum", "namespace"}


@dataclass(frozen=True, order=True)
class Target:
    document: str
    anchor: str
    kind: str


def balanced_end(text: str, start: int, opening: str, closing: str) -> int | None:
    depth = 0
    quote = ""
    escaped = False
    for position in range(start, len(text)):
        character = text[position]
        if quote:
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == quote:
                quote = ""
            continue
        if character in {"'", '"'}:
            quote = character
        elif character == opening:
            depth += 1
        elif character == closing:
            depth -= 1
            if depth == 0:
                return position + 1
    return None


def reference_name(text: str) -> tuple[str, str] | None:
    """Recognize a name, call, member assignment or simple type declarator.

    Call arguments identify no overload: they remain display text, never types.
    Template arguments are retained for exact specialization lookup.
    """
    text = text.strip()
    text = re.sub(r"^(?:(?:const|volatile|typename)\s+)+", "", text)
    position = 2 if text.startswith("::") else 0
    parts = []
    while True:
        match = IDENTIFIER.match(text, position)
        if match is None:
            return None
        start = position
        position = match.end()
        if position < len(text) and text[position] == "<":
            position = balanced_end(text, position, "<", ">")
            if position is None:
                return None
        parts.append(re.sub(r"\s+", "", text[start:position]))
        if not text.startswith("::", position):
            break
        position += 2
    name = "::".join(parts)
    if text.startswith("::"):
        name = "::" + name
    tail = text[position:].strip()
    if not tail:
        return name, "name"
    if tail.startswith("("):
        end = balanced_end(tail, 0, "(", ")")
        if end is not None and tail[end:].strip() in {"", ";"}:
            return name, "call"
    if "::" in name and re.fullmatch(r"=(?!=).+", tail, re.DOTALL):
        return name, "assignment"
    if re.fullmatch(r"(?:[*&]\s*(?:(?:const|volatile)\s*)?)+(?:[A-Za-z_]\w*)?", tail):
        return name, "type"
    return None


def without_templates(name: str) -> tuple[str, list[str]]:
    result = []
    prefixes = []
    position = 0
    while position < len(name):
        if name[position] == "<":
            prefixes.append("".join(result))
            position = balanced_end(name, position, "<", ">")
        else:
            result.append(name[position])
            position += 1
    return "".join(result), prefixes


class Index:
    def __init__(self, objects, namespaces, documents, templates):
        self.exact = defaultdict(set)
        self.suffixes = defaultdict(set)
        self.names = {}
        self.templates = templates
        for name, _, kind, document, anchor, _ in objects:
            if (kind in KINDS and name.startswith("CE::")
                    and document.startswith("generated/api/") and document in documents):
                name = re.sub(r"\s+", "", name)
                self.exact[name].add(Target(document, anchor, kind))
        for name, document in namespaces.items():
            if (name == "CE" or name.startswith("CE::")) and document in documents:
                self.exact[name].add(Target(document, "", "namespace"))
        for name, targets in self.exact.items():
            for target in targets:
                self.names[target] = name
            # Specialized names keep their exact spelling. Splitting their
            # template arguments into suffixes could mistake a type for an owner.
            if "<" in name:
                continue
            parts = name.split("::")
            for position in range(1, len(parts)):
                self.suffixes["::".join(parts[position:])].update(targets)

    def resolve(self, name: str, mode: str) -> Target | None:
        rooted = name.startswith(("::", "CE::")) or name == "CE"
        name = name.removeprefix("::")
        targets = self.exact.get(name)
        prefixes = []
        if targets is None and "<" in name:
            # Fall back only to an actual generic declaration, never to a
            # different specialization with its template arguments erased.
            name, prefixes = without_templates(name)
            targets = self.exact.get(name)
        if targets is None and not rooted:
            targets = self.suffixes.get(name)
        if not targets:
            return None
        if mode == "call":
            targets = {target for target in targets if target.kind in {"function", "class", "type"}}
        elif mode == "assignment":
            targets = {target for target in targets if target.kind == "member"}
        elif mode == "type" or "::" not in name:
            targets = {target for target in targets if target.kind in TYPE_KINDS}
        if len(targets) == 1:
            target = next(iter(targets))
            if prefixes:
                canonical = self.names[target]
                if not canonical.endswith(name):
                    return None
                owner = canonical[:len(canonical) - len(name)]
                if any(owner + prefix not in self.templates for prefix in prefixes):
                    return None
            return target
        return None

    def fingerprint(self) -> str:
        records = [
            (name, [(target.document, target.anchor, target.kind) for target in sorted(targets)])
            for name, targets in sorted(self.exact.items())
        ]
        return hashlib.sha256(json.dumps((records, sorted(self.templates))).encode()).hexdigest()


def prepare(app, environment):
    started = perf_counter()
    app._cheryl_inline_api = Index(
        environment.domains["cpp"].get_objects(),
        app._cheryl_cpp_namespaces,
        environment.found_docs,
        app._cheryl_cpp_references.templates,
    )
    fingerprint = app._cheryl_inline_api.fingerprint()
    changed = fingerprint != getattr(environment, "cheryl_inline_api_fingerprint", None)
    environment.cheryl_inline_api_fingerprint = fingerprint
    app._cheryl_inline_api_stats = {
        "index_seconds": perf_counter() - started, "link_seconds": 0.0,
        "literals": 0, "links": 0,
    }
    # Incremental API changes can add/remove links on unchanged prose pages.
    if changed:
        return sorted(environment.found_docs)
    return []


def already_linked(node):
    if "xref" in node.get("classes", []):
        return True
    ancestor = node.parent
    while ancestor is not None:
        if isinstance(ancestor, (nodes.reference, addnodes.pending_xref, nodes.literal_block, addnodes.desc_signature)):
            return True
        ancestor = ancestor.parent
    return False


def link_inline(app, doctree, document):
    started = perf_counter()
    stats = app._cheryl_inline_api_stats
    for literal in list(doctree.findall(nodes.literal)):
        if already_linked(literal):
            continue
        stats["literals"] += 1
        parsed = reference_name(literal.astext())
        target = app._cheryl_inline_api.resolve(*parsed) if parsed else None
        if target is None:
            continue
        replacement = make_refnode(
            app.builder, document, target.document, target.anchor, literal.deepcopy(),
        )
        replacement.source = literal.source
        replacement.line = literal.line
        replacement["classes"].append("inline-api")
        literal.replace_self(replacement)
        stats["links"] += 1
    stats["link_seconds"] += perf_counter() - started


def report(app, exception):
    if exception is not None or not hasattr(app, "_cheryl_inline_api_stats"):
        return
    stats = app._cheryl_inline_api_stats
    if app.parallel > 1:
        # Write workers have separate counters; do not report partial totals.
        LOGGER.info("Inline API index: %.3f s", stats["index_seconds"])
    else:
        LOGGER.info(
            "Inline API links: %d from %d literals; %.3f s indexing, %.3f s linking",
            stats["links"], stats["literals"], stats["index_seconds"], stats["link_seconds"],
        )


def setup(app):
    app.connect("env-updated", prepare)
    app.connect("doctree-resolved", link_inline)
    app.connect("build-finished", report)
    return {"version": "1", "env_version": 1, "parallel_read_safe": True, "parallel_write_safe": True}
