"""Keep repository-relative links usable in the published manual."""

from __future__ import annotations

import configparser
from dataclasses import dataclass
from pathlib import Path
import re
import subprocess
from urllib.parse import quote, unquote, urlsplit
import xml.etree.ElementTree as ElementTree

from docutils import nodes
from sphinx import addnodes
from sphinx.errors import ConfigError
from sphinx.util.nodes import make_refnode
from pygments.lexers.special import TextLexer
from api_rendering import namespace_identity
from cpp_references import CppReferences


@dataclass(frozen=True)
class Repository:
    root: Path
    url: str
    revision: str
    submodules: tuple[tuple[Path, str, str], ...]

    @classmethod
    def from_config(cls, config) -> Repository:
        root = Path(config.source_repository_root).resolve()
        revision = config.source_revision
        if not re.fullmatch(r"[0-9a-fA-F]{40}|[0-9a-fA-F]{64}", revision):
            raise ConfigError("source_revision must be a full Git commit ID.")
        url = config.source_repository.rstrip("/").removesuffix(".git")
        if not url.startswith("https://github.com/"):
            raise ConfigError("source_repository must be a GitHub HTTPS URL.")
        gitmodules = subprocess.run(
            ["git", "show", f"{revision}:.gitmodules"],
            cwd=root,
            capture_output=True,
            text=True,
            check=False,
        )
        submodules = []
        if gitmodules.returncode == 0:
            configuration = configparser.ConfigParser(interpolation=None)
            configuration.read_string(gitmodules.stdout)
            tree = subprocess.check_output(
                ["git", "ls-tree", "-rz", revision], cwd=root
            )
            gitlinks = {}
            for entry in tree.split(b"\0"):
                if not entry:
                    continue
                metadata, filename = entry.split(b"\t", 1)
                mode, _, commit = metadata.split(b" ", 2)
                if mode == b"160000":
                    gitlinks[filename.decode("utf-8")] = commit.decode("ascii")
            for section in configuration.sections():
                prefix = configuration.get(section, "path", fallback="")
                remote = configuration.get(section, "url", fallback="")
                if prefix in gitlinks and remote.startswith("https://github.com/"):
                    submodules.append(
                        (
                            Path(prefix),
                            remote.rstrip("/").removesuffix(".git"),
                            gitlinks[prefix],
                        )
                    )
        return cls(
            root,
            url,
            revision.lower(),
            tuple(sorted(submodules, key=lambda item: len(item[0].parts), reverse=True)),
        )

    def source_url(self, path: Path, fragment: str = "", query: str = "") -> str | None:
        path = path.resolve()
        if not path.is_relative_to(self.root):
            return None
        relative = path.relative_to(self.root)
        repository, revision = self.url, self.revision
        submodule = False
        for prefix, remote, commit in self.submodules:
            if relative.is_relative_to(prefix):
                relative = relative.relative_to(prefix)
                repository, revision = remote, commit
                submodule = True
                break
        if not path.exists() and not submodule:
            return None
        kind = "tree" if path.is_dir() or not relative.parts else "blob"
        if submodule and not path.exists() and not relative.suffix:
            kind = "tree"
        result = f"{repository}/{kind}/{revision}"
        if relative.parts:
            result += "/" + quote(relative.as_posix(), safe="/")
        if query:
            result += "?" + query
        if fragment:
            result += "#" + quote(fragment, safe="-._:")
        return result


def reference(node: nodes.Element, url: str) -> nodes.reference:
    replacement = nodes.reference("", "", refuri=url, internal=False)
    replacement.source = node.source
    replacement.line = node.line
    replacement.extend(child.deepcopy() for child in node.children)
    for child in replacement.findall(nodes.Element):
        if "download" in child.get("classes", []):
            child["classes"] = [item for item in child["classes"] if item != "download"]
    return replacement


def rewrite_links(app, doctree: nodes.document) -> None:
    repository = app._cheryl_repository_links
    environment = app.env
    sources = Path(app.srcdir)
    for node in list(doctree.findall(addnodes.download_reference)):
        parsed = urlsplit(node.get("reftarget", ""))
        if parsed.scheme or parsed.netloc or not parsed.path:
            continue
        document = Path(environment.doc2path(node.get("refdoc", environment.docname)))
        path = (
            sources / unquote(parsed.path).lstrip("/")
            if parsed.path.startswith("/")
            else document.parent / unquote(parsed.path)
        )
        docname = environment.path2doc(str(path))
        if docname in environment.found_docs:
            continue
        url = repository.source_url(path, parsed.fragment, parsed.query)
        if url:
            node.replace_self(reference(node, url))
    for node in list(doctree.findall(addnodes.pending_xref)):
        if node.get("reftype") != "myst":
            continue
        target = node.get("reftarget", "")
        if node.get("refdomain") == "doc":
            if target in environment.found_docs:
                continue
            for suffix in app.config.source_suffix:
                path = sources / (target + suffix)
                url = repository.source_url(path, node.get("reftargetid") or "")
                if url:
                    node.replace_self(reference(node, url))
                    break
            continue
        parsed = urlsplit(target)
        if parsed.scheme or parsed.netloc or not parsed.path:
            continue
        document = Path(environment.doc2path(node.get("refdoc", environment.docname)))
        path = (
            sources / unquote(parsed.path).lstrip("/")
            if parsed.path.startswith("/")
            else document.parent / unquote(parsed.path)
        )
        docname = environment.path2doc(str(path))
        if docname in environment.found_docs:
            continue
        url = repository.source_url(path, parsed.fragment, parsed.query)
        if url:
            node.replace_self(reference(node, url))
    for node in list(doctree.findall(nodes.reference)):
        target = node.get("refuri", "")
        parsed = urlsplit(target)
        if parsed.scheme or parsed.netloc or not parsed.path:
            continue
        document = Path(environment.doc2path(environment.docname))
        path = (
            sources / unquote(parsed.path).lstrip("/")
            if parsed.path.startswith("/")
            else document.parent / unquote(parsed.path)
        )
        docname = environment.path2doc(str(path))
        if docname in environment.found_docs:
            continue
        url = repository.source_url(path, parsed.fragment, parsed.query)
        if url:
            node["refuri"] = url
            node["internal"] = False


def prepare(app) -> None:
    app._cheryl_repository_links = Repository.from_config(app.config)
    project = app.config.breathe_default_project
    index = ElementTree.parse(Path(app.config.breathe_projects[project]) / "index.xml")
    app._cheryl_cpp_references = CppReferences(
        Path(app.config.breathe_projects[project]), app._cheryl_repository_links
    )
    app._cheryl_doxygen_documents = {
        identifier: "api/generated/" + Path(node.file_name).stem
        for identifier, node in app.exhale_root.node_by_refid.items()
        if getattr(node, "file_name", None)
    }
    namespaces = {}
    short_names = {}
    for compound in index.findall("compound"):
        if compound.get("kind") != "namespace":
            continue
        name = compound.findtext("name")
        docname = "api/generated/" + namespace_identity(name)
        if not (Path(app.srcdir) / (docname + ".rst")).is_file():
            continue
        namespaces[name] = docname
        short_names.setdefault(name.rsplit("::", 1)[-1], []).append(docname)
    for name, documents in short_names.items():
        if len(documents) == 1:
            namespaces.setdefault(name, documents[0])
    app._cheryl_cpp_namespaces = namespaces
    classes = {}
    for compound in index.findall("compound"):
        if compound.get("kind") not in {"class", "struct"}:
            continue
        identifier = compound.get("refid")
        classes[identifier] = ElementTree.parse(
            Path(app.config.breathe_projects[project]) / (identifier + ".xml")
        ).find("compounddef")

    def aliases(identifier, ancestors=()):
        if identifier in ancestors or identifier not in classes:
            return {}
        declaration = classes[identifier]
        result = {}
        for base in declaration.findall("basecompoundref"):
            if base.get("prot") != "public":
                continue
            for name, targets in aliases(base.get("refid"), (*ancestors, identifier)).items():
                result.setdefault(name, set()).update(targets)
        for member in declaration.findall(".//memberdef"):
            result.pop(member.findtext("name"), None)
            if member.get("kind") == "typedef" and member.get("prot") == "public":
                result[member.findtext("name")] = {
                    ("api/generated/" + identifier, member.get("id"))
                }
        return result

    inherited = {}
    for identifier, declaration in classes.items():
        targets = {}
        for base in declaration.findall("basecompoundref"):
            if base.get("prot") != "public":
                continue
            for name, owners in aliases(base.get("refid")).items():
                targets.setdefault(name, set()).update(owners)
        for member in declaration.findall(".//memberdef"):
            targets.pop(member.findtext("name"), None)
        inherited["api/generated/" + identifier] = {
            name: next(iter(owners))
            for name, owners in targets.items()
            if len(owners) == 1
        }
    app._cheryl_inherited_aliases = inherited


def cpp_reference(app, environment, node, content):
    if node.get("refdomain") == "std" and node.get("reftype") == "ref":
        docname = app._cheryl_doxygen_documents.get(node.get("reftarget"))
        if docname in environment.found_docs:
            return make_refnode(
                app.builder, node.get("refdoc", environment.docname), docname, "", content
            )
        location = app._cheryl_cpp_references.hidden_sources.get(node.get("reftarget"))
        url = app._cheryl_cpp_references.location_url(location)
        if url:
            return nodes.reference("", "", content, refuri=url, internal=False)
    if node.get("refdomain") != "cpp" or node.get("reftype") != "identifier":
        return None
    from_document = node.get("refdoc", environment.docname)
    alias = app._cheryl_inherited_aliases.get(from_document, {}).get(
        node.get("reftarget")
    )
    if alias and alias[0] in environment.found_docs:
        # Sphinx does not follow C++ template bases for alias lookup. Link only
        # aliases whose public declaring owner is unambiguous in Doxygen's XML.
        return make_refnode(app.builder, from_document, alias[0], alias[1], content)
    target = node.get("reftarget", "")
    docname = None
    # Names such as Input::Events and Events are intentionally reused. Resolve
    # namespace references from the declaring scope before trying global aliases.
    if not target.startswith("::"):
        for owner in app._cheryl_cpp_references.lexical_owners(from_document):
            qualified = "::".join(part for part in (owner, target) if part)
            candidate = app._cheryl_cpp_namespaces.get(qualified)
            if candidate in environment.found_docs:
                docname = candidate
                break
    if docname is None:
        docname = app._cheryl_cpp_namespaces.get(target.removeprefix("::"))
    if docname not in environment.found_docs:
        return app._cheryl_cpp_references.resolve(app, environment, node, content)
    return make_refnode(
        app.builder,
        from_document,
        docname,
        "",
        content,
    )


def page_context(app, pagename, templatename, context, doctree) -> None:
    context["declaring_source_url"] = None
    if pagename.startswith("api/generated/"):
        for identifier, document in app._cheryl_doxygen_documents.items():
            if document == pagename:
                location = app._cheryl_declaration_sources.get(identifier)
                context["declaring_source_url"] = app._cheryl_cpp_references.location_url(location)
                break
        # Concept owner pages are added after Exhale's node inventory is created.
        if context["declaring_source_url"] is None:
            identifier = pagename.removeprefix("api/generated/")
            context["declaring_source_url"] = app._cheryl_cpp_references.location_url(
                app._cheryl_declaration_sources.get(identifier)
            )
    # Generated reference sources are published by Sphinx but are not Git files.
    if pagename not in app.env.found_docs or pagename.startswith("api/generated/"):
        context["display_github"] = False
    elif pagename in app.env.found_docs:
        context["page_source_suffix"] = Path(app.env.doc2path(pagename)).suffix


def setup(app) -> dict[str, object]:
    app.add_config_value("source_repository", "", "env")
    app.add_config_value("source_repository_root", "", "env")
    app.add_config_value("source_revision", "", "env")
    app.connect("builder-inited", prepare)
    app.connect("missing-reference", cpp_reference)
    # Run before download collectors: source files remain links, not copied assets.
    app.connect("doctree-read", rewrite_links, priority=100)
    app.connect("html-page-context", page_context)
    # Existing Mermaid fences remain readable source until a diagram renderer is
    # selected. Treat them as text explicitly instead of suppressing warnings.
    app.add_lexer("mermaid", TextLexer)
    return {"version": "1", "parallel_read_safe": True, "parallel_write_safe": True}
