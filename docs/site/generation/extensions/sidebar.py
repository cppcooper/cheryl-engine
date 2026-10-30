"""Reuse the expanded navigation tree while preserving each page's active path."""

from __future__ import annotations

from docutils import nodes
from sphinx.environment.adapters.toctree import TocTree, _toctree_copy


MISSING = object()


class DocumentLinks:
    """Keep document identities until the cached tree is rendered for a page."""

    def __init__(self, builder):
        self.builder = builder

    def __getattr__(self, name):
        return getattr(self.builder, name)

    def get_relative_uri(self, source, target):
        return target


class Sidebar:
    """Cache shared structure, retaining page selection before Sphinx prunes it."""

    def __init__(self, builder, includehidden, titles_only):
        self.builder = builder
        self.tree = TocTree(builder.env).get_toctree_for(
            "", DocumentLinks(builder), collapse=False, maxdepth=-1,
            includehidden=includehidden, titles_only=titles_only,
        )
        self.current_classes = {}
        self.current_flags = {}
        if self.tree is None:
            return
        for reference in self.tree.findall(nodes.reference):
            if not reference.get("internal"):
                continue
            anchor = reference.get("anchorname", "")
            document = reference["refuri"]
            if anchor:
                document = document.removesuffix(anchor)
            reference["refuri"] = document
            parent = reference
            while parent is not None:
                if isinstance(parent, nodes.Element):
                    self.current_flags.setdefault(document, {})[id(parent)] = parent
                    if not anchor:
                        self.current_classes.setdefault(document, {})[id(parent)] = parent
                parent = parent.parent

    def render(self, document, maxdepth):
        if self.tree is None:
            return ""
        changed = []
        for node in self.current_classes.get(document, {}).values():
            previous = node["classes"]
            changed.append((node, "classes", previous))
            node["classes"] = [*previous, "current"]
        for node in self.current_flags.get(document, {}).values():
            changed.append((node, "iscurrent", node.get("iscurrent", MISSING)))
            node["iscurrent"] = True
        try:
            # Use Sphinx's own pruning/copying to retain depth, tag and node rules.
            tree = _toctree_copy(self.tree, 1, maxdepth, False, self.builder.tags)
        finally:
            for node, name, previous in changed:
                if previous is MISSING:
                    del node[name]
                else:
                    node[name] = previous
        for reference in tree.findall(nodes.reference):
            if reference.get("internal"):
                reference["refuri"] = (
                    self.builder.get_relative_uri(document, reference["refuri"])
                    + reference.get("anchorname", "")
                )
        return self.builder.render_partial(tree)["fragment"]


def prepare(app, environment):
    # The cache belongs to this build, outside the pickled Sphinx environment.
    app._cheryl_sidebars = {}


def page_context(app, pagename, templatename, context, doctree):
    if app.builder.name != "html":
        return
    original = context["toctree"]

    def toctree(collapse=True, maxdepth=0, includehidden=False, titles_only=False, **kwargs):
        if collapse or kwargs or not isinstance(maxdepth, int) or maxdepth <= 0:
            return original(collapse=collapse, maxdepth=maxdepth, includehidden=includehidden,
                            titles_only=titles_only, **kwargs)
        key = (includehidden, titles_only)
        if key not in app._cheryl_sidebars:
            app._cheryl_sidebars[key] = Sidebar(app.builder, includehidden, titles_only)
        return app._cheryl_sidebars[key].render(pagename, maxdepth)

    context["toctree"] = toctree


def setup(app):
    app.connect("env-updated", prepare, priority=900)
    app.connect("html-page-context", page_context, priority=900)
    return {"version": "1", "parallel_read_safe": True, "parallel_write_safe": True}
