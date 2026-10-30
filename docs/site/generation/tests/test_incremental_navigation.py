"""Shared navigation invalidation using the production hook without Sphinx."""

from __future__ import annotations

import ast
import hashlib
import json
from pathlib import Path
import pickle
import types
import unittest


EXTENSION = Path(__file__).resolve().parents[1] / "extensions/incremental_api.py"


def production_functions():
    source = ast.parse(EXTENSION.read_text(encoding="utf-8"), filename=str(EXTENSION))
    selected = [node for node in source.body if isinstance(node, ast.FunctionDef)
                and node.name in {"navigation_fingerprint", "refresh_navigation", "setup"}]
    namespace = {"hashlib": hashlib, "json": json,
                 "LOGGER": types.SimpleNamespace(info=lambda *arguments: None),
                 **{name: object() for name in ("begin", "preserve", "remember_removed", "finish")}}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(EXTENSION), "exec"), namespace)
    return types.SimpleNamespace(**namespace)


HOOKS = production_functions()
API = "generation/generated/api/unchanged"


class Toc:
    def __init__(self, text):
        self.text = text

    def pformat(self):
        return self.text


def environment():
    return types.SimpleNamespace(
        found_docs={"index", "guides", "guide", API},
        tocs={
            "index": Toc('<toctree caption="Game Development">\n    guides\n'),
            "guides": Toc('<reference refuri="guide">\n    Guide title\n'),
            "guide": Toc('<bullet_list>\n    Section\n'),
            API: Toc('<bullet_list>\n    API title\n'),
        },
        metadata={"guide": {"tocdepth": 2}},
    )


class NavigationTests(unittest.TestCase):
    def setUp(self):
        self.app = types.SimpleNamespace(builder=types.SimpleNamespace(format="html"))
        self.environment = environment()

    def prime(self):
        self.assertEqual(HOOKS.refresh_navigation(self.app, self.environment), sorted(self.environment.found_docs))

    def assert_rewrite(self):
        documents = HOOKS.refresh_navigation(self.app, self.environment)
        self.assertEqual(documents, sorted(self.environment.found_docs))
        self.assertIn(API, documents, "Unchanged API HTML also renders the shared sidebar")

    def test_first_snapshot(self):
        self.assertFalse(hasattr(self.environment, "_cheryl_navigation_fingerprint"))
        self.assert_rewrite()
        self.assertEqual(HOOKS.refresh_navigation(self.app, self.environment), [])

    def test_prose_only(self):
        self.prime()
        self.environment.prose = "Revised explanation outside the TOC"
        self.environment.metadata["guide"]["description"] = "Revised description"
        self.assertEqual(HOOKS.refresh_navigation(self.app, self.environment), [])

    def test_toc_changes(self):
        changes = {
            "title": ("guides", '<reference refuri="guide">\n    New title\n'),
            "caption": ("index", '<toctree caption="New root caption">\n    guides\n'),
            "parent": ("index", '<toctree caption="Game Development">\n    guide\n'),
        }
        for name, (document, text) in changes.items():
            with self.subTest(change=name):
                self.environment = environment()
                self.prime()
                self.environment.tocs[document].text = text
                self.assert_rewrite()

    def test_toc_depth(self):
        self.prime()
        self.environment.metadata["guide"]["tocdepth"] = 3
        self.assert_rewrite()

    def test_added_document(self):
        self.prime()
        self.environment.found_docs.add("new-guide")
        self.environment.tocs["new-guide"] = Toc('<bullet_list>\n    New guide\n')
        self.assert_rewrite()

    def test_removed_document(self):
        self.prime()
        self.environment.found_docs.remove("guide")
        del self.environment.tocs["guide"]
        self.assert_rewrite()

    def test_mapping_order(self):
        self.prime()
        self.environment.tocs = dict(reversed(list(self.environment.tocs.items())))
        self.assertEqual(HOOKS.refresh_navigation(self.app, self.environment), [])

    def test_saved_snapshot(self):
        self.prime()
        fingerprint = self.environment._cheryl_navigation_fingerprint
        self.environment = pickle.loads(pickle.dumps(self.environment))
        self.assertEqual(self.environment._cheryl_navigation_fingerprint, fingerprint)
        self.assertEqual(HOOKS.refresh_navigation(self.app, self.environment), [])
        self.environment.tocs["guides"].text += "Changed title\n"
        self.assert_rewrite()

    def test_non_html(self):
        self.app.builder.format = "latex"
        self.assertEqual(HOOKS.refresh_navigation(self.app, self.environment), [])
        self.assertFalse(hasattr(self.environment, "_cheryl_navigation_fingerprint"))
        self.environment._cheryl_navigation_fingerprint = "saved HTML snapshot"
        self.assertEqual(HOOKS.refresh_navigation(self.app, self.environment), [])
        self.assertEqual(self.environment._cheryl_navigation_fingerprint, "saved HTML snapshot")

    def test_registration(self):
        events = {}
        app = types.SimpleNamespace(connect=lambda event, callback, **options: events.update({event: callback}))
        HOOKS.setup(app)
        self.assertIs(events.get("env-updated"), HOOKS.refresh_navigation)


if __name__ == "__main__":
    unittest.main()
