"""Compare cached navigation with Sphinx using a small disposable manual."""

from __future__ import annotations

import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from docutils import nodes
from sphinx.application import Sphinx


EXTENSIONS = Path(__file__).resolve().parents[1] / "extensions"
sys.path.insert(0, str(EXTENSIONS))
try:
    import sidebar
finally:
    sys.path.pop(0)


class SidebarTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory(prefix="cheryl-sidebar-")
        cls.addClassCleanup(temporary.cleanup)
        root = Path(temporary.name)
        source = root / "source"
        source.mkdir()
        (source / "conf.py").write_text(
            "project = 'Navigation fixture'\n"
            "html_theme = 'basic'\n"
            "exclude_patterns = []\n", encoding="utf-8",
        )
        pages = {
            "index": "Home\n====\n\n.. toctree::\n   :caption: Manual\n   :titlesonly:\n\n   guide/index\n   Home again <self>\n   Outside <https://example.com/path#anchor>\n\n.. toctree::\n   :caption: Hidden\n   :hidden:\n\n   hidden\n",
            "guide/index": "Guide\n=====\n\nSection\n-------\n\n.. toctree::\n\n   child\n",
            "guide/child": "Child\n=====\n\n.. toctree::\n\n   deep/index\n",
            "guide/deep/index": "Deep\n====\n\n.. toctree::\n\n   leaf\n",
            "guide/deep/leaf": "Leaf\n====\n\nDetails\n-------\n\nLeaf details.\n",
            "hidden": "Hidden\n======\n\nHidden section\n--------------\n\nHidden details.\n",
        }
        for name, text in pages.items():
            path = source / (name + ".rst")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        warnings = io.StringIO()
        cls.app = Sphinx(str(source), str(source), str(root / "html"), str(root / "doctrees"),
                         "html", status=io.StringIO(), warning=warnings, freshenv=True,
                         warningiserror=True)
        cls.app.build(force_all=True)
        if cls.app.statuscode:
            raise AssertionError(warnings.getvalue())
        cls.documents = [*pages, "search", "genindex"]

    def setUp(self):
        sidebar.prepare(self.app, self.app.env)

    def test_sphinx_equivalence(self):
        for hidden in (False, True):
            for titles in (False, True):
                cached = sidebar.Sidebar(self.app.builder, hidden, titles)
                for depth in (1, 2, 4, 8):
                    for document in self.documents:
                        with self.subTest(hidden=hidden, titles=titles, depth=depth, document=document):
                            expected = self.app.builder._get_local_toctree(
                                document, collapse=False, maxdepth=depth,
                                includehidden=hidden, titles_only=titles,
                            )
                            self.assertEqual(cached.render(document, depth), expected)

    def test_page_isolation(self):
        cached = sidebar.Sidebar(self.app.builder, True, True)
        cached.tree["iscurrent"] = None
        before = cached.tree.pformat()
        first = cached.render("guide/deep/leaf", 2)
        cached.render("hidden", 4)
        self.assertEqual(cached.render("guide/deep/leaf", 2), first)
        self.assertEqual(cached.tree.pformat(), before)
        with patch.object(sidebar, "_toctree_copy", side_effect=RuntimeError("fixture failure")):
            with self.assertRaisesRegex(RuntimeError, "fixture failure"):
                cached.render("guide/index", 4)
        self.assertEqual(cached.tree.pformat(), before)

    def test_new_build(self):
        context = {"toctree": self.app.builder._get_local_toctree}
        sidebar.page_context(self.app, "guide/index", "page.html", context, None)
        before = context["toctree"](collapse=False, maxdepth=4, includehidden=True)
        self.assertTrue(self.app._cheryl_sidebars)
        reference = next(self.app.env.tocs["guide/index"].findall(nodes.reference))
        previous = reference[0]
        try:
            reference[0] = nodes.Text("Updated guide")
            sidebar.prepare(self.app, self.app.env)
            self.assertEqual(self.app._cheryl_sidebars, {})
            after = context["toctree"](collapse=False, maxdepth=4, includehidden=True)
            self.assertNotEqual(before, after)
            self.assertIn("Updated guide", after)
        finally:
            reference[0] = previous

    def test_native_fallback(self):
        calls = []

        def native(**kwargs):
            calls.append(kwargs)
            return "native navigation"

        context = {"toctree": native}
        sidebar.page_context(self.app, "index", "page.html", context, None)
        for options in ({"collapse": True, "maxdepth": 4},
                        {"collapse": False, "maxdepth": 0}):
            self.assertEqual(context["toctree"](**options), "native navigation")
        self.assertEqual(len(calls), 2)
        self.assertFalse(self.app._cheryl_sidebars)


if __name__ == "__main__":
    unittest.main()
