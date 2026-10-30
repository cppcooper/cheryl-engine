"""C++ highlighting regressions using the installed documentation parser packages."""

from pathlib import Path
import sys
import types
import unittest
from unittest.mock import Mock, patch

from pygments import highlight
from pygments.formatters import HtmlFormatter
from pygments.lexers import CppLexer
from pygments.token import Comment, Keyword, Name, String


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "extensions"))
import cpp_highlighting as highlighting


EXAMPLE = """void SpaceInvaders::prepare_render_frame(CE::Rendering::FrameWriter& frame) const {
    auto pass = frame.begin_pass(camera.projection_matrix(), camera.view_matrix());
    const CE::Rendering::Submission::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
"""


class HighlightingTests(unittest.TestCase):
    def setUp(self):
        self.enterContext(patch.object(highlighting.DocumentationCppLexer, "public_symbols", {}))
        self.lexer = highlighting.DocumentationCppLexer()

    def tokens(self, source):
        tokens = list(self.lexer.get_tokens_unprocessed(source))
        self.assertEqual("".join(value for _, _, value in tokens), source)
        for start, _, value in tokens:
            self.assertEqual(source[start:start + len(value)], value)
        return tokens

    def roles(self, source, word):
        return [token for _, token, value in self.tokens(source) if value == word]

    def test_render_example(self):
        expected = {
            "SpaceInvaders": Name.Namespace,
            "prepare_render_frame": Name.Function,
            "FrameWriter": Name.Class,
            "Context2D": Name.Class,
            "frame": Name.Variable.Parameter,
            "pass": Name.Variable,
            "context": Name.Variable,
            "begin_pass": Name.Function.Call,
            "projection_matrix": Name.Function.Call,
            "camera": Name,
            "const": Keyword,
        }
        for word, token in expected.items():
            with self.subTest(word=word):
                roles = self.roles(EXAMPLE, word)
                self.assertTrue(roles)
                self.assertEqual(set(roles), {token})
        html = highlight(EXAMPLE, self.lexer, HtmlFormatter())
        self.assertRegex(html, r'class="[^"]*\bnf-Call\b[^"]*">begin_pass</span>')
        self.assertRegex(html, r'class="[^"]*\bnv-Parameter\b[^"]*">frame</span>')
        self.assertIn('class="nf">prepare_render_frame</span>', html)

    def test_shadowed_parameter(self):
        source = "void draw(int value) { use(value); { int value = 2; use(value); } use(value); }"
        self.assertEqual(self.roles(source, "value"), [
            Name.Variable.Parameter, Name.Variable.Parameter,
            Name.Variable, Name.Variable, Name.Variable.Parameter,
        ])

    def test_prototype_scope(self):
        source = "void draw(int value); use(value); void other(int value) { use(value); }"
        self.assertEqual(self.roles(source, "value"), [
            Name.Variable.Parameter, Name, Name.Variable.Parameter, Name.Variable.Parameter,
        ])

    def test_member_call_and_local(self):
        source = "auto begin_pass = 1; use(begin_pass); frame.begin_pass();"
        self.assertEqual(self.roles(source, "begin_pass"), [
            Name.Variable, Name.Variable, Name.Function.Call,
        ])

    def test_class_fields(self):
        source = "class Game { void draw(int camera) { use(camera); use(this->camera); } Camera camera; };"
        self.assertEqual(self.roles(source, "camera"), [
            Name.Variable.Parameter, Name.Variable.Parameter,
            Name.Variable.Instance, Name.Variable.Instance,
        ])
        source = "class Game { void draw() { use(camera); } Camera camera; };"
        self.assertEqual(self.roles(source, "camera"), [Name.Variable.Instance] * 2)

    def test_local_constructor(self):
        source = "class Game {}; auto game = Game();"
        self.assertEqual(self.roles(source, "Game"), [Name.Class, Name.Class])

    def test_scoped_enum(self):
        source = "enum class Mode { active, next = active }; auto mode = Mode::active; use(active);"
        self.assertEqual(self.roles(source, "active"), [
            Name.Constant, Name.Constant, Name.Constant, Name,
        ])

    def test_unscoped_enum(self):
        source = "namespace CE { enum Mode { active }; auto mode = active; auto other = Mode::active; }"
        self.assertEqual(self.roles(source, "active"), [Name.Constant] * 3)

    def test_enum_visibility(self):
        source = "void one() { enum class Mode { active }; use(Mode::active); } void two() { use(Mode::active); }"
        self.assertEqual(self.roles(source, "active"), [Name.Constant, Name.Constant, Name])
        source = "namespace CE { enum class Mode { active }; } namespace CE { auto mode = Mode::active; }"
        self.assertEqual(self.roles(source, "active"), [Name.Constant, Name.Constant])

    def test_generated_symbols(self):
        target = lambda kind: types.SimpleNamespace(kind=kind)
        app = types.SimpleNamespace(_cheryl_inline_api=types.SimpleNamespace(exact={
            "CE::Mode::active": [target("enumerator")],
            "CE::Frame": [target("class")],
            "CE::Frame::time": [target("member")],
            "CE::global": [target("member")],
            "CE::unknown": [target("class"), target("enumerator")],
        }))
        highlighting.prepare(app, None)
        source = "auto mode = CE::Mode::active; auto frame = CE::Frame(); use(active);"
        self.assertEqual(self.roles(source, "active"), [Name.Constant, Name])
        self.assertEqual(self.roles(source, "Frame"), [Name.Class])
        self.assertEqual(self.roles("auto time = CE::Frame::time;", "time"), [
            Name.Variable, Name.Variable.Instance,
        ])
        self.assertEqual(self.roles("use(CE::global);", "global"), [Name])
        self.assertNotIn("CE::unknown", self.lexer.public_symbols)
        source = "namespace CE { auto mode = Mode::active; }"
        self.assertEqual(self.roles(source, "active"), [Name.Constant])
        app._cheryl_inline_api.exact = {}
        highlighting.prepare(app, None)
        self.assertEqual(self.roles(source, "active"), [Name])

    def test_range_scope(self):
        source = "for (auto [key, value] : entries) { use(key, value); } use(key);"
        self.assertEqual(self.roles(source, "key"), [Name.Variable, Name.Variable, Name])
        self.assertEqual(self.roles(source, "value"), [Name.Variable, Name.Variable])

    def test_lambda_scope(self):
        source = "auto draw = [count = 2](int value) { use(count, value); }; use(count, value);"
        self.assertEqual(self.roles(source, "count"), [Name.Variable, Name.Variable, Name])
        self.assertEqual(self.roles(source, "value"), [
            Name.Variable.Parameter, Name.Variable.Parameter, Name,
        ])

    def test_function_pointer(self):
        source = "void (*callback)(int argument); callback(2); int *factory();"
        self.assertEqual(self.roles(source, "callback"), [Name.Variable, Name.Function.Call])
        self.assertEqual(self.roles(source, "factory"), [Name.Function])

    def test_unicode_and_literals(self):
        source = '// caméra → draw()\nconst char* text = R"(camera.draw())";\nauto pass = draw("é");'
        original = [(token, value) for _, token, value in CppLexer().get_tokens_unprocessed(source)
                    if token in Comment or token in String]
        enriched = [(token, value) for _, token, value in self.tokens(source)
                    if token in Comment or token in String]
        self.assertEqual(enriched, original)
        self.assertEqual(self.roles(source, "pass"), [Name.Variable])

    def test_partial_code(self):
        for source in ("", "frame.begin_pass(", "auto pass = frame.begin_pass();", EXAMPLE, "...\n}"):
            with self.subTest(source=source):
                self.tokens(source)

    def test_registration(self):
        app = Mock()
        metadata = highlighting.setup(app)
        app.setup_extension.assert_called_once_with("inline_api")
        self.assertEqual([call.args[0] for call in app.add_lexer.call_args_list], ["cpp", "c++"])
        app.connect.assert_called_once_with("env-updated", highlighting.prepare, priority=600)
        self.assertTrue(metadata["parallel_write_safe"])


if __name__ == "__main__":
    unittest.main()
