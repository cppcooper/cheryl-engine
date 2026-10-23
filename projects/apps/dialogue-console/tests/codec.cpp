#include "support.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace {
    namespace G = GateConsole;
    using Json = nlohmann::json;
    using Cases = std::vector<std::pair<std::string, std::string>>;

    std::string valid() {
        G::Encounter encounter;
        return G::encode(encounter.capture().value());
    }

    // Every rejected document crosses the actual file/codec/resume boundary.
    // Compare the entire pair and exact bytes, then use the OLD live selection.
    void rejected(const Cases& cases) {
        GateTesting::Directory directory;
        unsigned sequence = 0;
        for (const auto& [name, bytes] : cases) {
            SCOPED_TRACE(name);
            const auto path = directory.file(std::to_string(++sequence) + ".json");
            GateTesting::write_fixture(path, bytes);
            for (const bool introduced : {false, true}) {
                SCOPED_TRACE(introduced ? "introduced gate" : "initial approach");
                std::size_t effects = 0;
                G::Encounter encounter(&effects);
                if (introduced)
                    ASSERT_TRUE(encounter.choose(encounter.view()->choices[0].selection));
                const auto before = encounter.capture().value();
                const auto old = encounter.view()->choices[0].selection;
                const auto result = G::resume_file(encounter, path);
                ASSERT_FALSE(result);
                EXPECT_FALSE(result.error().empty());
                EXPECT_EQ(encounter.capture().value(), before);
                EXPECT_EQ(GateTesting::contents(path), bytes);
                EXPECT_EQ(effects, introduced ? 1 : 0);
                ASSERT_TRUE(encounter.choose(old));
                EXPECT_EQ(effects, 1);
            }
        }
    }
}

TEST(dialogue_console_codec, ownership_roundtrip) {
    std::size_t effects = 0;
    G::Encounter encounter(&effects);
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[0].selection));
    const auto paired = encounter.capture().value();
    auto bytes = G::encode(paired);
    auto decoded = G::decode(bytes);
    ASSERT_TRUE(decoded);
    bytes.assign(bytes.size(), '?');
    EXPECT_EQ(*decoded, paired);
    G::Encounter fresh(&effects);
    ASSERT_TRUE(fresh.restore(*decoded));
    EXPECT_EQ(fresh.capture().value(), paired);
    ASSERT_TRUE(fresh.choose(fresh.view()->choices[0].selection));
    EXPECT_EQ(effects, 1);
}

TEST(dialogue_console_codec, malformed_trailing) {
    const auto good = valid();
    rejected(
        {{"empty", ""},
         {"whitespace", " \r\n\t"},
         {"truncated", good.substr(0, good.find_last_of('}'))},
         {"unterminated", "{\"save_schema\""},
         {"unquoted key", "{save_schema:1}"},
         {"trailing comma", "{\"save_schema\":1,}"},
         {"trailing junk", good + "junk"},
         {"second document", good + good},
         {"comment", "/* comment */" + good},
         {"bad utf8", "{\"" + std::string(1, static_cast<char>(0xff)) + "\":1}"}}
    );
}

TEST(dialogue_console_codec, duplicate_fields) {
    const auto document = Json::parse(valid());
    Cases cases;
    for (const auto& [key, value] : document.items()) {
        auto bytes = document.dump();
        bytes.insert(1, Json(key).dump() + ":" + value.dump() + ",");
        cases.emplace_back("root duplicate " + key, std::move(bytes));
    }
    for (const auto* object : {"flags", "dialogue"}) {
        for (const auto& [key, value] : document[object].items()) {
            auto child = document[object].dump();
            child.insert(1, Json(key).dump() + ":" + value.dump() + ",");
            auto bytes = document.dump();
            const auto begin = bytes.find(document[object].dump());
            bytes.replace(begin, document[object].dump().size(), child);
            cases.emplace_back(std::string(object) + " duplicate " + key, std::move(bytes));
        }
    }
    auto escaped_root = document.dump();
    escaped_root.insert(1, "\"\\u0073ave_schema\":1,");
    cases.emplace_back("escaped root alias", escaped_root);
    auto escaped_flag = document.dump();
    escaped_flag.insert(escaped_flag.find("\"introduced\""), "\"\\u0069ntroduced\":false,");
    cases.emplace_back("escaped flag alias", escaped_flag);
    auto escaped_session = document.dump();
    escaped_session.insert(escaped_session.find("\"session\""), "\"\\u0073ession\":\"gate-0001\",");
    cases.emplace_back("escaped dialogue alias", escaped_session);
    rejected(cases);
}

TEST(dialogue_console_codec, unknown_missing) {
    const auto document = Json::parse(valid());
    Cases cases;
    for (const auto& [key, value] : document.items()) {
        auto copy = document;
        copy.erase(key);
        cases.emplace_back("missing root " + key, copy.dump());
    }
    for (const auto* object : {"flags", "dialogue"}) {
        for (const auto& [key, value] : document[object].items()) {
            auto copy = document;
            copy[object].erase(key);
            cases.emplace_back(std::string("missing ") + object + "." + key, copy.dump());
        }
        auto copy = document;
        copy[object]["unknown"] = true;
        cases.emplace_back(std::string("unknown ") + object, copy.dump());
    }
    auto copy = document;
    copy["unknown"] = true;
    cases.emplace_back("unknown root", copy.dump());
    rejected(cases);
}

TEST(dialogue_console_codec, shapes_types) {
    const auto document = Json::parse(valid());
    const std::vector<Json> types{nullptr, true, false, 0, 1, 1.0, "1", Json::array(), Json::object()};
    Cases cases;
    for (const auto& type : types) {
        if (!type.is_object())
            cases.emplace_back("root shape " + type.dump(), type.dump());
        for (const auto* object : {"flags", "dialogue"}) {
            if (!type.is_object()) {
                auto copy = document;
                copy[object] = type;
                cases.emplace_back(std::string(object) + " shape " + type.dump(), copy.dump());
            }
        }
        for (const auto* field :
             {"save_schema", "snapshot_schema", "definition_revision", "step", "introduced", "definition", "session", "node"}) {
            const std::string name(field);
            const bool correct = name == "introduced"                                            ? type.is_boolean()
                                 : (name == "definition" || name == "session" || name == "node") ? type.is_string()
                                                                                                 : type.is_number_integer();
            if (correct)
                continue;
            auto copy = document;
            if (name == "save_schema")
                copy[name] = type;
            else if (name == "introduced")
                copy["flags"][name] = type;
            else
                copy["dialogue"][name] = type;
            cases.emplace_back(name + " type " + type.dump(), copy.dump());
        }
    }
    rejected(cases);
}

TEST(dialogue_console_codec, schemas_ranges) {
    const auto document = Json::parse(valid());
    Cases cases;
    for (const auto* field : {"save_schema", "snapshot_schema", "definition_revision", "step"}) {
        for (const auto* literal : {"-1", "1.0", "1e0", "18446744073709551616", "-9223372036854775809"}) {
            auto bytes = document.dump();
            const auto key = Json(field).dump() + ":";
            const auto begin = bytes.find(key) + key.size();
            const auto end = bytes.find_first_of(",}", begin);
            bytes.replace(begin, end - begin, literal);
            cases.emplace_back(std::string(field) + " range " + literal, std::move(bytes));
        }
    }
    for (const auto* field : {"save_schema", "snapshot_schema", "definition_revision"}) {
        for (const auto value : {std::uint64_t{0}, std::uint64_t{2}, std::numeric_limits<std::uint64_t>::max()}) {
            auto copy = document;
            if (std::string_view(field) == "save_schema")
                copy[field] = value;
            else
                copy["dialogue"][field] = value;
            cases.emplace_back(std::string(field) + " unsupported " + std::to_string(value), copy.dump());
        }
    }
    rejected(cases);
    auto maximum = document;
    maximum["dialogue"]["step"] = std::numeric_limits<std::uint64_t>::max();
    G::Encounter encounter;
    ASSERT_TRUE(G::restore_bytes(encounter, maximum.dump()));
    EXPECT_EQ(encounter.capture()->dialogue.step, std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(encounter.choose(encounter.view()->choices[0].selection).error(), G::D::SessionError::StepExhausted);
}

TEST(dialogue_console_codec, content) {
    const auto document = Json::parse(valid());
    Cases cases;
    for (const auto* field : {"definition", "session", "node"}) {
        auto copy = document;
        copy["dialogue"][field] = "";
        cases.emplace_back(std::string("empty ") + field, copy.dump());
    }
    for (const auto* field : {"definition", "node"}) {
        auto copy = document;
        copy["dialogue"][field] = "missing";
        copy["flags"]["introduced"] = true;
        cases.emplace_back(std::string("mismatched ") + field, copy.dump());
    }
    rejected(cases);
    auto edited = document;
    edited["dialogue"]["step"] = 27;
    edited["dialogue"]["node"] = "gate";
    edited["flags"]["introduced"] = false;
    G::Encounter encounter;
    ASSERT_TRUE(G::restore_bytes(encounter, edited.dump()));
    EXPECT_EQ(encounter.capture()->dialogue.step, 27);
    EXPECT_FALSE(encounter.view()->choices[0].available); // No history inference/authentication.
}

TEST(dialogue_console_codec, raw_nul) {
    const auto good = valid();
    auto interior = good;
    interior.insert(interior.size() / 2, 1, '\0');
    rejected(
        {{"NUL suffix alone", good + '\0'},
         {"NUL plus junk", good + '\0' + "junk"},
         {"NUL plus document", good + '\0' + good},
         {"interior NUL", interior}}
    );
}

TEST(dialogue_console_codec, limits_nesting) {
    auto exact = valid();
    exact.resize(G::maximum_save_bytes, ' ');
    G::Encounter encounter;
    ASSERT_TRUE(G::restore_bytes(encounter, exact));
    auto deep = std::string("{\"flags\":{\"introduced\":") + std::string(10000, '[') + "0" + std::string(10000, ']') + "}}";
    rejected(
        {{"one byte oversized", exact + ' '},
         {"larger oversized", exact + std::string(1000, ' ')},
         {"deep arrays", deep},
         {"deep objects", "{\"flags\":{\"introduced\":{\"flags\":{}}}}"}}
    );
    GateTesting::Directory directory;
    const auto path = directory.file("exact.json");
    GateTesting::write_fixture(path, exact);
    EXPECT_EQ(G::read_save(path).value(), exact);
}
