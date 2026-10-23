#include <cheryl/core/game-framework/dialogue.h>

#include "../src/definition-codec.h"
#include "support.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <limits>
#include <memory>
#include <utility>

namespace {
    namespace G = GateConsole;
    using Json = nlohmann::json;
    std::string fixture(std::string_view name = "gate") {
        return GateTesting::contents(std::string(DIALOGUE_CONSOLE_FIXTURES) + "/" + std::string(name) + ".dialogue.json");
    }
    using Cases = std::vector<std::pair<std::string, std::string>>;
    Json terminal_document(std::size_t count = 1) {
        Json document{{"definition_schema", 1}, {"id", "limits"}, {"revision", 1}, {"entry", "n0"}, {"nodes", Json::array()}};
        for (std::size_t index = 0; index < count; ++index)
            document["nodes"].push_back(
                {{"id", "n" + std::to_string(index)}, {"text", ""}, {"kind", "terminal"}, {"choices", Json::array()}}
            );
        return document;
    }
    Json choice_row(std::string id, std::string target = "n0") {
        return {{"id", std::move(id)}, {"text", ""}, {"target", std::move(target)}};
    }
    std::string duplicate(const Json& document, const Json& object, std::string key, const Json& value) {
        auto bytes = document.dump();
        const auto child = object.dump();
        bytes.insert(bytes.find(child) + 1, std::move(key) + ":" + value.dump() + ",");
        return bytes;
    }
    void rejected(const Cases& cases) {
        GateTesting::Directory directory;
        unsigned index = 0;
        for (const auto& [name, bytes] : cases) {
            SCOPED_TRACE(name);
            const auto path = directory.file(std::to_string(++index) + ".dialogue.json");
            GateTesting::write_fixture(path, bytes);
            const auto loaded = G::load_definition(path);
            ASSERT_FALSE(loaded);
            const auto& error = loaded.error();
            EXPECT_EQ(error.origin, path);
            EXPECT_TRUE(error.field.starts_with("$"));
            EXPECT_FALSE(error.message.empty());
            const auto formatted = G::format_definition_error(error);
            EXPECT_NE(formatted.find(path + ": " + error.field), std::string::npos);
            EXPECT_EQ(GateTesting::contents(path), bytes);
            // No sidecars, fallback files or partial usable output on rejection.
            EXPECT_EQ(
                std::distance(std::filesystem::directory_iterator(directory.file(".")), std::filesystem::directory_iterator()), index
            );
        }
    }
}

TEST(dialogue_console_definition, consumer) {
    const auto path = std::string(DIALOGUE_CONSOLE_FIXTURES) + "/gate.dialogue.json";
    auto loaded = G::load_definition(path);
    ASSERT_TRUE(loaded);
    std::size_t effects = 0;
    G::Encounter encounter(std::move(*loaded), &effects);
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[0].selection));
    const auto paired = encounter.capture().value();
    ASSERT_TRUE(G::restore_bytes(encounter, G::encode(paired)));
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[0].selection));
    EXPECT_EQ(encounter.view()->text, "You enter");
    EXPECT_EQ(effects, 1);
}

TEST(dialogue_console_definition, routes) {
    const auto bytes = fixture();
    for (const bool introduced : {false, true}) {
        std::size_t effects = 0;
        G::Encounter encounter(G::compile_definition(bytes, "gate-fixture").value(), &effects);
        const auto approach = encounter.view().value();
        EXPECT_EQ(approach.text, "A guard waits.");
        ASSERT_EQ(approach.choices.size(), 2);
        EXPECT_EQ(approach.choices[0].id.value, "introduce");
        EXPECT_EQ(approach.choices[0].text, "Introduce yourself");
        EXPECT_EQ(approach.choices[1].id.value, "silent");
        ASSERT_TRUE(encounter.choose(approach.choices[introduced ? 0 : 1].selection));
        const auto gate = encounter.view().value();
        ASSERT_EQ(gate.choices.size(), 2);
        EXPECT_EQ(gate.choices[0].text, "Ask to enter");
        EXPECT_EQ(gate.choices[1].text, "Leave");
        EXPECT_EQ(gate.choices[0].available, introduced);
        if (!introduced)
            EXPECT_EQ(encounter.choose(gate.choices[0].selection).error(), G::D::SessionError::Unavailable);
        ASSERT_TRUE(encounter.choose(gate.choices[introduced ? 0 : 1].selection));
        EXPECT_EQ(encounter.view()->text, introduced ? "You enter" : "You depart");
        EXPECT_EQ(effects, introduced ? 1 : 0);
    }
}

TEST(dialogue_console_definition, second_definition) {
    std::size_t effects = 0;
    G::Encounter encounter(G::compile_definition(fixture("crossroads"), "crossroads").value(), &effects);
    const auto initial = encounter.capture().value();
    EXPECT_EQ(initial.dialogue.definition.value, "crossroads");
    EXPECT_EQ(initial.dialogue.definition_revision, 7);
    EXPECT_EQ(initial.dialogue.node.value, "arrival");
    const auto offered = encounter.view().value();
    EXPECT_EQ(offered.text, "Sunset at the crossroads.");
    ASSERT_EQ(offered.choices.size(), 1);
    EXPECT_EQ(offered.choices[0].text, "Rest here");
    ASSERT_TRUE(encounter.choose(offered.choices[0].selection));
    EXPECT_EQ(encounter.view()->text, "You make camp.");
    const auto terminal = encounter.capture().value();
    ASSERT_TRUE(G::restore_bytes(encounter, G::encode(initial)));
    EXPECT_EQ(encounter.capture().value(), initial);
    ASSERT_TRUE(G::restore_bytes(encounter, G::encode(terminal)));
    EXPECT_EQ(encounter.capture().value(), terminal);
    EXPECT_EQ(effects, 0);
}

TEST(dialogue_console_definition, ownership) {
    G::D::DefinitionHandle direct;
    std::weak_ptr<const G::D::Definition> weak;
    std::unique_ptr<G::Encounter> encounter;
    {
        auto bytes = fixture();
        auto draft = G::decode_definition(bytes, "owned-input").value();
        direct = G::D::compile(draft, G::operation_keys()).value();
        auto compiled = G::compile_definition(bytes, "owned-input").value();
        weak = compiled;
        encounter = std::make_unique<G::Encounter>(compiled);
        compiled.reset();
        bytes.assign(bytes.size(), '?');
        draft.id.value = "mutated";
        draft.nodes[0].text = "mutated";
        draft.nodes[0].choices.clear();
        draft.nodes.clear();
    }
    ASSERT_FALSE(weak.expired());
    EXPECT_EQ(direct->entry().text, "A guard waits.");
    ASSERT_NE(direct->find_choice({"approach"}, {"introduce"}), nullptr);
    EXPECT_EQ(direct->find_choice({"approach"}, {"introduce"})->text, "Introduce yourself");
    const auto retained = encounter->view().value();
    EXPECT_EQ(retained.text, "A guard waits.");
    ASSERT_TRUE(encounter->choose(retained.choices[0].selection));
    encounter.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(retained.choices[1].text, "Stay silent");
}

TEST(dialogue_console_definition, semantic_errors) {
    const auto source = Json::parse(fixture());
    std::vector<Json> documents;
    for (const auto& field : {"id", "entry"}) {
        auto copy = source;
        copy[field] = "";
        documents.push_back(copy);
    }
    auto copy = source;
    copy["entry"] = "absent";
    documents.push_back(copy);
    copy = source;
    copy["nodes"][0]["id"] = "";
    documents.push_back(copy);
    copy = source;
    copy["nodes"][1]["id"] = copy["nodes"][0]["id"];
    documents.push_back(copy);
    copy = source;
    copy["nodes"][0]["choices"][1]["id"] = "introduce";
    documents.push_back(copy);
    for (const auto& field : {"id", "target", "condition", "effect"}) {
        copy = source;
        copy["nodes"][0]["choices"][0][field] = "";
        documents.push_back(copy);
    }
    for (const auto& [field, value] :
         {std::pair{"target", "absent"}, {"condition", "introduce"}, {"effect", "introduced"}, {"effect", "grant_admin"}}) {
        copy = source;
        copy["nodes"][0]["choices"][0][field] = value;
        documents.push_back(copy);
    }
    copy = source;
    copy["nodes"][0]["choices"] = Json::array();
    documents.push_back(copy);
    copy = source;
    copy["nodes"][3]["choices"] = source["nodes"][0]["choices"];
    documents.push_back(copy); // Invalid unreachable node is compiled eagerly too.
    Cases cases;
    for (std::size_t index = 0; index < documents.size(); ++index) {
        const auto bytes = documents[index].dump();
        const auto draft = G::decode_definition(bytes, "semantic");
        ASSERT_TRUE(draft);
        const auto expected = G::D::compile(*draft, G::operation_keys());
        ASSERT_FALSE(expected);
        const auto loaded = G::compile_definition(bytes, "semantic");
        ASSERT_FALSE(loaded);
        EXPECT_EQ(loaded.error().diagnostics, expected.error());
        for (const auto& diagnostic : expected.error())
            EXPECT_NE(G::format_definition_error(loaded.error()).find(diagnostic.field), std::string::npos);
        cases.emplace_back("semantic " + std::to_string(index), bytes);
    }
    rejected(cases);
}

TEST(dialogue_console_definition, owned_diagnostics) {
    G::DefinitionError error;
    std::vector<G::D::Diagnostic> expected;
    {
        auto document = Json::parse(fixture());
        document["entry"] = "bad-entry";
        document["nodes"][0]["choices"][0]["target"] = "bad-target";
        document["nodes"][0]["choices"][0]["effect"] = "bad-effect";
        document["nodes"][3]["choices"] = document["nodes"][0]["choices"];
        auto bytes = document.dump();
        auto origin = std::string("owned-origin.dialogue.json");
        const auto draft = G::decode_definition(bytes, origin).value();
        expected = G::D::compile(draft, G::operation_keys()).error();
        auto compiled = G::compile_definition(bytes, origin);
        ASSERT_FALSE(compiled);
        error = std::move(compiled.error());
        bytes.clear();
        origin.assign("destroyed-origin");
    }
    EXPECT_EQ(error.origin, "owned-origin.dialogue.json");
    EXPECT_EQ(error.diagnostics, expected);
    ASSERT_GT(error.diagnostics.size(), 3);
    const auto message = G::format_definition_error(error);
    EXPECT_NE(message.find("$.entry: missing reference"), std::string::npos);
    EXPECT_NE(message.find("$.nodes[0].choices[0].target"), std::string::npos);
    EXPECT_NE(message.find("$.nodes[3].choices: invalid node shape"), std::string::npos);
    EXPECT_NE(message.find("value='bad-effect'"), std::string::npos);
    EXPECT_NE(message.find("[gate-encounter@1]"), std::string::npos);
    auto bytes = fixture();
    bytes.insert(bytes.find("\"text\""), "\"extra\":1,");
    auto invalid = G::decode_definition(bytes, "structural-origin");
    ASSERT_FALSE(invalid);
    auto structural = std::move(invalid.error());
    bytes.clear();
    EXPECT_EQ(structural.node, 0);
    EXPECT_FALSE(structural.choice);
    EXPECT_EQ(structural.field, "$.nodes[0].extra");
    EXPECT_NE(G::format_definition_error(structural).find("structural-origin"), std::string::npos);
}

TEST(dialogue_console_definition, duplicates) {
    const auto source = Json::parse(fixture());
    Cases cases;
    for (const auto& [key, value] : source.items())
        cases.emplace_back("root " + key, duplicate(source, source, Json(key).dump(), value));
    for (const auto& [key, value] : source["nodes"][0].items())
        cases.emplace_back("node " + key, duplicate(source, source["nodes"][0], Json(key).dump(), value));
    for (const auto* row : {&source["nodes"][0]["choices"][0], &source["nodes"][1]["choices"][0]})
        for (const auto& [key, value] : row->items())
            cases.emplace_back("choice " + key, duplicate(source, *row, Json(key).dump(), value));
    cases.emplace_back("escaped root", duplicate(source, source, "\"\\u006eodes\"", source["nodes"]));
    cases.emplace_back("escaped node", duplicate(source, source["nodes"][0], "\"\\u0069d\"", "approach"));
    cases.emplace_back("escaped choice", duplicate(source, source["nodes"][0]["choices"][0], "\"e\\u0066fect\"", "introduce"));
    rejected(cases);
}

TEST(dialogue_console_definition, unknown_missing) {
    const auto source = Json::parse(fixture());
    Cases cases;
    for (const auto& [key, value] : source.items()) {
        auto copy = source;
        copy.erase(key);
        cases.emplace_back("missing root " + key, copy.dump());
    }
    for (const auto& [key, value] : source["nodes"][0].items()) {
        auto copy = source;
        copy["nodes"][0].erase(key);
        cases.emplace_back("missing node " + key, copy.dump());
    }
    for (const auto* field : {"id", "text", "target"}) {
        auto copy = source;
        copy["nodes"][0]["choices"][0].erase(field);
        cases.emplace_back(std::string("missing choice ") + field, copy.dump());
    }
    auto copy = source;
    copy["callbacks"] = {"grant_admin"};
    cases.emplace_back("unknown root", copy.dump());
    copy = source;
    copy["nodes"][0]["speaker"] = "guard";
    cases.emplace_back("unknown node", copy.dump());
    copy = source;
    copy["nodes"][0]["choices"][0]["enabled"] = true;
    cases.emplace_back("unknown choice", copy.dump());
    rejected(cases);
}

TEST(dialogue_console_definition, shapes_types) {
    const auto source = Json::parse(fixture());
    const std::vector<Json> types{nullptr, true, false, 0, 1, 1.0, "1", Json::object(), Json::array()};
    Cases cases;
    for (const auto& value : types) {
        if (!value.is_object())
            cases.emplace_back("root shape " + value.dump(), value.dump());
        for (const auto* pointer :
             {"/definition_schema", "/id", "/revision", "/entry", "/nodes", "/nodes/0", "/nodes/0/id", "/nodes/0/text", "/nodes/0/kind",
              "/nodes/0/choices", "/nodes/0/choices/0", "/nodes/0/choices/0/id", "/nodes/0/choices/0/text", "/nodes/0/choices/0/target",
              "/nodes/0/choices/0/condition", "/nodes/0/choices/0/effect"}) {
            const std::string name(pointer);
            const bool number = name == "/definition_schema" || name == "/revision";
            const bool array = name == "/nodes" || name.ends_with("/choices");
            const bool object = name == "/nodes/0" || name == "/nodes/0/choices/0";
            if (number ? value.is_number_integer() : array ? value.is_array() : object ? value.is_object() : value.is_string())
                continue;
            auto copy = source;
            copy[Json::json_pointer(name)] = value;
            cases.emplace_back(name + " type " + value.dump(), copy.dump());
        }
    }
    auto copy = source;
    copy["nodes"][0]["kind"] = "automatic";
    cases.emplace_back("unsupported kind", copy.dump());
    rejected(cases);
}

TEST(dialogue_console_definition, schema_revision) {
    const auto source = Json::parse(fixture());
    Cases cases;
    for (const auto* field : {"definition_schema", "revision"}) {
        for (const auto* literal : {"0", "-1", "1.0", "1e0", "18446744073709551616", "-9223372036854775809"}) {
            auto bytes = source.dump();
            const auto key = Json(field).dump() + ":";
            const auto begin = bytes.find(key) + key.size();
            bytes.replace(begin, bytes.find_first_of(",}", begin) - begin, literal);
            cases.emplace_back(std::string(field) + " " + literal, std::move(bytes));
        }
    }
    auto copy = source;
    copy["definition_schema"] = 2;
    cases.emplace_back("unsupported schema", copy.dump());
    rejected(cases);
    copy = source;
    copy["revision"] = std::numeric_limits<std::uint64_t>::max();
    auto compiled = G::compile_definition(copy.dump(), "max-revision");
    ASSERT_TRUE(compiled);
    EXPECT_EQ((*compiled)->revision(), std::numeric_limits<std::uint64_t>::max());
    G::Encounter encounter(*compiled);
    const auto before = encounter.capture().value();
    const auto token = encounter.view()->choices[0].selection;
    auto mismatched = before;
    mismatched.dialogue.definition_revision = 1;
    EXPECT_EQ(encounter.restore(mismatched).error(), G::D::SessionError::RevisionMismatch);
    EXPECT_EQ(encounter.capture().value(), before);
    ASSERT_TRUE(encounter.choose(token));
}

TEST(dialogue_console_definition, limits_strings) {
    auto document = terminal_document();
    const std::string identifier(G::maximum_definition_id_bytes, 'x');
    document["id"] = identifier;
    document["entry"] = identifier;
    document["nodes"][0]["id"] = identifier;
    ASSERT_TRUE(G::compile_definition(document.dump(), "128-byte-identities"));
    Cases cases;
    for (const auto* pointer : {"/id", "/entry", "/nodes/0/id"}) {
        auto copy = document;
        copy[Json::json_pointer(pointer)] = identifier + 'x';
        cases.emplace_back(pointer, copy.dump());
    }
    document = terminal_document();
    document["nodes"][0]["kind"] = "prompt";
    document["nodes"][0]["choices"] = {choice_row(identifier)};
    ASSERT_TRUE(G::compile_definition(document.dump(), "128-byte-choice"));
    for (const auto* field : {"id", "target", "condition", "effect"}) {
        auto copy = document;
        copy["nodes"][0]["choices"][0][field] = identifier + 'x';
        cases.emplace_back(field, copy.dump());
    }
    cases.emplace_back("oversized property", "{\"" + std::string(129, 'x') + "\":1}");
    rejected(cases);
    std::string unicode;
    for (unsigned index = 0; index < 64; ++index)
        unicode += "\xc3\xa9";
    document = terminal_document();
    document["id"] = unicode;
    ASSERT_TRUE(G::compile_definition(document.dump(-1, ' ', true), "escaped-128-decoded-bytes"));
    document["id"] = unicode + "\xc3\xa9";
    EXPECT_FALSE(G::compile_definition(document.dump(), "130-decoded-bytes"));
}

TEST(dialogue_console_definition, limits_text) {
    auto document = terminal_document(4);
    for (auto& node : document["nodes"]) {
        node["text"] = std::string(G::maximum_definition_text_bytes, 't');
        node["kind"] = "prompt";
        node["choices"] = {choice_row("c", node["id"].get<std::string>())};
        node["choices"][0]["text"] = std::string(G::maximum_definition_text_bytes, 'c');
    }
    ASSERT_LT(document.dump().size(), G::maximum_definition_bytes);
    ASSERT_TRUE(G::compile_definition(document.dump(), "exact-32768-text-bytes"));
    Cases cases;
    for (const auto* pointer : {"/nodes/0/text", "/nodes/0/choices/0/text"}) {
        auto copy = document;
        copy[Json::json_pointer(pointer)] = std::string(G::maximum_definition_text_bytes + 1, 'x');
        cases.emplace_back(pointer, copy.dump());
    }
    document["nodes"].push_back({{"id", "extra"}, {"text", "x"}, {"kind", "terminal"}, {"choices", Json::array()}});
    const auto aggregate = G::decode_definition(document.dump(), "aggregate");
    ASSERT_FALSE(aggregate);
    EXPECT_EQ(aggregate.error().field, "$.nodes[4].text");
    cases.emplace_back("aggregate plus one", document.dump());
    rejected(cases);
}

TEST(dialogue_console_definition, limits_nodes_choices) {
    auto document = terminal_document(G::maximum_definition_nodes);
    ASSERT_TRUE(G::compile_definition(document.dump(), "256-nodes"));
    document["nodes"].push_back({{"id", "extra"}, {"text", ""}, {"kind", "terminal"}, {"choices", Json::array()}});
    const auto nodes = G::decode_definition(document.dump(), "257-nodes");
    ASSERT_FALSE(nodes);
    EXPECT_EQ(nodes.error().node, 256);
    EXPECT_EQ(nodes.error().field, "$.nodes[256]");
    rejected({{"257 nodes", document.dump()}});

    document = terminal_document(2);
    for (auto& node : document["nodes"])
        node["kind"] = "prompt";
    document["nodes"][0]["choices"] = {choice_row("a"), choice_row("b")};
    document["nodes"][1]["choices"] = {choice_row("a")};
    ASSERT_TRUE(G::Testing::decode_definition_choice_budget(document.dump(), "three-choices", 3));
    document["nodes"][1]["choices"].push_back(choice_row("b"));
    const auto choices = G::Testing::decode_definition_choice_budget(document.dump(), "four-choices", 3);
    ASSERT_FALSE(choices);
    EXPECT_EQ(choices.error().node, 1);
    EXPECT_EQ(choices.error().choice, 1);
    EXPECT_EQ(choices.error().field, "$.nodes[1].choices[1]");
    EXPECT_NE(choices.error().message.find("budget exceeded (3)"), std::string::npos);
    document = terminal_document();
    document["nodes"][0]["kind"] = "prompt";
    document["nodes"][0]["choices"] = Json::array();
    for (std::size_t index = 0; index < G::maximum_definition_choices; ++index)
        document["nodes"][0]["choices"].push_back(choice_row("", ""));
    ASSERT_GT(document.dump().size(), G::maximum_definition_bytes); // Even the shortest complete rows exceed the byte cap.
    rejected({{"2048 choices already oversized", document.dump()}});
}

TEST(dialogue_console_definition, malformed_depth) {
    const auto good = fixture();
    auto deep = Json::parse(good);
    deep["nodes"][0]["choices"][0]["target"] = Json::object();
    const auto depth = G::decode_definition(deep.dump(), "six-levels");
    ASSERT_FALSE(depth);
    EXPECT_EQ(depth.error().node, 0);
    EXPECT_EQ(depth.error().choice, 0);
    EXPECT_EQ(depth.error().field, "$.nodes[0].choices[0].target");
    rejected(
        {{"empty", ""},
         {"truncated", good.substr(0, good.find_last_of('}'))},
         {"junk", good + "junk"},
         {"second document", good + good},
         {"unterminated", "{\"id\":"},
         {"comment", "/*comment*/" + good},
         {"deep array", "{\"nodes\":" + std::string(10000, '[') + "0" + std::string(10000, ']') + "}"},
         {"six levels", deep.dump()},
         {"bad utf8", "{\"id\":\"" + std::string(1, static_cast<char>(0xff)) + "\"}"}}
    );
}

TEST(dialogue_console_definition, raw_nul_bytes) {
    const auto good = fixture();
    auto interior = good;
    interior.insert(interior.size() / 2, 1, '\0');
    auto exact = good;
    exact.resize(G::maximum_definition_bytes, ' ');
    GateTesting::Directory directory;
    const auto path = directory.file("exact.dialogue.json");
    GateTesting::write_fixture(path, exact);
    ASSERT_TRUE(G::load_definition(path));
    EXPECT_EQ(GateTesting::contents(path), exact);
    rejected(
        {{"NUL alone suffix", good + '\0'},
         {"NUL junk suffix", good + '\0' + "junk"},
         {"NUL document suffix", good + '\0' + good},
         {"interior NUL", interior},
         {"one byte oversized", exact + ' '},
         {"larger oversized", exact + std::string(1000, ' ')}}
    );
}

TEST(dialogue_console_definition, io_and_formats) {
    GateTesting::Directory directory;
    const auto missing = directory.file("missing.json");
    const auto failed = G::load_definition(missing);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().origin, missing);
    EXPECT_FALSE(std::filesystem::exists(missing));
    EXPECT_FALSE(G::load_definition(""));
    EXPECT_FALSE(G::load_definition(missing + '\0' + "suffix"));
    G::Encounter encounter;
    const auto before = encounter.capture().value();
    const auto token = encounter.view()->choices[0].selection;
    EXPECT_FALSE(G::restore_bytes(encounter, fixture()));
    EXPECT_EQ(encounter.capture().value(), before);
    ASSERT_TRUE(encounter.choose(token));
    rejected({{"paired save is not authored content", G::encode(before)}});
}

TEST(dialogue_console_definition, order_cycles_scope) {
    auto document = Json::parse(fixture());
    document["nodes"][1]["choices"][1]["id"] = "introduce"; // Same choice ID in another node is valid.
    document["nodes"][1]["choices"][1]["target"] = "approach";
    std::swap(document["nodes"][0], document["nodes"][3]); // Entry need not be first.
    auto loaded = G::compile_definition(document.dump(), "cycle");
    ASSERT_TRUE(loaded);
    EXPECT_EQ((*loaded)->nodes()[0].id.value, "departed");
    EXPECT_EQ((*loaded)->entry().id.value, "approach");
    std::size_t effects = 0;
    G::Encounter encounter(*loaded, &effects);
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[1].selection)); // silent
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[1].selection)); // explicit return
    EXPECT_EQ(encounter.capture()->dialogue.node.value, "approach");
    EXPECT_EQ(encounter.capture()->dialogue.step, 2);
    EXPECT_EQ(effects, 0);
}
