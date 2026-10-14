"""Resolve template-dependent references through their Doxygen declaring owners."""

from __future__ import annotations

from collections import defaultdict
from pathlib import Path
import re
import xml.etree.ElementTree as ET

from docutils import nodes
from sphinx.util.nodes import make_refnode


def plain_name(name: str) -> str:
    result = []
    depth = 0
    for character in name:
        if character == "<":
            depth += 1
        elif character == ">" and depth:
            depth -= 1
        elif not depth:
            result.append(character)
    return "".join(result).strip().removeprefix("::")


class CppReferences:
    def __init__(self, directory: Path, repository):
        self.repository = repository
        self.classes = {}
        self.symbols = defaultdict(set)
        self.owners = {}
        self.sources = {}
        self.documents = {}
        self.hidden_sources = {}
        self.templates = set()
        index = ET.parse(directory / "index.xml")
        indexed = {compound.get("refid") for compound in index.findall("compound")}
        configuration = ET.parse(directory / "Doxyfile.xml")
        self.private_namespaces = {
            value.text[:-3]
            for value in configuration.findall("option[@id='EXCLUDE_SYMBOLS']/value")
            if value.text and value.text.endswith("::*")
        }
        for compound in index.findall("compound"):
            identifier = compound.get("refid")
            declaration = ET.parse(directory / (identifier + ".xml")).find("compounddef")
            if declaration is None:
                continue
            kind = compound.get("kind")
            name = declaration.findtext("compoundname", "") if kind != "file" else ""
            if name and declaration.findall("templateparamlist/param"):
                self.templates.add(plain_name(name))
            document = "api/generated/" + identifier
            if kind in {"class", "struct", "concept"}:
                self.symbols[plain_name(name)].add((document, identifier))
                self.classes[identifier] = declaration
                self.documents[document] = declaration
            self.owners[document] = plain_name(name)
            location = declaration.find("location")
            if location is not None:
                self.sources[document] = location
            for member in declaration.findall(".//memberdef"):
                if member.findall("templateparamlist/param"):
                    qualified = member.findtext("qualifiedname") or "::".join(part for part in (name, member.findtext("name")) if part)
                    self.templates.add(plain_name(qualified))
                member_kind = member.get("kind")
                if member_kind not in {"typedef", "variable", "enum", "define"}:
                    continue
                target = (document, member.get("id"))
                if kind in {"file", "namespace"}:
                    document_name = "api/generated/" + member_kind + "_" + member.get("id")
                    target = (document_name, member.get("id"))
                    self.owners[document_name] = plain_name(name)
                    if member.find("location") is not None:
                        self.sources[document_name] = member.find("location")
                    self.documents[document_name] = member
                if member.get("prot") != "private":
                    qualified = member.findtext("qualifiedname") or "::".join(part for part in (name, member.findtext("name")) if part)
                    self.symbols[plain_name(qualified)].add(target)
        for filename in directory.glob("*.xml"):
            if filename.name in {"index.xml", "Doxyfile.xml"}:
                continue
            declaration = ET.parse(filename).find("compounddef")
            if declaration is None or declaration.get("id") in indexed:
                continue
            if declaration.get("kind") not in {"class", "struct", "union"}:
                continue
            location = declaration.find("location")
            if location is not None:
                self.hidden_sources[declaration.get("id")] = location
                self.hidden_sources[plain_name(declaration.findtext("compoundname", ""))] = location

    def member(self, identifier, name, ancestors=()):
        if identifier in ancestors or identifier not in self.classes:
            return set()
        declaration = self.classes[identifier]
        own = [member for member in declaration.findall(".//memberdef") if member.findtext("name") == name]
        if own:
            return {(identifier, member.get("id")) for member in own if member.get("prot") != "private"}
        result = set()
        for base in declaration.findall("basecompoundref"):
            if base.get("prot") == "public":
                result.update(self.member(base.get("refid"), name, (*ancestors, identifier)))
        return result

    def lexical_owners(self, document):
        owner = self.owners.get(document, "")
        while owner:
            yield owner
            owner = owner.rsplit("::", 1)[0] if "::" in owner else ""
        yield ""

    def private_source(self, document, member=None):
        location = member.find("location") if member is not None else self.sources.get(document)
        return self.location_url(location)

    def location_url(self, location):
        if location is None:
            return None
        return self.repository.source_url(
            self.repository.root / location.get("file"),
            "L" + location.get("line") if location.get("line") else "",
        )

    def resolve(self, app, environment, node, content):
        document = node.get("refdoc", environment.docname)
        target = plain_name(node.get("reftarget", ""))
        if not target:
            return None
        declaration = self.documents.get(document)
        # A nested type's enclosing template parameters retain the parent's owner.
        for owner in self.lexical_owners(document):
            owners = self.symbols.get(owner, set())
            parameter_owners = set()
            for owner_document, identifier in owners:
                element = self.classes.get(identifier)
                if element is None:
                    continue
                for parameter in element.findall("templateparamlist/param"):
                    name = parameter.findtext("declname")
                    if not name:
                        names = re.findall(r"[A-Za-z_]\w*", "".join(parameter.find("type").itertext()))
                        name = names[-1] if names else ""
                    if target == name and owner_document in environment.found_docs:
                        parameter_owners.add((owner_document, identifier))
            if len(parameter_owners) == 1:
                owner_document, identifier = next(iter(parameter_owners))
                return make_refnode(app.builder, document, owner_document, identifier, content)
            if parameter_owners:
                return None
            qualified = "::".join(part for part in (owner, target) if part)
            if qualified in self.hidden_sources:
                url = self.location_url(self.hidden_sources[qualified])
                if url:
                    return nodes.reference("", "", content, refuri=url, internal=False)
            symbols = self.symbols.get(qualified, set())
            if len(symbols) == 1:
                destination, anchor = next(iter(symbols))
                if destination in environment.found_docs:
                    return make_refnode(app.builder, document, destination, anchor, content)
            # An intentionally hidden implementation namespace stays source-linked.
            if any(qualified == namespace or qualified.startswith(namespace + "::") for namespace in self.private_namespaces):
                url = self.private_source(document)
                if url:
                    return nodes.reference("", "", content, refuri=url, internal=False)
        # Resolve dependent members through actual typedef references, never by
        # pretending that a missing member is declared on the referring class.
        if declaration is not None and declaration.tag == "compounddef":
            first, _, remaining = target.partition("::")
            matches = [member for member in declaration.findall(".//memberdef") if member.get("kind") == "typedef" and member.findtext("name") == first]
            for alias in matches:
                if alias.get("prot") == "private":
                    url = self.private_source(document, alias)
                    if url:
                        return nodes.reference("", "", content, refuri=url, internal=False)
                if remaining:
                    owners = set()
                    for reference in alias.findall("type/ref"):
                        owners.update(self.member(reference.get("refid"), remaining))
                    if len(owners) == 1:
                        identifier, anchor = next(iter(owners))
                        destination = "api/generated/" + identifier
                        if destination in environment.found_docs:
                            return make_refnode(app.builder, document, destination, anchor, content)
        if "::" in target:
            owner_name, member_name = target.rsplit("::", 1)
            for prefix in self.lexical_owners(document):
                qualified = "::".join(part for part in (prefix, owner_name) if part)
                matches = self.symbols.get(qualified, set())
                owners = set()
                for _, identifier in matches:
                    owners.update(self.member(identifier, member_name))
                if len(owners) == 1:
                    identifier, anchor = next(iter(owners))
                    destination = "api/generated/" + identifier
                    if destination in environment.found_docs:
                        return make_refnode(app.builder, document, destination, anchor, content)
        return None
