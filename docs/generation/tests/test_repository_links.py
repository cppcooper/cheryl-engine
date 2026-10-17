"""Owner-guide projection regressions without Sphinx or generated output."""

from __future__ import annotations

import ast
import copy
from pathlib import Path
import re
import tempfile
import types
import unittest
from urllib.parse import unquote, urlsplit


EXTENSION = Path(__file__).resolve().parents[1] / "extensions/repository_links.py"
CHECKOUT = EXTENSION.parents[3]


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
        elif isinstance(statement, ast.Assign) and any(
            isinstance(target, ast.Name) and target.id == "PROJECTED_GUIDES" for target in statement.targets
        ):
            selected.append(statement)
        elif isinstance(statement, ast.FunctionDef) and statement.name in {"reference", "projected_reference", "rewrite_links"}:
            selected.append(statement)
    namespace = {
        "Path": Path,
        "unquote": unquote,
        "urlsplit": urlsplit,
        "make_refnode": make_reference,
        "nodes": types.SimpleNamespace(Element=Element, document=Element, inline=Element, reference=Reference),
        "addnodes": types.SimpleNamespace(download_reference=Download, pending_xref=Pending),
    }
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(EXTENSION), "exec"), namespace)
    return types.SimpleNamespace(**namespace)


LINKS = production_functions()


class ProjectionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="cheryl-guide-projection-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.docs = self.root / "docs"
        self.docs.mkdir()
        self.owner = self.root / "projects/modules/platform/native-glfw/README.md"
        self.owner.parent.mkdir(parents=True)
        self.owner.write_text("# Native GLFW\n", encoding="utf-8")
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

    def assert_projected(self, result, fragment="selection-and-dependency", source="engine-systems/input"):
        self.assertEqual(result["target_document"], "modules/native-glfw")
        self.assertEqual(result["target_fragment"], fragment)
        self.assertEqual(result["source_document"], source)
        self.assertTrue(result["internal"])

    def test_wrapper_inventory(self):
        wrappers = {}
        for page in (CHECKOUT / "docs/modules").glob("*.md"):
            included = re.search(r"```\{include\}\s+([^\n]+)", page.read_text(encoding="utf-8"))
            if included:
                owner = (page.parent / included[1]).resolve().relative_to(CHECKOUT).as_posix()
                wrappers[owner] = "modules/" + page.stem
        self.assertEqual(LINKS.PROJECTED_GUIDES, wrappers)

    def test_myst_link(self):
        label = Element("", "Native window")
        node = Pending("", "", label, reftype="myst",
                       reftarget="../../projects/modules/platform/native-glfw/README.md#selection-and-dependency")
        result = self.rewrite(node)
        self.assert_projected(result)
        self.assertEqual(result.children[0].children[0].text, "Native window")
        self.assertIsNot(result.children[0].children[0], label)

    def test_document_link(self):
        result = self.rewrite(Pending("", "", reftype="myst", refdomain="doc",
                                      reftarget="../projects/modules/platform/native-glfw/README",
                                      reftargetid="selection-and-dependency", refdoc="modules/index"))
        self.assert_projected(result, source="modules/index")

    def test_download_link(self):
        result = self.rewrite(Download("", "", reftarget="../../projects/modules/platform/native-glfw/README.md#selection-and-dependency"))
        self.assert_projected(result)

    def test_plain_link(self):
        result = self.rewrite(Reference("", "", refuri="../../projects/modules/platform/native-glfw/README.md#selection-and-dependency"))
        self.assert_projected(result)

    def test_unpublished_owner(self):
        self.environment.found_docs.clear()
        result = self.rewrite(Reference("", "", refuri="../../projects/modules/platform/native-glfw/README.md#selection-and-dependency"))
        self.assertFalse(result["internal"])
        self.assertIn("/projects/modules/platform/native-glfw/README.md#selection-and-dependency", result["refuri"])
        self.assertIn("a" * 40, result["refuri"])

    def test_source_link(self):
        source = self.owner.parent / "include/window.h"
        source.parent.mkdir()
        source.write_text("#pragma once\n", encoding="utf-8")
        result = self.rewrite(Reference("", "", refuri="../../projects/modules/platform/native-glfw/include/window.h?plain=1#L1"))
        self.assertFalse(result["internal"])
        self.assertTrue(result["refuri"].endswith("/include/window.h?plain=1#L1"))

    def test_external_link(self):
        node = Reference("", "", refuri="https://example.com/README.md#selection-and-dependency")
        self.assertIs(self.rewrite(node), node)

    def test_existing_document(self):
        self.environment.found_docs.add("assets/asset-loading")
        node = Reference("", "", refuri="../assets/asset-loading.md#publication-and-retry")
        self.assertIs(self.rewrite(node), node)

    def test_outside_checkout(self):
        node = Reference("", "", refuri="../../../outside.md")
        self.assertIs(self.rewrite(node), node)


if __name__ == "__main__":
    unittest.main()
