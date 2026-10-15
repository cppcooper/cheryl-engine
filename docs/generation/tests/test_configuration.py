"""Configuration regressions without importing Sphinx or generating documentation."""

import ast
from fnmatch import fnmatchcase
from pathlib import Path
import tempfile
import unittest


CONFIGURATION = Path(__file__).resolve().parents[1] / "conf.py"


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
