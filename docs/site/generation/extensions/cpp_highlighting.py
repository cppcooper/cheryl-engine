"""Enrich C++ code blocks with syntax roles and the existing public API inventory."""

from __future__ import annotations

from dataclasses import dataclass, field
from functools import lru_cache
import re

from pygments.lexers import CppLexer
from pygments.token import Name
from tree_sitter import Language, Parser
import tree_sitter_cpp


TYPE_KINDS = {"class", "union", "type", "concept", "enum"}
SCOPES = {
    "translation_unit", "namespace_definition", "class_specifier", "struct_specifier",
    "union_specifier", "enum_specifier", "function_definition", "lambda_expression", "compound_statement",
    "for_statement", "for_range_loop", "if_statement", "switch_statement",
    "while_statement", "catch_clause",
}
CLASS_SCOPES = {"class_specifier", "struct_specifier", "union_specifier"}
WRAPPERS = {
    "init_declarator", "pointer_declarator", "reference_declarator", "array_declarator",
    "parenthesized_declarator", "attributed_declarator", "variadic_declarator",
    "function_declarator",
}


def terminal(node):
    """Find a declarator's name without descending into arguments or initializers."""
    if node is None:
        return None
    if node.type in {"qualified_identifier", "template_function", "template_method", "template_type"}:
        return terminal(node.child_by_field_name("name"))
    if node.type in WRAPPERS:
        child = node.child_by_field_name("declarator")
        if child is None:
            child = next((child for child in node.named_children
                          if child.type in WRAPPERS or child.type in {
                              "identifier", "field_identifier", "qualified_identifier",
                              "structured_binding_declarator",
                          }), None)
        return terminal(child)
    if node.type == "destructor_name":
        return next((child for child in node.named_children if child.type == "identifier"), None)
    if node.type in {"identifier", "field_identifier", "type_identifier", "namespace_identifier"}:
        return node
    return None


def declared_names(node):
    if node is None:
        return []
    if node.type == "structured_binding_declarator":
        return [child for child in node.named_children if child.type == "identifier"]
    if node.type in WRAPPERS and node.type != "function_declarator":
        child = node.child_by_field_name("declarator")
        if child is None:
            child = next((child for child in node.named_children
                          if child.type in WRAPPERS or child.type in {
                              "identifier", "field_identifier", "structured_binding_declarator",
                          }), None)
        return declared_names(child)
    name = terminal(node)
    return [name] if name is not None else []


def is_function(node):
    while node is not None and node.type in WRAPPERS:
        if node.type == "function_declarator":
            declarator = node.child_by_field_name("declarator")
            return declarator is not None and declarator.type in {
                "identifier", "field_identifier", "qualified_identifier", "destructor_name",
            }
        node = node.child_by_field_name("declarator")
    return False


@dataclass
class Scope:
    kind: str
    parent: Scope | None = None
    prefix: str = ""
    bindings: dict = field(default_factory=dict)
    enums: dict = field(default_factory=dict)

    def resolve(self, name, position):
        scope = self
        while scope is not None:
            binding = scope.bindings.get(name)
            if binding is not None and binding[1] <= position:
                return binding[0]
            scope = scope.parent
        return None


class Roles:
    def __init__(self, source, symbols):
        self.source = source
        self.symbols = symbols
        self.annotations = {}

    def text(self, node):
        return self.source[node.start_byte:node.end_byte].decode("utf-8")

    def mark(self, node, token, priority=10):
        if node is not None and not node.is_missing:
            key = (node.start_byte, node.end_byte)
            if key not in self.annotations or self.annotations[key][0] < priority:
                self.annotations[key] = (priority, token)

    def bind(self, scope, node, token, *, position=None):
        self.mark(node, token, 50)
        scope.bindings[self.text(node)] = (token, node.start_byte if position is None else position)

    def qualified(self, node, scope):
        name = re.sub(r"\s+", "", self.text(node)).removeprefix("::")
        candidates = [name]
        current = scope
        while current is not None:
            if current.prefix:
                candidates.append(current.prefix + "::" + name)
            current = current.parent
        for key in candidates:
            current = scope
            while current is not None:
                if key in current.enums:
                    return current.enums[key]
                current = current.parent
            if key in self.symbols:
                return self.symbols[key]
        return None

    def visit(self, node, scope=None):
        if node.type in SCOPES:
            prefix = scope.prefix if scope is not None else ""
            name = node.child_by_field_name("name")
            if name is not None and scope is not None and node.type in CLASS_SCOPES | {"enum_specifier"}:
                self.bind(scope, name, Name.Class)
            if name is not None and node.type in CLASS_SCOPES | {"enum_specifier", "namespace_definition"}:
                prefix = "::".join(part for part in (prefix, self.text(name)) if part)
            scope = Scope(node.type, scope, prefix)
            if node.type in CLASS_SCOPES:
                # All fields are visible throughout a class, including earlier methods.
                body = node.child_by_field_name("body")
                for member in body.named_children if body is not None else ():
                    if member.type == "field_declaration":
                        for declarator in member.children_by_field_name("declarator"):
                            if not is_function(declarator):
                                for name in declared_names(declarator):
                                    self.bind(scope, name, Name.Variable.Instance, position=0)

        kind = node.type
        if kind in {"type_identifier", "namespace_identifier"}:
            self.mark(node, Name.Class if kind == "type_identifier" else Name.Namespace)
        elif kind == "field_identifier":
            self.mark(node, Name.Variable.Instance)
        elif kind == "identifier":
            token = scope.resolve(self.text(node), node.start_byte)
            if token is not None:
                self.mark(node, token)
        elif kind == "qualified_identifier":
            token = self.qualified(node, scope)
            if token is not None:
                self.mark(terminal(node), token, 45)
        elif kind == "function_declarator":
            declarator = node.child_by_field_name("declarator")
            if is_function(node):
                self.mark(terminal(declarator), Name.Function, 50)
            # Prototype parameters must not leak into the enclosing snippet's scope.
            ancestor = node.parent
            while ancestor is not None and ancestor.type in WRAPPERS:
                ancestor = ancestor.parent
            if ancestor is None or ancestor.type not in {"function_definition", "lambda_expression"}:
                scope = Scope(kind, scope, scope.prefix)
        elif kind == "call_expression":
            function = node.child_by_field_name("function")
            if function is not None:
                if function.type == "field_expression":
                    function = function.child_by_field_name("field")
                name = terminal(function)
                known = scope.resolve(self.text(name), name.start_byte) if name is not None else None
                self.mark(name, Name.Class if known == Name.Class else Name.Function.Call, 40)
        elif kind in {"parameter_declaration", "optional_parameter_declaration"}:
            for name in declared_names(node.child_by_field_name("declarator")):
                self.bind(scope, name, Name.Variable.Parameter)
        elif kind in {"declaration", "field_declaration", "for_range_loop"}:
            token = Name.Variable.Instance if kind == "field_declaration" else Name.Variable
            for declarator in node.children_by_field_name("declarator"):
                for name in declared_names(declarator):
                    self.bind(scope, name, Name.Function if is_function(declarator) else token)
        elif kind == "lambda_capture_initializer":
            for name in declared_names(node.child_by_field_name("left")):
                self.bind(scope, name, Name.Variable)
        elif kind == "enum_specifier":
            name = node.child_by_field_name("name")
            body = node.child_by_field_name("body")
            scoped = any(child.type in {"class", "struct"} for child in node.children)
            for value in body.named_children if body is not None else ():
                enumerator = value.child_by_field_name("name")
                if enumerator is not None:
                    self.bind(scope, enumerator, Name.Constant)
                    if not scoped and scope.parent is not None:
                        self.bind(scope.parent, enumerator, Name.Constant)
                    if name is not None:
                        qualified = "::".join(part for part in (
                            scope.prefix, self.text(enumerator)
                        ) if part)
                        owner = scope.parent
                        while owner.parent is not None and owner.kind in CLASS_SCOPES | {"namespace_definition"}:
                            owner = owner.parent
                        owner.enums[qualified] = Name.Constant

        for child in node.named_children:
            self.visit(child, scope)


@lru_cache(maxsize=1)
def language():
    return Language(tree_sitter_cpp.language())


class DocumentationCppLexer(CppLexer):
    """Preserve Pygments' lexical tokens and add roles established by C++ syntax."""

    public_symbols = {}

    def get_tokens_unprocessed(self, text, stack=("root",)):
        source = text.encode("utf-8")
        roles = Roles(source, self.public_symbols)
        roles.visit(Parser(language()).parse(source).root_node)
        # Tree-sitter uses UTF-8 byte offsets; Pygments uses Python character offsets.
        boundaries = sorted({position for pair in roles.annotations for position in pair})
        offsets = {}
        previous = characters = 0
        for position in boundaries:
            characters += len(source[previous:position].decode("utf-8"))
            offsets[position] = characters
            previous = position
        spans = [(offsets[start], offsets[end], token)
                 for (start, end), (_, token) in sorted(roles.annotations.items())]
        cursor = 0
        for start, token, value in super().get_tokens_unprocessed(text, stack):
            end = start + len(value)
            if token not in Name:
                yield start, token, value
                continue
            # Pygments combines qualified function names into one token.
            base = Name if "::" in value else token
            position = start
            while cursor < len(spans) and spans[cursor][1] <= start:
                cursor += 1
            index = cursor
            while index < len(spans) and spans[index][0] < end:
                left, right, replacement = spans[index]
                if position < left:
                    yield position, base, text[position:left]
                stop = min(end, right)
                yield max(position, left), replacement, text[max(position, left):stop]
                position = stop
                if right > end:
                    break
                index += 1
            if position < end:
                yield position, base, text[position:end]


def prepare(app, environment):
    # inline_api already builds this inventory from Sphinx's generated declarations.
    symbols = {}
    classes = {name for name, targets in app._cheryl_inline_api.exact.items()
               if {target.kind for target in targets} <= {"class", "union"}}
    for name, targets in app._cheryl_inline_api.exact.items():
        kinds = {target.kind for target in targets}
        if len(kinds) == 1:
            kind = next(iter(kinds))
            if kind in TYPE_KINDS:
                symbols[name] = Name.Class
            elif kind == "enumerator":
                symbols[name] = Name.Constant
            elif kind == "member" and name.rpartition("::")[0] in classes:
                symbols[name] = Name.Variable.Instance
    DocumentationCppLexer.public_symbols = symbols


def setup(app):
    app.setup_extension("inline_api")
    for name in ("cpp", "c++"):
        app.add_lexer(name, DocumentationCppLexer)
    app.connect("env-updated", prepare, priority=600)
    return {"version": "1", "parallel_read_safe": True, "parallel_write_safe": True}
