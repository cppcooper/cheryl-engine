"""Check the source/catalog contract and explorer publication without an Engine build."""

import ast
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest

GENERATION = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GENERATION))
from diagram_catalog import api_targets, frame_version, load_catalog, version_frame_assets, write_catalog


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


def operation():
    return {"name": "CE::Runtime::Context::events", "label": "events()", "role": "call",
            "file": "projects/engine/include/cheryl/core/engine/engine-context.h", "line": 129,
            "brief": "Access the context-owned event bus."}


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

    def test_method_labels_must_appear_in_the_owning_box(self):
        catalog = fixture()
        catalog["diagrams"][0]["operations"] = {"Runtime::Context": [operation()]}
        self.write(catalog)
        with self.assertRaisesRegex(ValueError, "omits operation label"):
            load_catalog(self.source)
        self.write(catalog, SOURCE.replace('Runtime::Context"]', 'Runtime::Context<br/>events()"]'))
        self.assertEqual(load_catalog(self.source), catalog)

    def test_template_arguments_survive_label_markup_normalization(self):
        catalog = fixture()
        method = {**operation(), "label": "notify(Events::EventChannel<Payload>& event)"}
        catalog["diagrams"][0]["operations"] = {"Runtime::Context": [method]}
        source = SOURCE.replace('Runtime::Context"]',
                                'Runtime::Context<br/>notify(<br/>Events::EventChannel&lt;Payload&gt;&amp; event)"]')
        self.write(catalog, source)
        self.assertEqual(load_catalog(self.source), catalog)
        self.write(catalog, source.replace("Payload", "OtherPayload"))
        with self.assertRaisesRegex(ValueError, "omits operation label"):
            load_catalog(self.source)

    def test_unknown_method_owner_is_rejected(self):
        catalog = fixture()
        catalog["diagrams"][0]["operations"] = {"Elsewhere": [operation()]}
        self.write(catalog)
        with self.assertRaisesRegex(ValueError, "Unknown operation owner"):
            load_catalog(self.source)

    def test_wrapped_signature_keeps_argument_and_result_types(self):
        catalog = fixture()
        method = {**operation(), "label": "events(): Events::EventBus&"}
        catalog["diagrams"][0]["operations"] = {"Runtime::Context": [method]}
        source = SOURCE.replace('Runtime::Context"]',
                                'Runtime::Context<br/>events():<br/>Events::EventBus&amp;"]')
        self.write(catalog, source)
        self.assertEqual(load_catalog(self.source), catalog)
        self.write(catalog, source.replace("Events::EventBus", "Events::EventChannel"))
        with self.assertRaisesRegex(ValueError, "omits operation label"):
            load_catalog(self.source)

    def test_local_namespace_preserves_nested_and_external_qualifiers(self):
        catalog = fixture()
        method = {**operation(), "label": "attach(Runtime::Context next, Input::Actions::Context actions): Runtime::Context"}
        diagram = catalog["diagrams"][0]
        diagram["namespace"] = "Runtime"
        diagram["operations"] = {"Runtime::Context": [method]}
        source = SOURCE.replace('Runtime::Context"]',
                                'Context<br/>attach(Context next, Input::Actions::Context actions): Context"]')
        self.write(catalog, source)
        self.assertEqual(load_catalog(self.source), catalog)
        self.write(catalog, source.replace("Input::Actions::Context", "Input::Context"))
        with self.assertRaisesRegex(ValueError, "omits operation label"):
            load_catalog(self.source)


class PublicationTests(unittest.TestCase):
    def test_local_namespace_keeps_full_inspector_and_api_identity(self):
        with tempfile.TemporaryDirectory(prefix="cheryl-diagram-namespace-") as directory:
            source = Path(directory) / "source"
            source.mkdir()
            catalog = fixture()
            method = {**operation(), "label": "context(): Runtime::Context&"}
            diagram = catalog["diagrams"][0]
            diagram["namespace"] = "Runtime"
            diagram["operations"] = {"Runtime::Context": [method]}
            (source / "catalog.json").write_text(json.dumps(catalog))
            (source / "runtime-application.mmd").write_text(SOURCE.replace(
                'Runtime::Context"]', 'Context<br/>context(): Context&amp;"]'))
            destination = Path(directory) / "html"
            objects = [("CE::Runtime::Context", "", "class", "api/context", "type", 1),
                       (method["name"], "", "function", "api/context", "method", 1)]
            write_catalog(source, destination, objects, lambda name: name + ".html", "https://example.com")
            output = json.loads((destination / "catalog.json").read_text())["diagrams"][0]
            self.assertEqual(output["namespace"], "Runtime")
            node = output["nodes"][0]
            self.assertEqual(node["label"], "Runtime::Context")
            self.assertEqual(node["api"], "api/context.html#type")
            self.assertEqual(node["operations"][0]["label"], method["label"])
            self.assertEqual(node["operations"][0]["api"], "api/context.html#method")

    def test_typed_free_operation_stays_specific_to_its_view(self):
        with tempfile.TemporaryDirectory(prefix="cheryl-diagram-operation-") as directory:
            source = Path(directory) / "source"
            source.mkdir()
            catalog = fixture()
            key = "Runtime::Context"
            catalog["symbols"][key]["kind"] = "operation"
            catalog["symbols"][key]["label"] = "make_context()"
            application = catalog["diagrams"][0]
            application["operation_labels"] = {key: "make_context(): Runtime::Context [unique]"}
            catalog["diagrams"].append({**fixture()["diagrams"][0], "id": "runtime-internal"})
            (source / "catalog.json").write_text(json.dumps(catalog))
            (source / "runtime-application.mmd").write_text(SOURCE.replace(
                "Runtime::Context", "make_context():<br/>Runtime::Context [unique]"))
            (source / "runtime-internal.mmd").write_text(SOURCE.replace("Runtime::Context", "make_context()"))
            destination = Path(directory) / "html"
            write_catalog(source, destination, [], lambda name: name + ".html", "https://example.com")
            output = json.loads((destination / "catalog.json").read_text())
            self.assertEqual(output["diagrams"][0]["nodes"][0]["label"],
                             "make_context(): Runtime::Context [unique]")
            self.assertEqual(output["diagrams"][1]["nodes"][0]["label"], "make_context()")
            self.assertEqual(catalog["symbols"][key]["label"], "make_context()")

    def test_frame_assets_change_their_browser_version(self):
        with tempfile.TemporaryDirectory(prefix="cheryl-diagram-assets-") as directory:
            root = Path(directory)
            (root / "frame.html").write_text('<link href="explorer.css"><script src="explorer.mjs"></script>')
            (root / "explorer.css").write_text('body { color: black; }')
            (root / "explorer.mjs").write_text('const version = 1;')
            (root / "shell.js").write_text('const layout = "shared";')
            before = frame_version(root)
            version_frame_assets(root)
            first = (root / "frame.html").read_text()
            self.assertIn('explorer.css?v=', first)
            self.assertIn('explorer.mjs?v=', first)
            (root / "explorer.css").write_text('body { color: blue; }')
            version_frame_assets(root)
            self.assertNotEqual((root / "frame.html").read_text(), first)
            self.assertNotEqual(frame_version(root), before)

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

    def test_methods_are_published_only_in_their_selected_view(self):
        with tempfile.TemporaryDirectory(prefix="cheryl-diagram-methods-") as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            catalog = fixture()
            callable_operation = {**operation(), "name": "CE::Runtime::Context::operator()", "label": "operator()()"}
            catalog["diagrams"][0]["operations"] = {"Runtime::Context": [operation(), callable_operation]}
            other = {**fixture()["diagrams"][0], "id": "runtime-internal"}
            catalog["diagrams"].append(other)
            (source / "catalog.json").write_text(json.dumps(catalog))
            (source / "runtime-application.mmd").write_text(
                SOURCE.replace('Runtime::Context"]', 'Runtime::Context<br/>events()<br/>operator()()"]'))
            (source / "runtime-internal.mmd").write_text(SOURCE)
            objects = [("CE::Runtime::Context::events", "", "function", "api/context", "events", 1),
                       ("CE::Runtime::Context::operator()", "", "function", "api/context", "invoke", 1)]
            destination = root / "html"
            write_catalog(source, destination, objects, lambda name: name + ".html", "https://example.com")
            result = json.loads((destination / "catalog.json").read_text())
            application, internal = result["diagrams"]
            method = application["nodes"][0]["operations"][0]
            self.assertEqual(method["api"], "api/context.html#events")
            self.assertEqual(method["brief"], operation()["brief"])
            self.assertEqual(application["nodes"][0]["operations"][1]["api"], "api/context.html#invoke")
            self.assertNotIn("operations", internal["nodes"][0])
            self.assertNotIn("operations", application)
            self.assertNotIn("operations", catalog["symbols"]["Runtime::Context"])


class AnchorTests(unittest.TestCase):
    def hook(self):
        extension = GENERATION / "extensions/engine_diagrams.py"
        tree = ast.parse(extension.read_text())
        function = next(item for item in tree.body if isinstance(item, ast.FunctionDef) and item.name == "register_targets")
        scope = {
            "EXPLORER": "engine-systems/diagram-explorer",
            "ExtensionError": ValueError,
            "nodes": SimpleNamespace(Element=object),
            "source_directory": lambda app: None,
            "load_catalog": lambda source: {"diagrams": [{"id": "runtime-application", "title": "Runtime"}]},
        }
        exec(compile(ast.Module(body=[function], type_ignores=[]), str(extension), "exec"), scope)
        return scope["register_targets"]

    def test_generated_ids_are_checked_myst_slugs(self):
        heading = (1, "diagram-explorer", "Diagram explorer")
        metadata = {"engine-systems/diagram-explorer": {"myst_slugs": {"diagram-explorer": heading}}}
        app = SimpleNamespace(env=SimpleNamespace(docname="engine-systems/diagram-explorer", metadata=metadata))
        doctree = SimpleNamespace(findall=lambda kind: [{"ids": ["runtime-application"]}])
        self.hook()(app, doctree)
        slugs = metadata[app.env.docname]["myst_slugs"]
        self.assertEqual(slugs["diagram-explorer"], heading)
        self.assertEqual(slugs["runtime-application"], (None, "runtime-application", "Runtime"))

    def test_missing_real_target_does_not_get_a_fake_slug(self):
        app = SimpleNamespace(env=SimpleNamespace(
            docname="engine-systems/diagram-explorer", metadata={"engine-systems/diagram-explorer": {}},
        ))
        doctree = SimpleNamespace(findall=lambda kind: [{"ids": ["other-target"]}])
        with self.assertRaisesRegex(ValueError, "target was not emitted"):
            self.hook()(app, doctree)


if __name__ == "__main__":
    unittest.main()
