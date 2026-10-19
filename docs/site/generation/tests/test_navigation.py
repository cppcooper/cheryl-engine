"""Authored navigation contracts without Sphinx or generated API pages."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import posixpath
import re
import unittest


DOCS = Path(__file__).resolve().parents[2]


@dataclass
class Directive:
    name: str
    options: dict[str, str]
    targets: list[str]
    heading: int
    line: int


def directives(source):
    """Read navigation directives while treating other fenced content as opaque."""
    result = []
    fence = None
    block = None
    heading = 0
    for number, line in enumerate(source.splitlines(), 1):
        marker = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", line)
        if fence:
            if marker and marker[1][0] == fence[0] and len(marker[1]) >= len(fence) and not marker[2].strip():
                fence = None
                block = None
            elif block is not None:
                option = re.match(r"^:([\w-]+):\s*(.*)$", line)
                if option:
                    block.options[option[1]] = option[2]
                elif line.strip() and block.name == "toctree":
                    entry = re.fullmatch(r".*?\s*<([^>]+)>\s*", line)
                    block.targets.append(entry[1] if entry else line.strip())
            continue
        if marker:
            fence = marker[1]
            kind = re.match(r"\{(toctree|contents)\}(?:\s|$)", marker[2])
            if kind:
                block = Directive(kind[1], {}, [], heading, number)
                result.append(block)
            continue
        title = re.match(r"^ {0,3}(#{1,6})\s+", line)
        if title:
            heading = len(title[1])
    if fence:
        raise ValueError("Unclosed Markdown fence")
    return result


def destination(owner, target):
    if "://" in target or target == "self":
        return None
    path = target.split("#", 1)[0]
    path = posixpath.normpath(posixpath.join(posixpath.dirname(owner), path)) if not path.startswith("/") else path[1:]
    return path.removesuffix(".md").removesuffix(".rst")


def navigation(sources):
    pages = {}
    parents = {}
    pending = ["index"]
    while pending:
        owner = pending.pop()
        if owner in pages:
            continue
        pages[owner] = directives(sources[owner])
        for tree in pages[owner]:
            if tree.name != "toctree":
                continue
            for target in tree.targets:
                child = destination(owner, target)
                if child is None or child.startswith("generation/generated/"):
                    continue
                if child not in sources:
                    raise ValueError(f"{owner}: missing authored page {child}")
                parents.setdefault(child, []).append(owner)
                pending.append(child)
    return pages, parents


def check_owners(pages, parents):
    for page in pages:
        owners = parents.get(page, [])
        expected = 0 if page == "index" else 1
        if len(owners) != expected:
            raise ValueError(f"{page}: expected {expected} parent, found {owners}")


def check_scope(pages):
    for page, blocks in pages.items():
        for block in blocks:
            if block.name == "toctree" and block.heading != 1:
                raise ValueError(f"{page}:{block.line}: toctree belongs at page scope, before H2")


class NavigationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sources = {
            path.relative_to(DOCS).with_suffix("").as_posix(): path.read_text(encoding="utf-8")
            for path in DOCS.rglob("*.md")
            if not path.is_relative_to(DOCS / "generation")
        }
        cls.pages, cls.parents = navigation(sources)

    def children(self, page):
        return [destination(page, target) for tree in self.pages[page]
                if tree.name == "toctree" for target in tree.targets]

    def test_root_categories(self):
        trees = [tree for tree in self.pages["index"] if tree.name == "toctree"]
        self.assertEqual([tree.options.get("caption") for tree in trees],
                         ["API Reference", "Game Development", "Engine Development"])

    def test_sidebar_pages(self):
        trees = [tree for tree in self.pages["index"] if tree.name == "toctree"]
        self.assertTrue(all("titlesonly" in tree.options for tree in trees),
                        "Sidebar entries represent pages; task groups use local contents")

    def test_game_sections(self):
        tree = next(tree for tree in self.pages["index"] if tree.options.get("caption") == "Game Development")
        self.assertEqual([destination("index", target) for target in tree.targets], [
            "getting-started/project-setup", "getting-started/create-a/index",
            "getting-started/how-to/index", "guides/index", "engine-systems/index",
        ])

    def test_tutorial_sequence(self):
        self.assertEqual(self.children("getting-started/create-a/index"), [
            "getting-started/first-game", "getting-started/create-a/actor-controller",
            "getting-started/create-a/controller", "getting-started/create-a/character",
            "getting-started/create-a/ai-controller", "getting-started/create-a/ui-controller",
            "getting-started/create-a/hud", "getting-started/create-a/abstract-controller",
        ])

    def test_engine_sections(self):
        tree = next(tree for tree in self.pages["index"] if tree.options.get("caption") == "Engine Development")
        self.assertEqual({destination("index", target) for target in tree.targets}, {
            "development/contributing", "development/building", "development/modules",
            "development/testing", "development/documentation", "development/ci",
        })

    def test_task_index(self):
        blocks = self.pages["getting-started/how-to/index"]
        self.assertFalse(any(block.name == "toctree" for block in blocks),
                         "Task links must lead to topic-owned guides")
        self.assertTrue(any(block.name == "contents" and "local" in block.options for block in blocks))

    def test_single_parent(self):
        check_owners(self.pages, self.parents)

    def test_page_scope(self):
        check_scope(self.pages)

    def test_guide_homes(self):
        homes = {
            "getting-started/project-setup": ["modules/index", "development/consuming-engine"],
            "assets/index": ["getting-started/how-to/animate-sprite"],
            "rendering/index": ["getting-started/how-to/draw-sprite", "getting-started/how-to/use-camera", "modules/opengl"],
            "input/index": ["getting-started/how-to/bind-input"],
            "text/index": ["getting-started/how-to/update-text"],
            "gui/index": ["getting-started/how-to/create-gui", "modules/tgui", "modules/rmlui"],
            "audio/index": ["modules/miniaudio"],
            "runtime/index": ["modules/startup", "modules/workers", "modules/native-glfw",
                              "architecture/ownership-cleanup", "architecture/threads-data-flow"],
            "diagnostics/index": ["modules/debug-terminal-linux"],
            "development/contributing": ["development/code-style"],
            "development/modules": ["architecture/interfaces-adapters", "development/ui-adapters"],
            "development/testing": ["development/architecture-validation", "development/logging-acceptance",
                                    "development/native-desktop-checks"],
            "development/documentation": ["development/publishing-documentation"],
        }
        for owner, children in homes.items():
            for child in children:
                with self.subTest(page=child):
                    self.assertEqual(self.parents.get(child), [owner])


class ParserTests(unittest.TestCase):
    def test_fenced_examples(self):
        source = """# Guide
````markdown
## Example section
```{toctree}
Not real <example>
```
````
~~~cpp
## Also not a section
~~~
```{toctree}
Real page <real>
```
"""
        blocks = directives(source)
        self.assertEqual([block.targets for block in blocks], [["real"]])
        check_scope({"index": blocks})

    def test_duplicate_owners(self):
        pages, parents = navigation({
            "index": "# Home\n```{toctree}\nleft\nright\n```\n",
            "left": "# Left\n```{toctree}\nleaf\n```\n",
            "right": "# Right\n```{toctree}\nleaf\n```\n",
            "leaf": "# Leaf\n",
        })
        with self.assertRaisesRegex(ValueError, "leaf: expected 1 parent"):
            check_owners(pages, parents)

    def test_section_nesting(self):
        blocks = directives("# Guide\n## Unrelated section\n```{toctree}\nchild\n```\n")
        with self.assertRaisesRegex(ValueError, "toctree belongs at page scope"):
            check_scope({"index": blocks})


if __name__ == "__main__":
    unittest.main()
