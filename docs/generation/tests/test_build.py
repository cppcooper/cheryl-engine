"""Incremental build regressions with temporary outputs and simulated tools."""

from __future__ import annotations

from contextlib import nullcontext, redirect_stderr, redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


TOOLS_ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="cheryl-docs-build-import-") as cache:
    # Importing the wrapper must not put bytecode in a real documentation tree.
    with patch.object(sys, "pycache_prefix", str(Path(cache) / "pycache")), patch.dict(
        os.environ, {"PYTHONPYCACHEPREFIX": str(Path(cache) / "pycache")}
    ):
        sys.path.insert(0, str(TOOLS_ROOT))
        try:
            spec = importlib.util.spec_from_file_location("cheryl_docs_build", TOOLS_ROOT / "build.py")
            builder = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(builder)
        finally:
            sys.path.pop(0)


class BuildTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="cheryl-docs-build-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve() / "checkout"
        self.tools = self.root / "docs/generation"
        self.output = self.tools / "generated"
        self.include = self.root / "projects/engine/include"
        self.header = self.include / "cheryl/fixture.h"
        self.header.parent.mkdir(parents=True)
        self.header.write_text("#pragma once\nstruct Fixture {};\n", encoding="utf-8")
        self.tools.mkdir(parents=True)
        for filename in ("build.py", "Doxyfile.in"):
            (self.tools / filename).write_bytes((TOOLS_ROOT / filename).read_bytes())
        (self.tools / "check.py").write_text("# Simulated output checker\n", encoding="utf-8")
        (self.tools / "extensions").mkdir()
        (self.tools / "extensions/fixture.py").write_text("# Fixture extension\n", encoding="utf-8")
        (self.tools / "templates").mkdir()
        (self.tools / "templates/layout.html").write_text("{{ body }}\n", encoding="utf-8")
        (self.tools / "static").mkdir()
        self.stylesheet = self.tools / "static/site.css"
        self.stylesheet.write_text("body { font-size: 100%; }\n", encoding="utf-8")
        self.guide = self.root / "docs/guide.md"
        self.guide.write_text("# Guide\nInitial instructions.\n", encoding="utf-8")
        self.doxygen = self.root / "tools/doxygen"
        self.doxygen.parent.mkdir()
        self.doxygen.write_bytes(b"simulated Doxygen executable")
        self.state = self.output / "build-state.json"
        self.html_marker = self.output / "html/retained.html"
        self.doctree_marker = self.output / "doctrees/retained.doctree"
        self.documents = {
            "index.xml": '<doxygenindex><compound refid="classFixture" kind="class">'
                         "<name>Fixture</name></compound></doxygenindex>\n",
            "classFixture.xml": '<doxygen><compounddef id="classFixture" kind="class">'
                                "<compoundname>Fixture</compoundname></compounddef></doxygen>\n",
        }
        self.metadata = {
            "source_revision": "1" * 40,
            "dirty": False,
            "tools": {"python": "fixture", "python_source": "system", "packages": {"Sphinx": "8.2.3"}},
            "requirements_sha256": "fixture requirements",
            "native_manifest_sha256": "fixture native tools",
        }
        self.sphinx_runs = []
        self.check_runs = []
        self.check_states = []
        self.fail_sphinx = False
        self.fail_check = False
        self.last_output = ""
        self.enterContext(patch.dict(os.environ, {
            "CHERYL_DOCS_TOOLS_ACTIVE": "1",
            "CHERYL_DOCS_DOXYGEN": str(self.doxygen),
        }, clear=True))
        for name, value in (
            ("REPO_ROOT", self.root),
            ("GENERATION_ROOT", self.tools),
            ("GENERATED_ROOT", self.output),
            ("PUBLIC_INCLUDE_ROOTS", (self.include,)),
            ("__file__", str(self.tools / "build.py")),
        ):
            self.enterContext(patch.object(builder, name, value))
        self.enterContext(patch.object(builder, "header_inputs", return_value=[self.header]))
        self.enterContext(patch.object(builder, "revision", return_value=self.metadata["source_revision"]))
        self.enterContext(patch.object(builder, "provenance", return_value=self.metadata))
        self.enterContext(patch.object(builder, "validate_doxygen", return_value="1.18.0"))
        self.enterContext(patch.object(builder, "file_lock", side_effect=lambda path: nullcontext()))
        self.extractor = self.enterContext(patch.object(builder, "extract", side_effect=self.extract))
        self.enterContext(patch.object(builder.subprocess, "run", side_effect=self.run_tool))

    def extract(self, output, doxygen, commit):
        xml = output / "xml"
        xml.mkdir(parents=True, exist_ok=True)
        for name, contents in self.documents.items():
            (xml / name).write_text(contents, encoding="utf-8")

    def run_tool(self, command, **options):
        if command[1:3] == ["-m", "sphinx"]:
            output = Path(command[-1])
            doctrees = Path(command[command.index("-d") + 1])
            environment = doctrees / "environment.pickle"
            self.sphinx_runs.append({
                "command": command,
                "environment": environment.read_bytes() if environment.exists() else None,
                "html_retained": self.html_marker.exists(),
                "doctrees_retained": self.doctree_marker.exists(),
                "incremental": options["env"]["CHERYL_DOCS_INCREMENTAL"],
            })
            output.mkdir(parents=True, exist_ok=True)
            doctrees.mkdir(parents=True, exist_ok=True)
            (output / "index.html").write_text("<html><body>Fixture</body></html>\n", encoding="utf-8")
            environment.write_text(f"environment-{len(self.sphinx_runs)}", encoding="utf-8")
            if self.fail_sphinx:
                self.fail_sphinx = False
                # A warning failure can leave apparently complete cached output.
                raise subprocess.CalledProcessError(1, command)
        elif len(command) > 1 and Path(command[1]) == self.tools / "check.py":
            self.check_runs.append(command)
            self.check_states.append(json.loads(self.state.read_text(encoding="utf-8")))
            if self.fail_check:
                self.fail_check = False
                raise subprocess.CalledProcessError(1, command)
        else:
            raise AssertionError(f"Unexpected external tool: {command!r}")
        return subprocess.CompletedProcess(command, 0)

    def run_build(self, *arguments):
        output = io.StringIO()
        try:
            with patch.object(sys, "argv", [str(self.tools / "build.py"), *arguments]), \
                    redirect_stdout(output), redirect_stderr(io.StringIO()):
                return builder.main()
        finally:
            self.last_output = output.getvalue()

    def retain_markers(self):
        self.html_marker.write_text("Existing HTML\n", encoding="utf-8")
        self.doctree_marker.write_text("Existing doctree\n", encoding="utf-8")

    def test_warm_reuse(self):
        self.assertEqual(self.run_build(), 0)
        self.retain_markers()
        self.guide.write_text("# Guide\nRevised instructions.\n", encoding="utf-8")

        self.assertEqual(self.run_build(), 0)

        self.assertEqual(self.extractor.call_count, 1)
        self.assertEqual(self.sphinx_runs[-1]["environment"], b"environment-1")
        self.assertTrue(self.sphinx_runs[-1]["html_retained"])
        self.assertTrue(self.sphinx_runs[-1]["doctrees_retained"])
        self.assertNotIn("-a", self.sphinx_runs[-1]["command"])
        self.assertEqual(len(self.check_runs), 2)
        self.assertTrue(self.state.is_file())
        self.assertIn("Updating Sphinx output incrementally.", self.last_output)

    def test_header_change(self):
        self.run_build()
        self.header.write_text("#pragma once\nstruct Fixture { int value; };\n", encoding="utf-8")

        self.run_build()

        self.assertEqual(self.extractor.call_count, 2)
        self.assertEqual(len(self.check_runs), 2)

    def test_removed_compound(self):
        original_index = self.documents["index.xml"]
        self.documents["index.xml"] = original_index.replace(
            "</doxygenindex>",
            '<compound refid="classObsolete" kind="class"><name>Obsolete</name></compound></doxygenindex>',
        )
        self.documents["classObsolete.xml"] = '<doxygen><compounddef id="classObsolete"/></doxygen>\n'
        self.run_build()
        self.retain_markers()
        self.documents["index.xml"] = original_index
        self.documents.pop("classObsolete.xml")
        self.header.write_text("#pragma once\nstruct Fixture { int replacement; };\n", encoding="utf-8")

        self.run_build()

        self.assertEqual(self.extractor.call_count, 2)
        self.assertIsNone(self.sphinx_runs[-1]["environment"])
        self.assertFalse(self.sphinx_runs[-1]["html_retained"])
        self.assertFalse(self.sphinx_runs[-1]["doctrees_retained"])
        self.assertFalse((self.output / "xml/classObsolete.xml").exists())

    def test_css_rewrites(self):
        self.run_build()
        self.retain_markers()
        self.stylesheet.write_text("body { font-size: 120%; }\n", encoding="utf-8")

        self.run_build()

        self.assertEqual(self.extractor.call_count, 1)
        self.assertIn("-a", self.sphinx_runs[-1]["command"])
        self.assertEqual(self.sphinx_runs[-1]["environment"], b"environment-1")
        self.assertTrue(self.sphinx_runs[-1]["html_retained"])
        self.assertTrue(self.sphinx_runs[-1]["doctrees_retained"])

    def test_failed_run_retries(self):
        self.run_build()
        self.retain_markers()
        self.fail_sphinx = True
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_build()
        state = json.loads(self.state.read_text(encoding="utf-8"))
        self.assertIn("extraction", state)
        self.assertNotIn("rendering", state)
        self.assertNotIn("presentation", state)
        self.assertEqual((self.output / "doctrees/environment.pickle").read_bytes(), b"environment-2")

        self.run_build()

        self.assertEqual(self.extractor.call_count, 1)
        self.assertIsNone(self.sphinx_runs[-1]["environment"])
        self.assertFalse(self.sphinx_runs[-1]["html_retained"])
        self.assertFalse(self.sphinx_runs[-1]["doctrees_retained"])
        self.assertEqual(len(self.check_runs), 2)
        self.assertTrue(self.state.is_file())
        self.assertIn("no completed Sphinx stage", self.last_output)

    def test_failed_checks_reuse(self):
        self.fail_check = True
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_build()
        state = json.loads(self.state.read_text(encoding="utf-8"))
        self.assertIn("rendering", state)
        self.assertIn("presentation", state)
        self.assertIn("rendering", self.check_states[-1])
        self.retain_markers()

        self.run_build()

        self.assertEqual(self.extractor.call_count, 1)
        self.assertEqual(len(self.check_runs), 2)
        self.assertEqual(self.sphinx_runs[-1]["environment"], b"environment-1")
        self.assertTrue(self.sphinx_runs[-1]["html_retained"])
        self.assertTrue(self.sphinx_runs[-1]["doctrees_retained"])
        self.assertIn("Updating Sphinx output incrementally.", self.last_output)

    def test_extract_only(self):
        self.run_build("--extract-only")
        state = json.loads(self.state.read_text(encoding="utf-8"))
        self.assertIn("extraction", state)
        self.assertNotIn("rendering", state)
        self.assertEqual(self.sphinx_runs, [])
        self.assertEqual(self.check_runs, [])

        self.run_build()

        self.assertEqual(self.extractor.call_count, 1)
        self.assertEqual(len(self.check_runs), 1)
        self.assertIn("Reusing cached Doxygen XML.", self.last_output)

    def test_warm_extract_only(self):
        self.run_build()
        original = self.state.read_bytes()

        self.run_build("--extract-only")

        self.assertEqual(self.state.read_bytes(), original)
        self.assertEqual(self.extractor.call_count, 1)
        self.assertEqual(len(self.sphinx_runs), 1)
        self.assertEqual(len(self.check_runs), 1)

    def test_initial_reasons(self):
        self.run_build()

        for reason in ("no completed Doxygen stage", "no completed Sphinx stage",
                       "cached Sphinx environment is missing", "cached HTML entry point is missing",
                       "Doxygen extraction took", "Sphinx rendering took", "Documentation checks took"):
            self.assertIn(reason, self.last_output)

    def test_revision_reason(self):
        self.run_build()
        self.metadata["source_revision"] = "2" * 40

        with patch.object(builder, "revision", return_value=self.metadata["source_revision"]):
            self.run_build()

        self.assertIn("rendering inputs changed (revision)", self.last_output)

    def test_missing_environment(self):
        self.run_build()
        (self.output / "doctrees/environment.pickle").unlink()

        self.run_build()

        self.assertEqual(self.extractor.call_count, 1)
        self.assertIsNone(self.sphinx_runs[-1]["environment"])
        self.assertIn("cached Sphinx environment is missing", self.last_output)

    def test_missing_html(self):
        self.run_build()
        (self.output / "html/index.html").unlink()

        self.run_build()

        self.assertEqual(self.extractor.call_count, 1)
        self.assertIsNone(self.sphinx_runs[-1]["environment"])
        self.assertIn("cached HTML entry point is missing", self.last_output)

    def test_clean(self):
        self.run_build()
        self.retain_markers()

        self.run_build("--clean")

        self.assertEqual(self.extractor.call_count, 2)
        self.assertIsNone(self.sphinx_runs[-1]["environment"])
        self.assertFalse(self.sphinx_runs[-1]["html_retained"])
        self.assertFalse(self.sphinx_runs[-1]["doctrees_retained"])
        self.assertEqual(self.sphinx_runs[-1]["incremental"], "0")
        self.assertIn("--clean requested", self.last_output)

    def test_jobs(self):
        for arguments, expected in (((), "1"), (("--jobs", "4"), "4")):
            with self.subTest(arguments=arguments):
                self.assertEqual(self.run_build(*arguments), 0)
                command = self.sphinx_runs[-1]["command"]
                self.assertEqual(command[command.index("-j") + 1], expected)

    def test_bootstrap_jobs(self):
        with patch.dict(os.environ, {"CHERYL_DOCS_TOOLS_ACTIVE": ""}), \
                patch.object(builder.subprocess, "call", return_value=0) as bootstrap:
            self.assertEqual(self.run_build("--jobs", "4"), 0)
        self.assertEqual(bootstrap.call_args.args[0][-2:], ["--jobs", "4"])
        self.extractor.assert_not_called()

    def test_invalid_jobs(self):
        for jobs in ("0", "-2", "many"):
            with self.subTest(jobs=jobs), self.assertRaises(SystemExit) as error:
                self.run_build("--jobs", jobs)
            self.assertEqual(error.exception.code, 2)
        self.extractor.assert_not_called()
        self.assertEqual(self.sphinx_runs, [])

    def test_xml_preserves_unchanged(self):
        self.documents["obsolete.xml"] = "<obsolete/>\n"
        self.run_build()
        index = self.output / "xml/index.xml"
        os.utime(index, ns=(1_600_000_000_000_000_000, 1_600_000_000_000_000_000))
        original_mtime = index.stat().st_mtime_ns
        self.documents.pop("obsolete.xml")
        self.documents["classFixture.xml"] = '<doxygen><compounddef id="replacement"/></doxygen>\n'

        builder.refresh_xml(self.output, str(self.doxygen), self.metadata["source_revision"], incremental=True)

        self.assertEqual(index.stat().st_mtime_ns, original_mtime)
        self.assertEqual((self.output / "xml/classFixture.xml").read_text(encoding="utf-8"),
                         self.documents["classFixture.xml"])
        self.assertFalse((self.output / "xml/obsolete.xml").exists())
        self.assertFalse((self.output / ".xml-previous").exists())

    def test_xml_failure_restores(self):
        self.run_build()
        original = {path.name: path.read_bytes() for path in (self.output / "xml").iterdir()}

        def fail(output, doxygen, commit):
            xml = output / "xml"
            xml.mkdir()
            (xml / "index.xml").write_text("partial output", encoding="utf-8")
            raise RuntimeError("Extraction interrupted")

        self.extractor.side_effect = fail
        with self.assertRaisesRegex(RuntimeError, "Extraction interrupted"):
            builder.refresh_xml(self.output, str(self.doxygen), self.metadata["source_revision"], incremental=True)

        self.assertEqual({path.name: path.read_bytes() for path in (self.output / "xml").iterdir()}, original)
        self.assertFalse((self.output / ".xml-previous").exists())

    def test_reserved_outputs(self):
        for name in ("api", ".api-previous", ".xml-previous", "pycache"):
            for relative in (name, name + "/nested"):
                with self.subTest(output=relative):
                    with self.assertRaises(SystemExit) as error:
                        self.run_build("--output", str(self.output / relative))
                    self.assertEqual(error.exception.code, 2)
                    self.assertFalse((self.output / relative).exists())
        self.extractor.assert_not_called()
        self.assertEqual(self.sphinx_runs, [])


if __name__ == "__main__":
    unittest.main()
