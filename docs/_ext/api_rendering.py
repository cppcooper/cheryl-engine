"""Preserve declaration ownership and semantic metadata in the C++ reference."""

from __future__ import annotations

from pathlib import Path
import xml.etree.ElementTree as ElementTree

from breathe.directives.class_like import DoxygenClassDirective, DoxygenStructDirective
from breathe.directives.item import DoxygenDefineDirective, DoxygenEnumDirective, DoxygenTypedefDirective, DoxygenVariableDirective
from breathe.renderer.filter import AttributeAccessor, FilterFactory, Node
from docutils import nodes
from exhale.graph import ExhaleRoot
from sphinx import addnodes


def namespace_identity(name: str) -> str:
    # Hex preserves C++ case distinctions on case-insensitive filesystems/labels.
    return "namespace_" + name.encode("utf-8").hex()


class DeclaringOwnerFilter(FilterFactory):
    def create_innerclass_filter(self, options, outerclass=""):
        node = Node()
        # Exhale creates an authoritative page and relationship link for each
        # nested type. Rendering it again in its outer class duplicates symbols.
        return ~((node.node_type == "ref") & (node.node_name == "innerclass"))


class OwnedClassDirective(DoxygenClassDirective):
    @property
    def filter_factory(self):
        return DeclaringOwnerFilter(self.env.app)


class OwnedStructDirective(DoxygenStructDirective):
    @property
    def filter_factory(self):
        return DeclaringOwnerFilter(self.env.app)


class ExactMember:
    def create_finder_filter(self, namespace, name):
        identifier = self.env.app._cheryl_member_documents.get(self.env.docname)
        if identifier:
            node = Node()
            return (node.node_type == "member") & (AttributeAccessor(node, "refid") == identifier)
        return super().create_finder_filter(namespace, name)


class ExactTypedefDirective(ExactMember, DoxygenTypedefDirective):
    pass


class ExactVariableDirective(ExactMember, DoxygenVariableDirective):
    pass


class ExactEnumDirective(ExactMember, DoxygenEnumDirective):
    pass


class ExactDefineDirective(ExactMember, DoxygenDefineDirective):
    pass


def install_namespace_identity() -> None:
    original = ExhaleRoot.initializeNodeFilenameAndLink
    if getattr(original, "_cheryl_namespace_identity", False):
        return

    def identify(root, node):
        original(root, node)
        if node.kind == "namespace":
            node.link_name = namespace_identity(node.name)
            node.file_name = str(Path(root.root_directory) / (node.link_name + ".rst"))

    identify._cheryl_namespace_identity = True
    ExhaleRoot.initializeNodeFilenameAndLink = identify


def collect_metadata(app) -> None:
    directory = Path(app.config.breathe_projects[app.config.breathe_default_project])
    index = ElementTree.parse(directory / "index.xml")
    metadata = {}
    documents = {}
    concepts = []
    for compound in index.findall("compound"):
        declaration = ElementTree.parse(directory / (compound.get("refid") + ".xml")).find("compounddef")
        if declaration is None:
            continue
        if compound.get("kind") == "concept":
            name = declaration.findtext("compoundname")
            docname = compound.get("refid")
            page = Path(app.srcdir) / "api/generated" / (docname + ".rst")
            title = "Concept " + name
            page.write_text(
                title + "\n" + "=" * len(title) + "\n\n.. doxygenconcept:: " + name + "\n   :project: Cheryl\n",
                encoding="utf-8",
            )
            concepts.append(docname)
        for element in [declaration, *declaration.findall(".//memberdef")]:
            if element.tag == "memberdef" and declaration.get("kind") in {"file", "namespace"}:
                documents["api/generated/" + element.get("kind") + "_" + element.get("id")] = element.get("id")
            notes = []
            if element.get("kind") == "concept":
                def source_text(part):
                    if part.tag == "sp":
                        return " "
                    return (part.text or "") + "".join(
                        source_text(child) + (child.tail or "") for child in part
                    )

                lines = element.findall("conceptparts/codepart/programlisting/codeline")
                if lines:
                    notes.append(("Definition", "\n".join(source_text(line) for line in lines)))
            if element.get("final") == "yes":
                notes.append(("Declaration", "final"))
            if element.get("nodiscard") == "yes":
                notes.append(("Declaration", "[[nodiscard]]"))
            clause = element.find("requiresclause")
            if clause is not None:
                notes.append(("Requires", " ".join("".join(clause.itertext()).split())))
            if notes:
                metadata[element.get("id")] = notes
    app._cheryl_declaration_metadata = metadata
    app._cheryl_member_documents = documents
    if concepts:
        root = Path(app.srcdir) / "api/generated/library_root.rst"
        with root.open("a", encoding="utf-8") as output:
            output.write("\nConcepts\n--------\n\n.. toctree::\n   :maxdepth: 1\n\n")
            output.writelines("   " + docname + "\n" for docname in concepts)


def declaration_notes(app, doctree) -> None:
    for declaration in doctree.findall(addnodes.desc):
        signatures = [child for child in declaration.children if isinstance(child, addnodes.desc_signature)]
        content = next((child for child in declaration.children if isinstance(child, addnodes.desc_content)), None)
        if content is None:
            continue
        notes = []
        for signature in signatures:
            for element in signature.findall(nodes.Element):
                for identifier in element.get("ids", []):
                    for note in app._cheryl_declaration_metadata.get(identifier, []):
                        if note not in notes:
                            notes.append(note)
        for label, value in reversed(notes):
            if label == "Definition":
                block = nodes.literal_block(value, value)
                block["language"] = "cpp"
                content.insert(0, block)
            else:
                paragraph = nodes.paragraph("", "", nodes.strong("", label + ": "), nodes.literal("", value))
                content.insert(0, paragraph)


def setup(app) -> dict[str, object]:
    install_namespace_identity()
    app.add_directive("doxygenclass", OwnedClassDirective, override=True)
    app.add_directive("doxygenstruct", OwnedStructDirective, override=True)
    app.add_directive("doxygentypedef", ExactTypedefDirective, override=True)
    app.add_directive("doxygenvariable", ExactVariableDirective, override=True)
    app.add_directive("doxygenenum", ExactEnumDirective, override=True)
    app.add_directive("doxygendefine", ExactDefineDirective, override=True)
    app.connect("builder-inited", collect_metadata)
    app.connect("doctree-read", declaration_notes)
    return {"version": "1", "parallel_read_safe": True, "parallel_write_safe": True}
