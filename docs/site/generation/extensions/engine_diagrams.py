"""Share curated Mermaid sources between system guides and the diagram explorer."""

from __future__ import annotations

from pathlib import Path

from docutils import nodes
from sphinx.util.docutils import SphinxDirective
from sphinx.util.nodes import make_refnode
from sphinxcontrib.mermaid import Mermaid

from diagram_catalog import load_catalog, write_catalog


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
            listing += nodes.target(ids=[diagram["id"]])
            paragraph = nodes.paragraph()
            paragraph += nodes.strong(text=diagram["title"])
            paragraph += nodes.Text(" — " + diagram["question"])
            listing += paragraph
        return [listing]


def page_context(app, pagename, templatename, context, doctree):
    if pagename == EXPLORER:
        return "diagram-explorer.html"


def publish_catalog(app, exception) -> None:
    if exception is not None or app.builder.format != "html":
        return
    write_catalog(
        source_directory(app), Path(app.outdir) / "_static/engine-diagrams",
        app.env.domains["cpp"].get_objects(), app.builder.get_target_uri,
        app.config.source_repository,
    )


def setup(app):
    app.add_directive("engine-diagram", EngineDiagram)
    app.add_directive("engine-explorer", EngineExplorer)
    app.add_css_file("engine-diagrams/shell.css")
    app.add_js_file("engine-diagrams/navigation.js", defer="defer")
    app.connect("html-page-context", page_context)
    app.connect("build-finished", publish_catalog)
    return {"version": "1", "parallel_read_safe": True, "parallel_write_safe": True}
