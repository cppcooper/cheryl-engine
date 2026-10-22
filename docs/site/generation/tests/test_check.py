"""Documentation output regressions using files and Git objects in a temp fixture."""

from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET


TOOLS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_ROOT))
try:
    spec = importlib.util.spec_from_file_location("cheryl_docs_check", TOOLS_ROOT / "check.py")
    checker = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(checker)
finally:
    sys.path.pop(0)

PRIVATE_ID = "classPublic_1secret"


class CheckTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="cheryl-docs-check-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.html = self.root / "html"
        self.xml = self.root / "xml"
        self.hooks = self.root / "fixture-hooks"
        self.hooks.mkdir()
        self.headers = [self.root / "include/cheryl/first.h", self.root / "include/cheryl/second.h"]
        for header in self.headers:
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text("#pragma once\nstruct Fixture {};\n", encoding="utf-8")
        self.extra_header = self.root / "internal/private.h"
        self.extra_header.parent.mkdir()
        self.extra_header.write_text("#pragma once\n", encoding="utf-8")
        (self.root / ".gitignore").write_text("html/\nxml/\nfixture-hooks/\n", encoding="utf-8")
        (self.root / "README.md").write_text("# Initial fixture\n", encoding="utf-8")
        isolated_environment = {
            name: value for name, value in os.environ.items() if not name.startswith("GIT_")
        }
        isolated_environment.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)
        self.enterContext(patch.dict(os.environ, isolated_environment, clear=True))
        self.git("init", "-q", "--initial-branch=main")
        self.git("add", ".gitignore", "README.md", "include", "internal")
        self.git("commit", "-q", "-m", "Adds fixture sources")
        self.previous = self.git("rev-parse", "HEAD")
        (self.root / "README.md").write_text("# Current fixture\n", encoding="utf-8")
        self.git("add", "README.md")
        self.git("commit", "-q", "-m", "Updates fixture revision")
        self.commit = self.git("rev-parse", "HEAD")
        self.enterContext(patch.object(checker, "REPO_ROOT", self.root))
        self.enterContext(patch.object(checker, "header_inputs", return_value=self.headers))
        self.write_metadata()
        self.write_xml()
        self.write_html()

    def git(self, *arguments):
        result = subprocess.run(
            ["git", "-c", "user.name=cppcooper", "-c",
             "user.email=cppcooper@users.noreply.github.com", "-c", "commit.gpgsign=false",
             "-c", f"core.hooksPath={self.hooks}", *arguments],
            cwd=self.root, capture_output=True, text=True, check=True,
        )
        return result.stdout.strip()

    def write_metadata(self, *, revision=None, dirty=False):
        self.html.mkdir(exist_ok=True)
        metadata = {
            "source_revision": self.commit if revision is None else revision,
            "source_ref": "refs/heads/main",
            "dirty": dirty,
            "header_count": len(self.headers),
        }
        (self.html / "revision.json").write_text(json.dumps(metadata), encoding="utf-8")

    def write_xml(self, headers=None):
        self.xml.mkdir(exist_ok=True)
        index = ET.Element("doxygenindex")
        for number, header in enumerate(self.headers if headers is None else headers):
            identifier = f"file_{number}"
            entry = ET.SubElement(index, "compound", kind="file", refid=identifier)
            ET.SubElement(entry, "name").text = header.name
            document = ET.Element("doxygen")
            declaration = ET.SubElement(document, "compounddef", kind="file", id=identifier)
            ET.SubElement(declaration, "location", file=header.relative_to(self.root).as_posix())
            ET.ElementTree(document).write(self.xml / f"{identifier}.xml", encoding="utf-8")
        entry = ET.SubElement(index, "compound", kind="class", refid="classPublic")
        ET.SubElement(entry, "name").text = "Fixture"
        document = ET.Element("doxygen")
        declaration = ET.SubElement(document, "compounddef", kind="class", id="classPublic")
        section = ET.SubElement(declaration, "sectiondef", kind="private-attrib")
        member = ET.SubElement(section, "memberdef", prot="private", id=PRIVATE_ID)
        ET.SubElement(member, "name").text = "secret"
        ET.ElementTree(document).write(self.xml / "classPublic.xml", encoding="utf-8")
        ET.ElementTree(index).write(self.xml / "index.xml", encoding="utf-8")

    def write_html(self):
        guide = self.html / "guide"
        guide.mkdir(exist_ok=True)
        static = self.html / "_static"
        fonts = static / "fonts"
        fonts.mkdir(parents=True, exist_ok=True)
        (fonts / "fixture.woff2").write_bytes(b"fixture font")
        (static / "icon.svg").write_text('<svg xmlns="http://www.w3.org/2000/svg"/>', encoding="utf-8")
        (static / "site.css").write_text(
            '@font-face { src: url("fonts/fixture.woff2?v=1"); }', encoding="utf-8"
        )
        source = "https://github.com/cppcooper/cheryl-engine"
        (self.html / "index.html").write_text(
            f'''<!doctype html><html><head><link href="_static/site.css" rel="stylesheet"></head>
<body><h1 id="top">Fixture</h1>
<a href="guide/setup.html#start">Setup</a>
<a href="guide/space%20page.html#caf%C3%A9">Encoded page and anchor</a>
<a href="guide/">Guide index</a><a href="#top">Top</a>
<form action="guide/setup.html"></form><img src="_static/icon.svg">
<a href="{source}/tree/{self.commit}">Repository root</a>
<a href="{source}/tree/{self.commit}/include/cheryl">Headers</a>
<a href="{source}/blob/{self.commit}/include/cheryl/first.h">Declaring header</a>
</body></html>''', encoding="utf-8",
        )
        (guide / "index.html").write_text(
            '<html><body><a href="setup.html#start">Setup</a></body></html>', encoding="utf-8"
        )
        (guide / "setup.html").write_text(
            '<html><head><link href="../_static/site.css" rel="stylesheet"></head>'
            '<body><h1 id="start">Setup</h1><a href="../index.html#top">Home</a>'
            '<img src="../_static/icon.svg"></body></html>', encoding="utf-8",
        )
        (guide / "space page.html").write_text(
            '<html><body><h1 id="café">Encoded anchor</h1></body></html>', encoding="utf-8"
        )

    def add_html(self, markup):
        page = self.html / "index.html"
        page.write_text(
            page.read_text(encoding="utf-8").replace("</body>", markup + "</body>"),
            encoding="utf-8",
        )

    def test_valid(self):
        result = checker.check(self.html, self.xml, require_clean=True)
        self.assertEqual(result["source_revision"], self.commit)
        self.assertEqual(result["public_headers"], 2)
        self.assertEqual(result["source_objects"], 3)
        self.assertEqual(result["html_pages"], 4)

    def test_missing_links(self):
        for target, failure in (
            ("missing.html", "Missing local target"),
            ("guide/setup.html#missing", "Missing local fragment"),
        ):
            with self.subTest(target=target):
                self.write_html()
                self.add_html(f'<a href="{target}">Broken link</a>')
                with self.assertRaisesRegex(ValueError, failure):
                    checker.check(self.html, self.xml)

    def test_missing_css_asset(self):
        (self.html / "_static/fonts/fixture.woff2").unlink()
        with self.assertRaisesRegex(ValueError, "Missing or prefix-dependent CSS asset"):
            checker.check(self.html, self.xml)

    def test_root_relative(self):
        for markup in ('<a href="/guide/setup.html">Setup</a>', '<form action="/guide/setup.html"></form>'):
            with self.subTest(markup=markup):
                self.write_html()
                self.add_html(markup)
                with self.assertRaisesRegex(ValueError, "Root-relative link breaks version/project prefixes"):
                    checker.check(self.html, self.xml)

    def test_stale_revision(self):
        self.write_metadata(revision=self.previous)
        with self.assertRaisesRegex(ValueError, "Published source revision .* differs from Git HEAD"):
            checker.check(self.html, self.xml)

    def test_missing_header(self):
        self.write_xml(headers=self.headers[:1])
        with self.assertRaisesRegex(ValueError, "Public header missing from XML"):
            checker.check(self.html, self.xml)

    def test_extra_header(self):
        self.write_xml(headers=[*self.headers, self.extra_header])
        with self.assertRaisesRegex(ValueError, "Unexpected extracted header"):
            checker.check(self.html, self.xml)

    def test_private_member(self):
        self.add_html(f'<section id="{PRIVATE_ID}">Private member</section>')
        with self.assertRaisesRegex(ValueError, "Private member rendered"):
            checker.check(self.html, self.xml)

    def test_dirty_source(self):
        self.write_metadata(dirty=True)
        self.assertTrue(checker.check(self.html, self.xml)["dirty"])
        with self.assertRaisesRegex(ValueError, "Publication requires a clean source checkout"):
            checker.check(self.html, self.xml, require_clean=True)


if __name__ == "__main__":
    unittest.main()
