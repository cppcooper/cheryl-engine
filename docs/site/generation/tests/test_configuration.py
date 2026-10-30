"""Configuration regressions without importing Sphinx or generating documentation."""

import ast
from fnmatch import fnmatchcase
import json
from pathlib import Path
import re
import shlex
import tempfile
import unittest


CONFIGURATION = Path(__file__).resolve().parents[1] / "conf.py"
CHECKOUT = CONFIGURATION.parents[3]
EXPORT_HEADER = CHECKOUT / "projects/engine/include/cheryl/export.h"


class LayoutTests(unittest.TestCase):
    def test_roots(self):
        for name in ("build.py", "conf.py"):
            with self.subTest(source=name):
                source = CONFIGURATION.with_name(name)
                tree = ast.parse(source.read_text(encoding="utf-8"))
                names = {"GENERATION_ROOT", "GENERATED_ROOT", "SITE_ROOT", "REPO_ROOT"}
                statements = [statement for statement in tree.body
                              if isinstance(statement, ast.Assign)
                              and any(isinstance(target, ast.Name) and target.id in names
                                      for target in statement.targets)]
                scope = {"Path": Path, "__file__": str(source)}
                exec(compile(ast.Module(body=statements, type_ignores=[]), str(source), "exec"), scope)
                self.assertEqual(scope["REPO_ROOT"], CHECKOUT)
                self.assertEqual(scope["SITE_ROOT"], CHECKOUT / "docs/site")
                self.assertEqual(scope["GENERATED_ROOT"], source.parent / "generated")

    def test_edit_location(self):
        tree = ast.parse(CONFIGURATION.read_text(encoding="utf-8"))
        context = next(statement.value for statement in tree.body
                       if isinstance(statement, ast.Assign)
                       and any(isinstance(target, ast.Name) and target.id == "html_context"
                               for target in statement.targets))
        location = next(value for key, value in zip(context.keys, context.values)
                        if isinstance(key, ast.Constant) and key.value == "conf_py_path")
        self.assertEqual(ast.literal_eval(location), "/docs/site/")


class DoxygenTests(unittest.TestCase):
    def setUp(self):
        source = CONFIGURATION.with_name("Doxyfile.in").read_text(encoding="utf-8")
        self.settings = {}
        for line in re.sub(r"\\\r?\n", " ", source).splitlines():
            assignment = re.match(r"\s*([A-Z_]+)\s*(\+?=)\s*(.*)", line)
            if assignment:
                name, operator, value = assignment.groups()
                tokens = shlex.split(value, comments=True)
                self.settings[name] = self.settings.get(name, []) + tokens if operator == "+=" else tokens

    def test_selective_expansion(self):
        for name in ("ENABLE_PREPROCESSING", "MACRO_EXPANSION", "EXPAND_ONLY_PREDEF"):
            with self.subTest(setting=name):
                self.assertEqual(self.settings.get(name), ["YES"])
        self.assertEqual(self.settings.get("SEARCH_INCLUDES"), ["NO"])
        self.assertFalse(self.settings.get("EXPAND_AS_DEFINED"))

    def test_export_decorators(self):
        # Read the owning header so a newly added component cannot silently
        # introduce another decorator into Breathe/Sphinx declarations.
        macros = set(re.findall(
            r"^\s*#\s*define\s+(CE_[A-Z0-9_]+)\b",
            EXPORT_HEADER.read_text(encoding="utf-8"), re.MULTILINE,
        ))
        self.assertIn("CE_API", macros)
        self.assertIn("CE_TYPE_API", macros)
        definitions = {}
        for token in self.settings.get("PREDEFINED", []):
            name, equals, value = token.partition("=")
            definitions[name.removesuffix(":")] = value if equals else "1"
        self.assertEqual(definitions.get("CHERYL_NATIVE_INPUT"), "1")
        for name in sorted(macros):
            with self.subTest(decorator=name):
                self.assertEqual(definitions.get(name), "", "Export decorators must expand to nothing")

    def test_public_macro_scope(self):
        excluded = set(self.settings.get("EXCLUDE_SYMBOLS", []))
        self.assertTrue({"CEFONT_H", "CE_CORE_H", "FFONT_H", "FRAME_H", "GLSLPROGRAM_H", "TEXTURE_H",
                         "CE_DETAIL_EXPORT", "CE_DETAIL_IMPORT", "GLM_ENABLE_EXPERIMENTAL"}.issubset(excluded))
        self.assertTrue({"CE_API", "CE_TYPE_API", "CE_UI_TGUI_API", "CHERYL_LOG_PROFILE"}.isdisjoint(excluded))


class ExclusionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="cheryl-docs-configuration-")
        self.addCleanup(temporary.cleanup)
        self.docs = Path(temporary.name) / "docs/site"
        self.generation = self.docs / "generation"
        self.generated = self.generation / "generated"
        (self.generated / "api").mkdir(parents=True)
        for name in ("conf.py", "build.py", "requirements.txt"):
            (self.generation / name).touch()
        for name in (".generation.lock", "Doxyfile", "sphinx-warnings.log"):
            (self.generated / name).touch()

    def exclusions(self):
        tree = ast.parse(CONFIGURATION.read_text(encoding="utf-8"), filename=str(CONFIGURATION))
        statements = [
            statement for statement in tree.body
            if any(isinstance(node, ast.Name) and node.id == "exclude_patterns" for node in ast.walk(statement))
        ]
        scope = {
            "SITE_ROOT": self.docs,
            "GENERATION_ROOT": self.generation,
            "GENERATED_ROOT": self.generated,
        }
        # Evaluate only the real exclusion rules, with all paths in the fixture.
        # The rest of conf.py would query Git and configure third-party tools.
        exec(compile(ast.Module(body=statements, type_ignores=[]), str(CONFIGURATION), "exec"), scope)
        return scope["exclude_patterns"]

    def test_output_directories(self):
        cold = self.exclusions()
        for name in ("html", "doctrees", "xml", "pycache"):
            (self.generated / name).mkdir()
        warm = self.exclusions()

        self.assertEqual(cold, warm)
        self.assertTrue(any(fnmatchcase("generation/generated/html/old.rst", pattern) for pattern in warm))
        self.assertFalse(any(fnmatchcase("generation/generated/api/class.rst", pattern) for pattern in warm))

    def test_staging_backup(self):
        before = self.exclusions()
        (self.generated / ".api-previous").mkdir()
        staged = self.exclusions()

        self.assertEqual(before, staged)
        self.assertTrue(any(fnmatchcase("generation/generated/.api-previous/old.rst", pattern) for pattern in staged))


class ThemeTests(unittest.TestCase):
    def test_sidebar_branches(self):
        tree = ast.parse(CONFIGURATION.read_text(encoding="utf-8"))
        setting = next(statement.value for statement in tree.body
                       if isinstance(statement, ast.Assign)
                       and any(isinstance(target, ast.Name) and target.id == "html_theme_options"
                               for target in statement.targets))
        options = ast.literal_eval(setting)
        self.assertFalse(options["collapse_navigation"],
                         "Inactive branches need child HTML for their expand buttons")
        self.assertGreaterEqual(options["navigation_depth"], 4)


class MermaidTests(unittest.TestCase):
    def test_local_modules(self):
        tree = ast.parse(CONFIGURATION.read_text(encoding="utf-8"))
        settings = {
            target.id: ast.literal_eval(statement.value)
            for statement in tree.body if isinstance(statement, ast.Assign)
            for target in statement.targets if isinstance(target, ast.Name)
            and target.id in {"extensions", "myst_fence_as_directive",
                              "mermaid_output_format", "mermaid_use_local"}
        }
        self.assertIn("sphinxcontrib.mermaid", settings["extensions"])
        self.assertIn("mermaid", settings["myst_fence_as_directive"])
        self.assertEqual(settings["mermaid_output_format"], "raw")
        entry = CONFIGURATION.parent / "static" / settings["mermaid_use_local"]
        self.assertTrue(entry.is_file())
        bundle = entry.parent.resolve()
        upstream = json.loads((bundle / "UPSTREAM.json").read_text(encoding="utf-8"))
        self.assertEqual(bundle.name, "mermaid-" + upstream["version"])
        imports = 0
        for module in bundle.rglob("*.mjs"):
            references = re.findall(
                r"""\b(?:from\s*|import\s*(?:\(\s*)?)["'](\.{1,2}/[^"']+\.mjs)["']""",
                module.read_text(encoding="utf-8"),
            )
            for reference in references:
                with self.subTest(module=module.name, dependency=reference):
                    target = (module.parent / reference).resolve()
                    self.assertTrue(target.is_relative_to(bundle))
                    self.assertTrue(target.is_file(), "Missing bundled JavaScript module")
                imports += 1
        self.assertGreater(imports, 0)


if __name__ == "__main__":
    unittest.main()
