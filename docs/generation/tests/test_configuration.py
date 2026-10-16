"""Configuration regressions without importing Sphinx or generating documentation."""

import ast
from fnmatch import fnmatchcase
from pathlib import Path
import re
import shlex
import tempfile
import unittest


CONFIGURATION = Path(__file__).resolve().parents[1] / "conf.py"
EXPORT_HEADER = CONFIGURATION.parents[2] / "projects/engine/include/cheryl/export.h"


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


class ExclusionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="cheryl-docs-configuration-")
        self.addCleanup(temporary.cleanup)
        self.docs = Path(temporary.name) / "docs"
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
            "DOCS_ROOT": self.docs,
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


if __name__ == "__main__":
    unittest.main()
