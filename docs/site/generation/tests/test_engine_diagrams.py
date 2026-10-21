"""Check the source/catalog contract and explorer publication without an Engine build."""

import json
from pathlib import Path
import sys
import tempfile
import unittest

GENERATION = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GENERATION))
from diagram_catalog import api_targets, load_catalog, write_catalog


def fixture():
    return {
        "schema_version": 1, "mermaid": "11.12.1", "systems": [{"id": "runtime", "title": "Runtime"}],
        "symbols": {
            "Runtime::Context": {"key": "Runtime::Context", "name": "CE::Runtime::Context", "label": "Runtime::Context"},
            "%application": {"key": "%application", "label": "Application"},
        },
        "diagrams": [{
            "id": "runtime-application", "nodes": ["Runtime::Context", "%application"],
            "edges": [{"source": "%application", "target": "Runtime::Context"}],
        }],
    }


SOURCE = 'flowchart LR\n    k0["Runtime::Context"]\n    k1(["Application"])\n    k1 --> k0\n'


class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="cheryl-diagram-tests-")
        self.addCleanup(self.temporary.cleanup)
        self.source = Path(self.temporary.name)

    def write(self, catalog=None, source=SOURCE):
        (self.source / "catalog.json").write_text(json.dumps(catalog or fixture()))
        (self.source / "runtime-application.mmd").write_text(source)

    def test_reviewed_catalog(self):
        catalog = load_catalog(GENERATION.parent / "engine-systems/diagrams")
        for system in catalog["systems"]:
            views = {d["view"] for d in catalog["diagrams"] if d["system"] == system["id"]}
            self.assertTrue({"internal", "application"} <= views, system["id"])

    def test_changed_arrow_needs_matching_evidence(self):
        self.write(source=SOURCE.replace("k1 --> k0", "k0 --> k1"))
        with self.assertRaisesRegex(ValueError, "arrows differ"):
            load_catalog(self.source)

    def test_node_order_is_inspector_identity(self):
        self.write(source=SOURCE.replace('k0["', 'k2["'))
        with self.assertRaisesRegex(ValueError, "node order"):
            load_catalog(self.source)

    def test_unknown_dependency(self):
        catalog = fixture()
        catalog["diagrams"][0]["edges"][0]["target"] = "missing"
        self.write(catalog)
        with self.assertRaisesRegex(ValueError, "Unknown edge endpoint"):
            load_catalog(self.source)

    def test_invalid_id_stays_inside_source(self):
        catalog = fixture()
        catalog["diagrams"][0]["id"] = "../elsewhere"
        self.write(catalog)
        with self.assertRaisesRegex(ValueError, "Invalid or duplicate"):
            load_catalog(self.source)


class PublicationTests(unittest.TestCase):
    def test_exact_unambiguous_api_links(self):
        objects = [
            ("CE::Runtime::Context", "", "class", "api/context", "context", 1),
            ("CE::operation", "", "function", "api/op", "overload-a", 1),
            ("CE::operation", "", "function", "api/op", "overload-b", 1),
        ]
        self.assertEqual(api_targets(objects), {"CE::Runtime::Context": ("api/context", "context")})

    def test_relocatable_catalog_and_mermaid_downloads(self):
        with tempfile.TemporaryDirectory(prefix="cheryl-diagram-publication-") as directory:
            root = Path(directory)
            source = root / "source/engine-systems/diagrams"
            source.mkdir(parents=True)
            catalog = fixture()
            (source / "catalog.json").write_text(json.dumps(catalog))
            (source / "runtime-application.mmd").write_text(SOURCE)
            destination = root / "html/_static/engine-diagrams"
            objects = [("CE::Runtime::Context", "", "class", "generation/generated/api/context", "anchor", 1)]
            write_catalog(source, destination, objects, lambda name: name + ".html",
                          "https://github.com/cppcooper/cheryl-engine")
            result = json.loads((destination / "catalog.json").read_text())
            diagram = result["diagrams"][0]
            self.assertEqual(diagram["source"], SOURCE)
            self.assertEqual(diagram["nodes"][0]["api"], "generation/generated/api/context.html#anchor")
            self.assertNotIn("api", diagram["nodes"][1])
            self.assertNotIn("svg", diagram)
            self.assertEqual(result["metadata"]["docsRoot"], "../../")
            self.assertEqual(result["metadata"]["revisionUrl"], "../../revision.json")
            self.assertEqual((destination / "runtime-application.mmd").read_text(), SOURCE)
            self.assertEqual(catalog, fixture(), "Compilation must not change authored metadata")


if __name__ == "__main__":
    unittest.main()
