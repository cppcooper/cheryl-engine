"""Canonical site and repository-link regressions without Sphinx or generation."""

from __future__ import annotations

import ast
import copy
from pathlib import Path
import tempfile
import types
import unittest
from urllib.parse import unquote, urlsplit


EXTENSION = Path(__file__).resolve().parents[1] / "extensions/repository_links.py"
CHECKOUT = EXTENSION.parents[4]


class Element(dict):
    def __init__(self, rawsource="", text="", *children, **attributes):
        super().__init__(attributes)
        self.children = list(children)
        self.text = text
        self.source = None
        self.line = None
        self.parent = None
        for child in self.children:
            child.parent = self

    def deepcopy(self):
        return copy.deepcopy(self)

    def extend(self, children):
        for child in children:
            child.parent = self
            self.children.append(child)

    def findall(self, kind):
        for child in self.children:
            if isinstance(child, kind):
                yield child
            yield from child.findall(kind)

    def replace_self(self, replacement):
        index = self.parent.children.index(self)
        self.parent.children[index] = replacement
        replacement.parent = self.parent


class Reference(Element):
    pass


class Pending(Element):
    pass


class Download(Element):
    pass


def make_reference(builder, source, document, fragment, content):
    return Reference("", "", content, refuri=document + ".html" + ("#" + fragment if fragment else ""),
                     source_document=source, target_document=document, target_fragment=fragment, internal=True)


def production_functions():
    tree = ast.parse(EXTENSION.read_text(encoding="utf-8"), filename=str(EXTENSION))
    selected = []
    for statement in tree.body:
        if isinstance(statement, ast.ImportFrom) and statement.module == "__future__":
            selected.append(statement)
        elif isinstance(statement, ast.FunctionDef) and statement.name in {"reference", "rewrite_links"}:
            selected.append(statement)
    namespace = {
        "Path": Path,
        "unquote": unquote,
        "urlsplit": urlsplit,
        "make_refnode": make_reference,
        "ConfigError": ValueError,
        "nodes": types.SimpleNamespace(Element=Element, document=Element, inline=Element, reference=Reference),
        "addnodes": types.SimpleNamespace(download_reference=Download, pending_xref=Pending),
    }
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(EXTENSION), "exec"), namespace)
    return types.SimpleNamespace(**namespace)


LINKS = production_functions()


class RepositoryLinkTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="cheryl-repository-links-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.docs = self.root / "docs/site"
        self.docs.mkdir(parents=True)
        self.owner = self.docs / "modules/native-glfw.md"
        self.owner.parent.mkdir(parents=True)
        self.owner.write_text("# Native GLFW\n", encoding="utf-8")
        # Simulate a checkout without symlink support. Site links must not need
        # to read or interpret this source-neighbor convenience alias.
        alias = self.root / "projects/modules/platform/native-glfw/README.md"
        alias.parent.mkdir(parents=True)
        alias.write_text("../../../../docs/site/modules/native-glfw.md", encoding="utf-8")
        self.environment = types.SimpleNamespace(
            found_docs={"modules/native-glfw"}, docname="engine-systems/input",
            doc2path=lambda name: str(self.docs / (name + ".md")),
            path2doc=self.document_name,
        )
        self.app = types.SimpleNamespace(
            srcdir=str(self.docs), env=self.environment, builder=object(),
            config=types.SimpleNamespace(source_suffix={".md": "markdown"}),
            _cheryl_repository_links=types.SimpleNamespace(root=self.root, source_url=self.source_url),
        )

    def document_name(self, path):
        path = Path(path).resolve()
        if path.is_relative_to(self.docs) and path.suffix == ".md":
            return path.relative_to(self.docs).with_suffix("").as_posix()
        return None

    def source_url(self, path, fragment="", query=""):
        path = Path(path).resolve()
        if not path.is_relative_to(self.root) or not path.exists():
            return None
        result = "https://github.com/example/repo/blob/" + "a" * 40 + "/" + path.relative_to(self.root).as_posix()
        return result + ("?" + query if query else "") + ("#" + fragment if fragment else "")

    def rewrite(self, node):
        document = Element("", "", node)
        LINKS.rewrite_links(self.app, document)
        return document.children[0]

    def test_module_sources(self):
        pages = list((CHECKOUT / "docs/site/modules").glob("*.md"))
        self.assertTrue(pages)
        for page in pages:
            with self.subTest(page=page.name):
                self.assertFalse(page.is_symlink())
                content = page.read_text(encoding="utf-8")
                self.assertTrue(any(line.startswith("# ") for line in content.splitlines()), "Missing page title")
                self.assertFalse("```{include}" in content, "Module page still includes another content owner")

    def test_canonical_module(self):
        target = "../modules/native-glfw.md#selection-and-dependency"
        nodes = (
            Pending("", "", reftype="myst", reftarget=target),
            Download("", "", reftarget=target),
            Reference("", "", refuri=target),
            Pending("", "", reftype="myst", refdomain="doc", reftarget="modules/native-glfw"),
        )
        for node in nodes:
            with self.subTest(kind=type(node).__name__, attributes=dict(node)):
                self.assertIs(self.rewrite(node), node)

    def test_unpublished_document(self):
        self.environment.found_docs.clear()
        result = self.rewrite(Reference("", "", refuri="../modules/native-glfw.md#selection-and-dependency"))
        self.assertFalse(result["internal"])
        self.assertIn("/docs/site/modules/native-glfw.md#selection-and-dependency", result["refuri"])
        self.assertIn("a" * 40, result["refuri"])

    def test_source_link(self):
        source = self.root / "projects/modules/platform/native-glfw/include/window.h"
        source.parent.mkdir(parents=True)
        source.write_text("#pragma once\n", encoding="utf-8")
        result = self.rewrite(Reference("", "", refuri="../../../projects/modules/platform/native-glfw/include/window.h?plain=1#L1"))
        self.assertFalse(result["internal"])
        self.assertTrue(result["refuri"].endswith("/include/window.h?plain=1#L1"))

    def test_repository_documents(self):
        source = self.root / "docs/for-agents/documentation.md"
        source.parent.mkdir(parents=True)
        source.write_text("# Documentation guidance\n", encoding="utf-8")
        target = "../../for-agents/documentation.md#support-the-claims"
        nodes = (
            Pending("", "", Element("", "Agent guidance"), reftype="myst", reftarget=target),
            Download("", "", Element("", "Agent guidance"), reftarget=target),
            Reference("", "", Element("", "Agent guidance"), refuri=target),
        )
        for node in nodes:
            with self.subTest(kind=type(node).__name__):
                label = node.children[0]
                result = self.rewrite(node)
                self.assertFalse(result["internal"])
                self.assertTrue(result["refuri"].endswith("/docs/for-agents/documentation.md#support-the-claims"))
                self.assertEqual(result.children[0].text, "Agent guidance")
                if isinstance(node, Reference):
                    self.assertIs(result.children[0], label)
                else:
                    self.assertIsNot(result.children[0], label)

    def test_repository_document_target(self):
        source = self.root / "docs/for-agents/documentation.md"
        source.parent.mkdir(parents=True)
        source.write_text("# Documentation guidance\n", encoding="utf-8")
        result = self.rewrite(Pending("", "", reftype="myst", refdomain="doc",
                                      reftarget="../for-agents/documentation", reftargetid="support-the-claims"))
        self.assertFalse(result["internal"])
        self.assertTrue(result["refuri"].endswith("/docs/for-agents/documentation.md#support-the-claims"))

    def test_external_link(self):
        node = Reference("", "", refuri="https://example.com/README.md#selection-and-dependency")
        self.assertIs(self.rewrite(node), node)

    def test_api_guide(self):
        self.environment.docname = "generation/generated/api/classInput"
        label = Element("", "Input contract")
        result = self.rewrite(Reference("", "", label,
                                        refuri="cheryl-guide:modules/native-glfw#input-lifetime-and-mapping"))
        self.assertTrue(result["internal"])
        self.assertEqual(result["source_document"], self.environment.docname)
        self.assertEqual(result["target_document"], "modules/native-glfw")
        self.assertEqual(result["target_fragment"], "input-lifetime-and-mapping")
        self.assertEqual(result.children[0].children[0].text, "Input contract")
        self.assertIsNot(result.children[0].children[0], label)

    def test_api_guide_suffix(self):
        result = self.rewrite(Reference("", "", refuri="cheryl-guide:modules/native-glfw.md"))
        self.assertEqual(result["target_document"], "modules/native-glfw")
        self.assertEqual(result["target_fragment"], "")

    def test_api_guide_rejects(self):
        for target in ("missing", "/modules/native-glfw", "../modules/native-glfw",
                       "modules/%2e%2e/native-glfw", "//host/modules/native-glfw", "modules/native-glfw?x=1"):
            with self.subTest(target=target), self.assertRaisesRegex(ValueError, "API guide target"):
                self.rewrite(Reference("", "", refuri="cheryl-guide:" + target))

    def test_existing_document(self):
        self.environment.found_docs.add("assets/asset-loading")
        node = Reference("", "", refuri="../assets/asset-loading.md#publication-and-retry")
        self.assertIs(self.rewrite(node), node)

    def test_outside_checkout(self):
        node = Reference("", "", refuri="../../../../outside.md")
        self.assertIs(self.rewrite(node), node)


if __name__ == "__main__":
    unittest.main()
