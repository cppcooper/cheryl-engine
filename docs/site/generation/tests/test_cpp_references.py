"""Doxygen-backed reference regressions without Sphinx or generated output."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET


def make_reference(builder, source, document, anchor, content):
    return {"source": source, "document": document, "anchor": anchor, "content": content}


docutils = types.ModuleType("docutils")
docutils.nodes = types.SimpleNamespace()
sphinx = types.ModuleType("sphinx")
sphinx.__path__ = []
sphinx_util = types.ModuleType("sphinx.util")
sphinx_util.__path__ = []
sphinx_nodes = types.ModuleType("sphinx.util.nodes")
sphinx_nodes.make_refnode = make_reference
with patch.object(sys, "dont_write_bytecode", True), patch.dict(sys.modules, {
    "docutils": docutils,
    "sphinx": sphinx,
    "sphinx.util": sphinx_util,
    "sphinx.util.nodes": sphinx_nodes,
}):
    source = Path(__file__).resolve().parents[1] / "extensions/cpp_references.py"
    spec = importlib.util.spec_from_file_location("cheryl_cpp_references", source)
    references = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(references)


class EnumeratorTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="cheryl-cpp-references-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.index = ET.Element("doxygenindex")
        self.compounds = {}
        self.environment = types.SimpleNamespace(found_docs=set(), docname="guide")
        self.app = types.SimpleNamespace(builder=object())
        self.repository = types.SimpleNamespace(root=self.root)
        ET.ElementTree(ET.Element("doxygenconfig")).write(self.root / "Doxyfile.xml", encoding="utf-8")

    def add_enum(self, *, owner="CE::Memory", kind="namespace", name="FitPolicy", scoped=False,
                 visibility="public", value_visibility="public", value="exact"):
        owner_id = kind + owner.replace("::", "_")
        if owner_id not in self.compounds:
            entry = ET.SubElement(self.index, "compound", kind=kind, refid=owner_id)
            ET.SubElement(entry, "name").text = owner
            compound = ET.Element("compounddef", id=owner_id, kind=kind)
            ET.SubElement(compound, "compoundname").text = owner
            self.compounds[owner_id] = compound
        compound = self.compounds[owner_id]
        enum_id = owner_id + "_" + name
        member = ET.SubElement(compound, "memberdef", kind="enum", id=enum_id,
                               prot=visibility, strong="yes" if scoped else "no")
        ET.SubElement(member, "name").text = name
        ET.SubElement(member, "qualifiedname").text = owner + "::" + name
        value_id = enum_id + "_" + value
        enumerator = ET.SubElement(member, "enumvalue", id=value_id, prot=value_visibility)
        ET.SubElement(enumerator, "name").text = value
        document = "generation/generated/api/" + ("enum_" + enum_id if kind == "namespace" else owner_id)
        self.environment.found_docs.add(document)
        return document, value_id

    def load(self):
        ET.ElementTree(self.index).write(self.root / "index.xml", encoding="utf-8")
        for identifier, compound in self.compounds.items():
            document = ET.Element("doxygen")
            document.append(compound)
            ET.ElementTree(document).write(self.root / (identifier + ".xml"), encoding="utf-8")
        return references.CppReferences(self.root, self.repository)

    def resolve(self, index, target):
        return index.resolve(self.app, self.environment,
                             {"refdoc": "guide", "reftarget": target}, "value")

    def assert_target(self, result, target):
        self.assertIsNotNone(result)
        self.assertEqual((result["document"], result["anchor"]), target)

    def test_unscoped(self):
        target = self.add_enum()
        index = self.load()

        # Native C++ lookup may have lost an unscoped alias after an incremental
        # reread; the fallback still reaches the actual documented enumerator.
        self.assert_target(self.resolve(index, "::CE::Memory::exact"), target)
        self.assert_target(self.resolve(index, "CE::Memory::FitPolicy::exact"), target)

    def test_scoped(self):
        target = self.add_enum(name="Mode", scoped=True, value="Quiet")
        index = self.load()

        self.assert_target(self.resolve(index, "CE::Memory::Mode::Quiet"), target)
        self.assertIsNone(self.resolve(index, "CE::Memory::Quiet"))

    def test_member_enum(self):
        target = self.add_enum(owner="CE::Pool", kind="struct")
        index = self.load()

        self.assert_target(self.resolve(index, "CE::Pool::exact"), target)
        self.assert_target(self.resolve(index, "CE::Pool::FitPolicy::exact"), target)

    def test_private_enum(self):
        self.add_enum(owner="CE::Pool", kind="struct", visibility="private")
        index = self.load()

        self.assertIsNone(self.resolve(index, "CE::Pool::exact"))
        self.assertIsNone(self.resolve(index, "CE::Pool::FitPolicy::exact"))

    def test_private_value(self):
        self.add_enum(value_visibility="private")
        index = self.load()

        self.assertIsNone(self.resolve(index, "CE::Memory::exact"))
        self.assertIsNone(self.resolve(index, "CE::Memory::FitPolicy::exact"))

    def test_anonymous(self):
        target = self.add_enum(name="@0")
        index = self.load()

        self.assert_target(self.resolve(index, "CE::Memory::exact"), target)
        self.assertIsNone(self.resolve(index, "CE::Memory::@0::exact"))

    def test_missing_page(self):
        target = self.add_enum()
        self.environment.found_docs.remove(target[0])
        index = self.load()

        self.assertIsNone(self.resolve(index, "CE::Memory::exact"))
        self.assertIsNone(self.resolve(index, "CE::Memory::FitPolicy::exact"))


if __name__ == "__main__":
    unittest.main()
