"""Keep repository-relative links usable in the published manual."""

from __future__ import annotations

import configparser
from dataclasses import dataclass
from pathlib import Path
import re
import subprocess
from urllib.parse import quote, unquote, urlsplit

from docutils import nodes
from sphinx import addnodes
from sphinx.errors import ConfigError


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


def page_context(app, pagename, templatename, context, doctree) -> None:
    # Generated reference sources are published by Sphinx but are not Git files.
    if pagename.startswith("api/generated/"):
        context["display_github"] = False
    elif pagename in app.env.found_docs:
        context["page_source_suffix"] = Path(app.env.doc2path(pagename)).suffix


def setup(app) -> dict[str, object]:
    app.add_config_value("source_repository", "", "env")
    app.add_config_value("source_repository_root", "", "env")
    app.add_config_value("source_revision", "", "env")
    app.connect("builder-inited", prepare)
    # Run before download collectors: source files remain links, not copied assets.
    app.connect("doctree-read", rewrite_links, priority=100)
    app.connect("html-page-context", page_context)
    return {"version": "1", "parallel_read_safe": True, "parallel_write_safe": True}
