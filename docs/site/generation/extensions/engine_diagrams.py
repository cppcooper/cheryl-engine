"""Share curated Mermaid sources between system guides and the diagram explorer."""

from __future__ import annotations

from pathlib import Path

from docutils import nodes
from sphinx.errors import ExtensionError
from sphinx.util.docutils import SphinxDirective
from sphinx.util.nodes import make_refnode
from sphinxcontrib.mermaid import Mermaid

from diagram_catalog import frame_version, load_catalog, version_frame_assets, write_catalog


EXPLORER = "engine-systems/diagram-explorer"


def source_directory(app) -> Path:
    return Path(app.srcdir) / "engine-systems/diagrams"


class EngineDiagram(Mermaid):
    required_arguments = 1
    optional_arguments = 0
    has_content = False

    def get_mm_code(self):
        self.state.document.settings.env.note_dependency(str(self.engine_source))
        return self.engine_source.read_text(encoding="utf-8")

    def run(self):
        env = self.state.document.settings.env
        source = source_directory(env.app)
        catalog = load_catalog(source)
        identity = self.arguments[0]
        diagram = next((item for item in catalog["diagrams"] if item["id"] == identity), None)
        if diagram is None:
            raise self.error(f"Unknown Engine diagram: {identity}")
        env.note_dependency(str(source / "catalog.json"))
        self.engine_source = source / (identity + ".mmd")
        container = nodes.container(classes=["engine-diagram"])
        container.extend(super().run())
        paragraph = nodes.paragraph(classes=["engine-diagram-caption"])
        if diagram.get("namespace"):
            paragraph += nodes.Text("Local names omit " + diagram["namespace"] + ":: where unambiguous. ")
        paragraph += make_refnode(
            env.app.builder, env.docname, EXPLORER, identity,
            nodes.inline(text="Explore this diagram and its other views →"),
        )
        container += paragraph
        return [container]


class EngineExplorer(SphinxDirective):
    has_content = False

    def run(self):
        source = source_directory(self.env.app)
        catalog = load_catalog(source)
        self.env.note_dependency(str(source / "catalog.json"))
        listing = nodes.container(classes=["engine-explorer-index"])
        for diagram in catalog["diagrams"]:
            self.env.note_dependency(str(source / (diagram["id"] + ".mmd")))
            target = nodes.target(ids=[diagram["id"]], names=[diagram["id"]])
            self.state.document.note_explicit_target(target)
            listing += target
            paragraph = nodes.paragraph()
            paragraph += nodes.strong(text=diagram["title"])
            paragraph += nodes.Text(" — " + diagram["question"])
            listing += paragraph
        return [listing]


def page_context(app, pagename, templatename, context, doctree):
    if pagename == EXPLORER:
        context["engine_diagram_version"] = frame_version(Path(app.confdir) / "static/engine-diagrams")
        return "diagram-explorer.html"


def register_targets(app, doctree) -> None:
    """Expose directive-generated anchors to MyST's checked document links."""
    if app.env.docname != EXPLORER:
        return
    catalog = load_catalog(source_directory(app))
    # Docutils can move a target's ID onto the following paragraph.
    identifiers = {identity for node in doctree.findall(nodes.Element) for identity in node.get("ids", [])}
    slugs = app.env.metadata[EXPLORER].setdefault("myst_slugs", {})
    for diagram in catalog["diagrams"]:
        identity = diagram["id"]
        if identity not in identifiers:
            raise ExtensionError(f"Engine diagram target was not emitted: {identity}")
        if identity in slugs:
            raise ExtensionError(f"Engine diagram target collides with a heading: {identity}")
        slugs[identity] = (None, identity, diagram["title"])


def publish_catalog(app, exception) -> None:
    if exception is not None or app.builder.format != "html":
        return
    write_catalog(
        source_directory(app), Path(app.outdir) / "_static/engine-diagrams",
        app.env.domains["cpp"].get_objects(), app.builder.get_target_uri,
        app.config.source_repository,
    )
    version_frame_assets(Path(app.outdir) / "_static/engine-diagrams")


def setup(app):
    app.add_directive("engine-diagram", EngineDiagram)
    app.add_directive("engine-explorer", EngineExplorer)
    app.add_css_file("engine-diagrams/shell.css")
    app.add_js_file("engine-diagrams/navigation.js", defer="defer")
    app.connect("doctree-read", register_targets, priority=600)
    app.connect("html-page-context", page_context)
    app.connect("build-finished", publish_catalog)
    return {"version": "1", "parallel_read_safe": True, "parallel_write_safe": True}
