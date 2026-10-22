#include "codec.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <new>
#include <sstream>
#include <streambuf>
#include <utility>

namespace {
    namespace MF = MapFixture;
    const std::filesystem::path source = "fixture/input.map.json";

    MF::Snapshot fixture() {
        return {
            .id = "fixture:two-layers", .width = 3, .height = 2,
            .layers = {
                {.id = "ground", .cells = {"fixture:grass", "fixture:stone", "fixture:grass",
                                          "fixture:grass", "fixture:grass", "fixture:stone"}},
                {.id = "overlay", .cells = {std::nullopt, "fixture:wall", std::nullopt,
                                           std::nullopt, std::nullopt, "fixture:wall"}}
            },
            .spawns = {{.name = "entry", .layer = "ground", .x = 1, .y = 1}}
        };
    }

    const std::string canonical = R"({
  "format": "cheryl.map-fixture",
  "version": 1,
  "id": "fixture:two-layers",
  "width": 3,
  "height": 2,
  "layers": [
    {
      "id": "ground",
      "cells": ["fixture:grass", "fixture:stone", "fixture:grass", "fixture:grass", "fixture:grass", "fixture:stone"]
    },
    {
      "id": "overlay",
      "cells": [null, "fixture:wall", null, null, null, "fixture:wall"]
    }
  ],
  "spawns": [
    {"name": "entry", "layer": "ground", "x": 1, "y": 1}
  ]
}
)";

    MF::Snapshot parse_text(const std::string& bytes) {
        std::istringstream input(bytes);
        return MF::parse(input, source);
    }

    std::string changed(const std::string& from, const std::string& to, std::string bytes = canonical) {
        const auto offset = bytes.find(from);
        if (offset == std::string::npos)
            throw std::logic_error("missing mutation target");
        bytes.replace(offset, from.size(), to);
        return bytes;
    }

    void diagnostic(const MF::Error& error, MF::Code code, const std::string& location,
                    const std::string& operation = "parse", const std::filesystem::path& origin = source) {
        EXPECT_EQ(error.code, code);
        EXPECT_EQ(error.operation, operation);
        EXPECT_EQ(error.origin, origin);
        EXPECT_TRUE(error.location.starts_with(location)) << error.location;
        const std::string message = error.what();
        EXPECT_NE(message.find(operation), std::string::npos);
        EXPECT_NE(message.find(origin.string()), std::string::npos);
        EXPECT_NE(message.find(error.location), std::string::npos);
        EXPECT_FALSE(error.location.empty());
    }

    void rejected(const std::string& bytes, MF::Code code, const std::string& location, bool offset = false) {
        // Valid setup precedes the negative operation. Failed assignment must retain this value.
        auto current = fixture();
        const auto previous = current;
        std::istringstream input(bytes);
        try {
            current = MF::parse(input, source);
            FAIL() << "document unexpectedly accepted";
        } catch (const MF::Error& error) {
            diagnostic(error, code, location);
            if (offset) {
                ASSERT_TRUE(error.byte_offset);
                EXPECT_GE(*error.byte_offset, 1u);
            }
        }
        EXPECT_EQ(current, previous);
    }

    void rejected_snapshot(const MF::Snapshot& value, MF::Code code, const std::string& location) {
        const auto previous = value;
        try {
            static_cast<void>(MF::encode(value));
            FAIL() << "snapshot unexpectedly encoded";
        } catch (const MF::Error& error) {
            diagnostic(error, code, location, "encode", "<snapshot>");
        }
        EXPECT_EQ(value, previous);
    }

    struct BadInput {
        std::string name;
        std::string bytes;
        MF::Code code;
        std::string location;
        bool offset = false;
    };

    std::vector<BadInput> bad_inputs() {
        using enum MF::Code;
        std::vector<BadInput> cases{
            {"empty", "", Malformed, "$", true},
            {"syntax", "{]", Malformed, "$", true},
            {"truncated_container", canonical.substr(0, canonical.size() - 4), Malformed, "$", true},
            {"truncated_string", "{\"format\":\"cheryl.map-", Malformed, "$.format", true},
            {"trailing_junk", canonical + "junk", Malformed, "$", true},
            {"second_document", canonical + canonical, Malformed, "$", true},
            {"comment", "/* hi */" + canonical, Malformed, "$", true},
            {"trailing_comma", changed("\"y\": 1}", "\"y\": 1,}"), Malformed, "$", true},
            {"root_array", "[]", InvalidValue, "$"},
            {"root_null", "null", InvalidValue, "$"},
            {"missing_header", "{}", InvalidValue, "$.format"},
            {"format_type", changed("\"cheryl.map-fixture\"", "null"), InvalidValue, "$.format"},
            {"format_unknown", changed("cheryl.map-fixture", "other.map"), UnsupportedFormat, "$.format"},
            {"version_zero", changed("\"version\": 1", "\"version\": 0"), UnsupportedVersion, "$.version"},
            {"version_two", changed("\"version\": 1", "\"version\": 2"), UnsupportedVersion, "$.version"},
            {"version_missing", changed("  \"version\": 1,\n", ""), InvalidValue, "$.version"},
            {"version_string", changed("\"version\": 1", "\"version\": \"1\""), InvalidValue, "$.version"},
            {"version_bool", changed("\"version\": 1", "\"version\": true"), InvalidValue, "$.version"},
            {"version_float", changed("\"version\": 1", "\"version\": 1.0"), InvalidValue, "$.version"},
            {"version_exponent", changed("\"version\": 1", "\"version\": 1e0"), InvalidValue, "$.version"},
            {"width_missing", changed("  \"width\": 3,\n", ""), InvalidValue, "$.width"},
            {"root_unknown", changed("\"width\": 3", "\"extra\": 1, \"width\": 3"), InvalidValue, "$.extra"},
            {"root_duplicate", changed("\"width\": 3", "\"width\": 3, \"width\": 3"), InvalidValue, "$.width"},
            {"escaped_duplicate", changed("\"width\": 3", R"("width": 3, "w\u0069dth": 3)"), InvalidValue, "$.width"},
            {"nested_duplicate", changed("\"id\": \"ground\"", R"("id": "ground", "i\u0064": "ground")"), InvalidValue, "$.layers[0].id"},
            {"duplicate_before_syntax", "{\"width\":1,\"width\":2,]", InvalidValue, "$.width"},
            {"width_zero", changed("\"width\": 3", "\"width\": 0"), InvalidValue, "$.width"},
            {"height_zero", changed("\"height\": 2", "\"height\": 0"), InvalidValue, "$.height"},
            {"width_negative", changed("\"width\": 3", "\"width\": -1"), InvalidValue, "$.width"},
            {"width_bool", changed("\"width\": 3", "\"width\": false"), InvalidValue, "$.width"},
            {"width_float", changed("\"width\": 3", "\"width\": 3.0"), InvalidValue, "$.width"},
            {"width_exponent", changed("\"width\": 3", "\"width\": 3e0"), InvalidValue, "$.width"},
            {"width_u32_overflow", changed("\"width\": 3", "\"width\": 4294967296"), InvalidValue, "$.width"},
            {"width_u64_max", changed("\"width\": 3", "\"width\": 18446744073709551615"), InvalidValue, "$.width"},
            {"width_u64_overflow", changed("\"width\": 3", "\"width\": 18446744073709551616"), InvalidValue, "$.width"},
            {"number_overflow", changed("\"width\": 3", "\"width\": 1e9999"), InvalidValue, "$.width", true},
            {"width_limit", changed("\"width\": 3", "\"width\": 1025"), LimitExceeded, "$.width"},
            {"height_limit", changed("\"height\": 2", "\"height\": 1025"), LimitExceeded, "$.height"},
            {"area_limit", changed("\"height\": 2", "\"height\": 1024", changed("\"width\": 3", "\"width\": 1024")), LimitExceeded, "$.height"},
            {"total_limit", changed("\"height\": 2", "\"height\": 512", changed("\"width\": 3", "\"width\": 512")), LimitExceeded, "$.layers"},
            {"map_id_empty", changed("fixture:two-layers", ""), InvalidValue, "$.id"},
            {"map_id_type", changed("\"fixture:two-layers\"", "5"), InvalidValue, "$.id"},
            {"map_id_colons", changed("fixture:two-layers", "fixture:two:layers"), InvalidValue, "$.id"},
            {"map_id_upper", changed("fixture:two-layers", "Fixture:map"), InvalidValue, "$.id"},
            {"map_id_nonascii", changed("fixture:two-layers", "fixture:m\xc3\xa9"), InvalidValue, "$.id"},
            {"map_id_escaped_nul", changed("fixture:two-layers", R"(fixture:m\u0000)"), InvalidValue, "$.id"},
            {"map_id_long", changed("fixture:two-layers", "f:" + std::string(127, 'x')), LimitExceeded, "$.id"},
            {"layer_unknown", changed("\"id\": \"ground\",", "\"extra\": true, \"id\": \"ground\","), InvalidValue, "$.layers[0].extra"},
            {"layer_id_missing", changed("      \"id\": \"ground\",\n", ""), InvalidValue, "$.layers[0].id"},
            {"layer_id_invalid", changed("\"id\": \"ground\"", "\"id\": \"bad.layer\""), InvalidValue, "$.layers[0].id"},
            {"layer_id_duplicate", changed("\"id\": \"overlay\"", "\"id\": \"ground\""), InvalidValue, "$.layers[1].id"},
            {"cell_count", changed("null, \"fixture:wall\", null, null, null, \"fixture:wall\"", "null"), InvalidValue, "$.layers[1].cells"},
            {"cell_integer", changed("\"fixture:grass\"", "0"), InvalidValue, "$.layers[0].cells[0]"},
            {"cell_bool", changed("\"fixture:grass\"", "true"), InvalidValue, "$.layers[0].cells[0]"},
            {"cell_empty", changed("\"fixture:grass\"", "\"\""), InvalidValue, "$.layers[0].cells[0]"},
            {"spawn_name_missing", changed("\"name\": \"entry\", ", ""), InvalidValue, "$.spawns[0].name"},
            {"spawn_unknown", changed("\"name\": \"entry\"", "\"extra\": 0, \"name\": \"entry\""), InvalidValue, "$.spawns[0].extra"},
            {"spawn_layer", changed("\"layer\": \"ground\"", "\"layer\": \"absent\""), InvalidValue, "$.spawns[0].layer"},
            {"spawn_name_invalid", changed("\"name\": \"entry\"", "\"name\": \"bad:name\""), InvalidValue, "$.spawns[0].name"},
            {"spawn_x", changed("\"x\": 1", "\"x\": 3"), InvalidValue, "$.spawns[0].x"},
            {"spawn_y", changed("\"y\": 1", "\"y\": 2"), InvalidValue, "$.spawns[0].y"},
            {"spawn_negative", changed("\"x\": 1", "\"x\": -1"), InvalidValue, "$.spawns[0].x"},
            {"spawn_float", changed("\"x\": 1", "\"x\": 1e0"), InvalidValue, "$.spawns[0].x"}
        };
        auto utf8 = changed("fixture:two-layers", std::string("f:") + char(0xc3) + '(');
        cases.push_back({"utf8", std::move(utf8), Malformed, "$.id", true});
        for (const auto& suffix : {std::string{}, std::string("junk"), canonical})
            cases.push_back({suffix.empty() ? "nul" : suffix == "junk" ? "nul_junk" : "nul_document",
                             canonical + '\0' + suffix, Malformed, "$", true});
        return cases;
    }

    class parse_rejection : public testing::TestWithParam<BadInput> {};

    TEST_P(parse_rejection, reports_owned_failure) {
        const auto& test = GetParam();
        rejected(test.bytes, test.code, test.location, test.offset);
    }
    INSTANTIATE_TEST_SUITE_P(invalid, parse_rejection, testing::ValuesIn(bad_inputs()),
                            [](const testing::TestParamInfo<BadInput>& item) { return item.param.name; });

    TEST(codec, round_trip) {
        const auto expected = fixture();
        const auto bytes = MF::encode(expected);
        EXPECT_EQ(bytes, canonical);
        EXPECT_EQ(MF::encode(expected), bytes);
        const auto loaded = parse_text(bytes);
        EXPECT_EQ(loaded, expected);
        EXPECT_EQ(loaded.id, "fixture:two-layers");
        EXPECT_EQ(loaded.width, 3u);
        EXPECT_EQ(loaded.height, 2u);
        ASSERT_EQ(loaded.layers.size(), 2u);
        for (std::size_t i = 0; i < 2; ++i) {
            EXPECT_EQ(loaded.layers[i].id, expected.layers[i].id);
            ASSERT_EQ(loaded.layers[i].cells.size(), 6u);
            for (std::size_t cell = 0; cell < 6; ++cell)
                EXPECT_EQ(loaded.layers[i].cells[cell], expected.layers[i].cells[cell]);
        }
        ASSERT_EQ(loaded.spawns.size(), 1u);
        EXPECT_EQ(loaded.spawns[0].name, "entry");
        EXPECT_EQ(loaded.spawns[0].layer, "ground");
        EXPECT_EQ(loaded.spawns[0].x, 1u);
        EXPECT_EQ(loaded.spawns[0].y, 1u);
    }

    TEST(codec, property_order_and_decoded_names) {
        const std::string bytes = R"({
          "spawns":[{"y":1,"layer":"ground","x":1,"name":"entry"}],
          "height":2,"layers":[
            {"cells":["fixture:grass","fixture:stone","fixture:grass","fixture:grass","fixture:grass","fixture:stone"],"id":"ground"},
            {"id":"overlay","cells":[null,"fixture:wall",null,null,null,"fixture:wall"]}],
          "wid\u0074h":3,"id":"fixture:two-layers","version":1,"format":"cheryl.map-fixture"})";
        EXPECT_EQ(parse_text(bytes), fixture());
        EXPECT_EQ(MF::encode(parse_text(bytes)), canonical);
    }

    TEST(codec, independent_values_and_equality) {
        const auto original = fixture();
        auto loaded = parse_text(canonical);
        const auto other = parse_text(canonical);
        const auto differs = [&](auto change) {
            auto candidate = loaded;
            change(candidate);
            EXPECT_NE(candidate, loaded);
            EXPECT_EQ(other, original);
        };
        differs([](auto& value) { value.id = "other:map"; });
        differs([](auto& value) { ++value.width; });
        differs([](auto& value) { ++value.height; });
        differs([](auto& value) { std::swap(value.layers[0], value.layers[1]); });
        differs([](auto& value) { value.layers[0].id = "different"; });
        differs([](auto& value) { value.layers[1].cells[0] = "other:tile"; });
        differs([](auto& value) { value.spawns[0].name = "other"; });
        differs([](auto& value) { value.spawns[0].layer = "overlay"; });
        differs([](auto& value) { value.spawns[0].x = 0; });
        differs([](auto& value) { value.spawns[0].y = 0; });
        loaded.layers.clear();
        loaded.spawns.clear();
        EXPECT_EQ(other, original);
    }

    TEST(codec, unbound_keys_and_empty_spawns) {
        auto value = fixture();
        value.spawns.clear();
        value.layers[0].cells[0] = "not.installed:unknown-tile";
        value.layers[0].id = "0_terrain";
        value.id = "other.pack:world-1";
        EXPECT_EQ(parse_text(MF::encode(value)), value);
    }

    std::string small_document(const std::string& layers, const std::string& spawns = "[]") {
        return R"({"format":"cheryl.map-fixture","version":1,"id":"test:small","width":1,"height":1,"layers":)" +
               layers + R"(,"spawns":)" + spawns + '}';
    }

    TEST(codec, structural_types_and_counts) {
        using enum MF::Code;
        const std::string layer = R"({"id":"ground","cells":[null]})";
        const std::string layers = '[' + layer + ']';
        EXPECT_NO_THROW(static_cast<void>(parse_text(small_document(layers))));
        rejected(small_document("null"), InvalidValue, "$.layers");
        rejected(small_document("[]"), InvalidValue, "$.layers");
        rejected(small_document("[null]"), InvalidValue, "$.layers[0]");
        rejected(small_document(R"([{"id":null,"cells":[null]}])"), InvalidValue, "$.layers[0].id");
        rejected(small_document(R"([{"id":"ground"}])"), InvalidValue, "$.layers[0].cells");
        rejected(small_document(R"([{"id":"ground","cells":null}])"), InvalidValue, "$.layers[0].cells");
        rejected(small_document(R"([{"id":"ground","cells":[]}])"), InvalidValue, "$.layers[0].cells");
        rejected(small_document(R"([{"id":"ground","cells":[null,null]}])"), InvalidValue, "$.layers[0].cells");
        rejected(small_document(layers, "{}"), InvalidValue, "$.spawns");
        rejected(small_document(layers, "[null]"), InvalidValue, "$.spawns[0]");
        rejected(small_document(layers, R"([{"name":"a","layer":"ground","y":0}])"), InvalidValue, "$.spawns[0].x");
        rejected(small_document(layers, R"([{"name":"a","layer":"ground","x":true,"y":0}])"), InvalidValue, "$.spawns[0].x");
        std::string many_layers = "[";
        for (std::size_t i = 0; i < 17; ++i)
            many_layers += (i ? "," : "") + layer;
        rejected(small_document(many_layers + ']'), LimitExceeded, "$.layers");
        std::string many_spawns = "[";
        for (std::size_t i = 0; i < 257; ++i)
            many_spawns += (i ? "," : "") + std::string(R"({"name":"entry-)" ) + std::to_string(i) +
                           R"(","layer":"ground","x":0,"y":0})";
        rejected(small_document(layers, many_spawns + ']'), LimitExceeded, "$.spawns");
    }

    TEST(codec, key_boundaries) {
        auto value = fixture();
        value.id = "f:" + std::string(126, 'x');
        value.layers[0].id = std::string(128, 'g');
        value.layers[0].cells[0] = "t:" + std::string(126, 'z');
        value.spawns[0].layer = value.layers[0].id;
        value.spawns[0].name = std::string(128, 's');
        EXPECT_EQ(parse_text(MF::encode(value)), value);
        value.layers[0].cells[0]->push_back('z');
        rejected_snapshot(value, MF::Code::LimitExceeded, "$.layers[0].cells[0]");
        rejected("{\"" + std::string(129, 'a') + "\":0}", MF::Code::LimitExceeded, "$");
        rejected("{\"" + std::string(128, 'a') + "\":0}", MF::Code::InvalidValue, "$.format");
    }

    MF::Snapshot rectangle(std::uint32_t width, std::uint32_t height, std::size_t count = 1) {
        MF::Snapshot value{.id = "test:bounds", .width = width, .height = height, .layers = {}, .spawns = {}};
        for (std::size_t i = 0; i < count; ++i)
            value.layers.push_back({"layer-" + std::to_string(i), std::vector<std::optional<std::string>>(std::size_t{width} * height)});
        return value;
    }

    TEST(codec, dimensions_and_cell_boundaries) {
        for (const auto& value : {rectangle(1, 1), rectangle(1024, 1), rectangle(1, 1024), rectangle(1024, 256)})
            EXPECT_EQ(parse_text(MF::encode(value)), value);
        auto invalid = rectangle(1, 1);
        invalid.width = std::numeric_limits<std::uint32_t>::max();
        rejected_snapshot(invalid, MF::Code::LimitExceeded, "$.width");
        invalid.width = 0;
        rejected_snapshot(invalid, MF::Code::InvalidValue, "$.width");
        invalid.width = invalid.height = 1024;
        rejected_snapshot(invalid, MF::Code::LimitExceeded, "$.height");
        invalid.width = invalid.height = 512;
        invalid.layers.push_back({"second", {}});
        rejected_snapshot(invalid, MF::Code::LimitExceeded, "$.layers");
        rejected_snapshot(rectangle(1, 1, 0), MF::Code::InvalidValue, "$.layers");
        rejected_snapshot(rectangle(1, 1, 17), MF::Code::LimitExceeded, "$.layers");
        const auto upper_layers = rectangle(1, 1, 16);
        EXPECT_EQ(parse_text(MF::encode(upper_layers)), upper_layers);
        invalid = fixture();
        invalid.layers[1].cells.pop_back();
        rejected_snapshot(invalid, MF::Code::InvalidValue, "$.layers[1].cells");
    }

    TEST(codec, spawn_boundaries_and_revalidation) {
        auto value = rectangle(1024, 1);
        for (std::size_t i = 0; i < 256; ++i)
            value.spawns.push_back({"entry-" + std::to_string(i), "layer-0", 1023, 0});
        EXPECT_EQ(parse_text(MF::encode(value)), value);
        auto invalid = value;
        invalid.spawns.push_back({"too-many", "layer-0", 0, 0});
        rejected_snapshot(invalid, MF::Code::LimitExceeded, "$.spawns");
        invalid = value;
        invalid.spawns[1].name = invalid.spawns[0].name;
        rejected_snapshot(invalid, MF::Code::InvalidValue, "$.spawns[1].name");
        invalid = value;
        invalid.spawns[0].layer = "missing";
        rejected_snapshot(invalid, MF::Code::InvalidValue, "$.spawns[0].layer");
        invalid = value;
        invalid.spawns[0].x = 1024;
        rejected_snapshot(invalid, MF::Code::InvalidValue, "$.spawns[0].x");
        invalid = value;
        invalid.spawns[0].y = 1;
        rejected_snapshot(invalid, MF::Code::InvalidValue, "$.spawns[0].y");
        auto duplicate = changed("\"y\": 1}", "\"y\": 1}, {\"name\":\"entry\",\"layer\":\"ground\",\"x\":0,\"y\":0}");
        rejected(duplicate, MF::Code::InvalidValue, "$.spawns[1].name");
    }

    std::string null_array(std::size_t count) {
        std::string bytes = "[";
        for (std::size_t i = 0; i < count; ++i)
            bytes += i ? ",null" : "null";
        return bytes + ']';
    }

    TEST(codec, depth_limits_and_precedence) {
        // A depth-16 tree passes parsing budgets, then fails the root schema check.
        const auto nested = [](std::size_t depth) { return std::string(depth, '[') + "null" + std::string(depth, ']'); };
        rejected(nested(16), MF::Code::InvalidValue, "$");
        rejected(nested(17), MF::Code::LimitExceeded, "$");
        // Early depth rejection precedes the absent closing brackets/syntax error.
        rejected(std::string(17, '['), MF::Code::LimitExceeded, "$");
    }

    TEST(codec, array_growth_limit) {
        rejected(null_array(MF::max_array_elements), MF::Code::InvalidValue, "$");
        rejected(null_array(MF::max_array_elements + 1), MF::Code::LimitExceeded, "$[262144]");
    }

    TEST(codec, node_growth_limit) {
        // Count the root and two child arrays as well as every scalar value.
        const auto first = null_array(149998);
        rejected('[' + first + ',' + null_array(149999) + ']', MF::Code::InvalidValue, "$");
        rejected('[' + first + ',' + null_array(150000) + ']', MF::Code::LimitExceeded, "$[1][149999]");
    }

    TEST(codec, member_growth_limit) {
        std::string object = "{";
        for (std::size_t i = 0; i < 256; ++i)
            object += (i ? ",\"x" : "\"x") + std::to_string(i) + "\":null";
        rejected(object + '}', MF::Code::InvalidValue, "$.format");
        rejected(object + ",\"extra\":null}", MF::Code::LimitExceeded, "$.extra");
    }

    TEST(codec, member_nodes_count) {
        std::string bytes = "[";
        for (std::size_t i = 0; i < 99999; ++i)
            bytes += i ? R"(,{"x":null})" : R"({"x":null})";
        bytes += ",null,null";
        // 1 root + 99999 * (object, key, null) + 2 nulls = 300000 nodes.
        rejected(bytes + ']', MF::Code::InvalidValue, "$");
        rejected(bytes + ",null]", MF::Code::LimitExceeded, "$[100001]");
    }

    class CountingBuffer final : public std::streambuf {
        std::string bytes_;
        std::size_t position_ = 0;

    public:
        explicit CountingBuffer(std::string bytes) : bytes_(std::move(bytes)) {}
        std::size_t consumed() const { return position_; }

    protected:
        int_type underflow() override {
            return position_ == bytes_.size() ? traits_type::eof() : traits_type::to_int_type(bytes_[position_]);
        }
        int_type uflow() override {
            const auto value = underflow();
            if (!traits_type::eq_int_type(value, traits_type::eof()))
                ++position_;
            return value;
        }
    };

    TEST(codec, byte_acquisition_limit) {
        std::string bytes = canonical;
        bytes.resize(MF::max_document_bytes, ' ');
        EXPECT_EQ(parse_text(bytes), fixture());
        CountingBuffer buffer(bytes + " trailing content never consumed");
        std::istream input(&buffer);
        try {
            static_cast<void>(MF::parse(input, source));
            FAIL() << "oversize document accepted";
        } catch (const MF::Error& error) {
            diagnostic(error, MF::Code::LimitExceeded, "$");
            EXPECT_EQ(error.byte_offset, MF::max_document_bytes + 1);
        }
        EXPECT_EQ(buffer.consumed(), MF::max_document_bytes + 1);
    }

    TEST(codec, raw_nul_preflight) {
        for (const auto& suffix : {std::string{}, std::string("junk"), canonical}) {
            CountingBuffer buffer(canonical + '\0' + suffix);
            std::istream input(&buffer);
            try {
                static_cast<void>(MF::parse(input, source));
                FAIL() << "raw NUL accepted as EOF";
            } catch (const MF::Error& error) {
                diagnostic(error, MF::Code::Malformed, "$");
                EXPECT_EQ(error.byte_offset, canonical.size() + 1);
            }
            EXPECT_EQ(buffer.consumed(), canonical.size() + 1);
        }
        rejected(std::string(1, '\0') + canonical, MF::Code::Malformed, "$", true);
        rejected(changed("fixture:two-layers", std::string("f:") + '\0'), MF::Code::Malformed, "$", true);
    }

    TEST(codec, encoded_byte_limit) {
        auto value = rectangle(512, 128);
        for (auto& cell : value.layers[0].cells)
            cell = "t:x";
        std::size_t remaining = MF::max_document_bytes - MF::encode(value).size();
        for (auto& cell : value.layers[0].cells) {
            const auto extra = std::min(remaining, MF::max_key_bytes - cell->size());
            cell->append(extra, 'x');
            remaining -= extra;
        }
        ASSERT_EQ(remaining, 0u);
        const auto bytes = MF::encode(value);
        ASSERT_EQ(bytes.size(), MF::max_document_bytes);
        EXPECT_EQ(parse_text(bytes), value);
        const auto spare = std::ranges::find_if(value.layers[0].cells, [](const auto& cell) { return cell->size() < 128; });
        ASSERT_NE(spare, value.layers[0].cells.end());
        (*spare)->push_back('x');
        rejected_snapshot(value, MF::Code::LimitExceeded, "$");
    }

    class ThrowingBuffer final : public std::streambuf {
    public:
        enum class Failure { Io, Allocation, Programming };

    private:
        std::string prefix_;
        std::size_t position_ = 0;
        Failure failure_;

    public:
        ThrowingBuffer(std::string prefix, Failure failure) : prefix_(std::move(prefix)), failure_(failure) {}

    protected:
        int_type underflow() override {
            if (position_ < prefix_.size())
                return traits_type::to_int_type(prefix_[position_]);
            if (failure_ == Failure::Io)
                throw std::ios_base::failure("injected stream failure", std::make_error_code(std::io_errc::stream));
            if (failure_ == Failure::Allocation)
                throw std::bad_alloc();
            throw std::logic_error("injected programming failure");
        }
        int_type uflow() override {
            const auto value = underflow();
            ++position_;
            return value;
        }
    };

    TEST(codec, stream_failures) {
        for (const auto& prefix : {std::string{}, canonical.substr(0, 40), canonical}) {
            ThrowingBuffer buffer(prefix, ThrowingBuffer::Failure::Io);
            std::istream input(&buffer);
            const auto mask = input.exceptions();
            try {
                static_cast<void>(MF::parse(input, source));
                FAIL() << "stream failure accepted";
            } catch (const MF::Error& error) {
                diagnostic(error, MF::Code::Read, "$");
                EXPECT_EQ(error.byte_offset, prefix.size() + 1);
                EXPECT_TRUE(error.io_error);
            }
            EXPECT_EQ(input.exceptions(), mask);
            EXPECT_EQ(input.rdstate(), std::ios::goodbit);
        }
        std::istringstream invalid(canonical);
        invalid.setstate(std::ios::badbit);
        try {
            static_cast<void>(MF::parse(invalid, source));
            FAIL() << "bad stream state accepted";
        } catch (const MF::Error& error) {
            diagnostic(error, MF::Code::Read, "$");
            EXPECT_EQ(error.byte_offset, 1u);
        }
    }

    TEST(codec, non_document_exceptions_propagate) {
        ThrowingBuffer allocation(canonical, ThrowingBuffer::Failure::Allocation);
        std::istream memory(&allocation);
        EXPECT_THROW(static_cast<void>(MF::parse(memory, source)), std::bad_alloc);
        ThrowingBuffer programming(canonical, ThrowingBuffer::Failure::Programming);
        std::istream logic(&programming);
        EXPECT_THROW(static_cast<void>(MF::parse(logic, source)), std::logic_error);
    }

    TEST(codec, error_lifetime) {
        std::optional<MF::Error> saved;
        {
            std::filesystem::path transient = "temporary/origin.map.json";
            std::istringstream input("{\"format\":\"unknown\"}");
            try {
                static_cast<void>(MF::parse(input, transient));
                FAIL() << "unknown format accepted";
            } catch (const MF::Error& error) {
                saved.emplace(error);
            }
        }
        ASSERT_TRUE(saved);
        diagnostic(*saved, MF::Code::UnsupportedFormat, "$.format", "parse", "temporary/origin.map.json");
    }
}
