#include <cheryl/core/resources/map-storage/codec.h>

#include "patch-fixture.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
#include <utility>

namespace {
    namespace Maps = CE::Maps;

    Maps::Error rejected(const Maps::Snapshot& snapshot, bool encoding) {
        try {
            if (encoding) (void)Maps::encode(snapshot);
            else Maps::validate(snapshot);
            ADD_FAILURE() << "unexpected acceptance";
        } catch (const Maps::Error& error) {
            return error;
        }
        return {Maps::Code::InvalidValue, "unexpected", "", "$", "expected rejection"};
    }

    void parity(const Maps::Snapshot& snapshot, Maps::Code code, const std::string& location) {
        const auto before = snapshot;
        const auto checked = rejected(snapshot, false);
        const auto encoded = rejected(snapshot, true);
        EXPECT_EQ(checked.code, code);
        EXPECT_EQ(checked.code, encoded.code);
        EXPECT_EQ(checked.operation, "validate");
        EXPECT_EQ(encoded.operation, "encode");
        EXPECT_EQ(checked.origin, "<snapshot>");
        EXPECT_EQ(checked.origin, encoded.origin);
        EXPECT_EQ(checked.location, location);
        EXPECT_EQ(checked.location, encoded.location);
        EXPECT_FALSE(checked.byte_offset);
        EXPECT_FALSE(checked.io_error);
        EXPECT_FALSE(checked.cleanup_error);
        EXPECT_TRUE(checked.staging_leftover.empty());
        auto message = std::string(checked.what());
        ASSERT_TRUE(message.starts_with("validate "));
        message.replace(0, 8, "encode");
        EXPECT_EQ(message, encoded.what());
        EXPECT_EQ(snapshot, before);
    }

    Maps::Snapshot exact_cap() {
        Maps::Snapshot result{"validation:cap", 1024, 64,
            {{"cells", std::vector<std::optional<std::string>>(65536)}}, {}};
        const auto baseline = Maps::encode(result).size();
        const std::string key = "p:" + std::string(126, 'a');
        result.layers[0].cells[0] = key;
        // Measure an actual formatter delta; production validation uses no formula.
        const auto delta = Maps::encode(result).size() - baseline;
        const auto count = (Maps::max_document_bytes - baseline) / delta;
        const auto remainder = (Maps::max_document_bytes - baseline) % delta;
        if (count + 1 >= result.layers[0].cells.size()) throw std::logic_error("cap fixture does not fit");
        std::fill_n(result.layers[0].cells.begin(), count, key);
        if (remainder) result.layers[0].cells[count] = "p:" + std::string(remainder, 'a');
        return result;
    }
}

TEST(validation, complete_valid_snapshot) {
    const auto value = PatchFixture::base();
    const auto before = value;
    EXPECT_NO_THROW(Maps::validate(value));
    EXPECT_EQ(value, before);
    std::istringstream input(Maps::encode(value));
    EXPECT_EQ(Maps::parse(input, "validation:memory"), before);
}

TEST(validation, semantic_checks_and_precedence) {
    auto value = PatchFixture::base();
    value.width = 0;
    value.id = "bad";
    parity(value, Maps::Code::InvalidValue, "$.width");
    value = PatchFixture::base();
    value.height = 1025;
    parity(value, Maps::Code::LimitExceeded, "$.height");
    value = PatchFixture::base();
    value.layers.clear();
    parity(value, Maps::Code::InvalidValue, "$.layers");
    value = PatchFixture::base();
    value.layers.resize(Maps::max_layers + 1);
    parity(value, Maps::Code::LimitExceeded, "$.layers");
    value = PatchFixture::base();
    value.id = "bad";
    parity(value, Maps::Code::InvalidValue, "$.id");
    value = PatchFixture::base();
    value.layers[0].cells.pop_back();
    parity(value, Maps::Code::InvalidValue, "$.layers[0].cells");
    value = PatchFixture::base();
    value.layers[1].id = "soil";
    parity(value, Maps::Code::InvalidValue, "$.layers[1].id");
    value = PatchFixture::base();
    value.layers[0].cells[1] = "bad";
    value.spawns[0].layer = "missing";
    parity(value, Maps::Code::InvalidValue, "$.layers[0].cells[1]");
    value = PatchFixture::base();
    value.spawns.resize(Maps::max_spawns + 1);
    parity(value, Maps::Code::LimitExceeded, "$.spawns");
    value = PatchFixture::base();
    value.spawns[0].layer = "missing";
    parity(value, Maps::Code::InvalidValue, "$.spawns[0].layer");
    value = PatchFixture::base();
    value.spawns[1].name = "gate";
    parity(value, Maps::Code::InvalidValue, "$.spawns[1].name");
    value = PatchFixture::base();
    value.spawns[0].x = 2;
    parity(value, Maps::Code::InvalidValue, "$.spawns[0].x");
    value = PatchFixture::base();
    value.spawns[0].y = 3;
    parity(value, Maps::Code::InvalidValue, "$.spawns[0].y");
}

TEST(validation, checked_cell_products) {
    auto value = PatchFixture::base();
    value.width = 1024;
    value.height = 1024;
    parity(value, Maps::Code::LimitExceeded, "$.height");
    value.height = 256;
    parity(value, Maps::Code::LimitExceeded, "$.layers");
}

TEST(validation, ascii_and_escaped_input) {
    auto bytes = Maps::encode(PatchFixture::base());
    const auto start = bytes.find("garden:north");
    ASSERT_NE(start, std::string::npos);
    bytes.replace(start, 12, "garden:\\u006eorth");
    std::istringstream input(bytes);
    const auto value = Maps::parse(input, "validation:escaped-ascii");
    EXPECT_EQ(value, PatchFixture::base());
    EXPECT_NO_THROW(Maps::validate(value));
    EXPECT_EQ(Maps::encode(value), Maps::encode(PatchFixture::base()));
    // V1 admits ASCII identifiers only: no Unicode/escape inflation is hidden.
    for (const std::string& key : {std::string{"p:\xc3\xa9"}, std::string{"p:a\\b"},
                                  std::string{"p:a\"b"}, std::string{"p:a\nb"}, std::string{"p:a\0b", 5}}) {
        auto invalid = value;
        invalid.layers[0].cells[0] = key;
        parity(invalid, Maps::Code::InvalidValue, "$.layers[0].cells[0]");
    }
}

TEST(validation, maximum_and_overlong_keys) {
    auto value = PatchFixture::base();
    value.layers[0].cells[0] = "p:" + std::string(126, 'a');
    EXPECT_NO_THROW(Maps::validate(value));
    value.layers[0].cells[0]->push_back('a');
    parity(value, Maps::Code::LimitExceeded, "$.layers[0].cells[0]");
}

TEST(validation, exact_encoded_byte_cap) {
    auto value = exact_cap();
    ASSERT_EQ(Maps::encode(value).size(), Maps::max_document_bytes);
    EXPECT_NO_THROW(Maps::validate(value));
    auto occupied = std::find_if(value.layers[0].cells.begin(), value.layers[0].cells.end(),
                                 [](const auto& cell) { return cell && cell->size() > 3; });
    ASSERT_NE(occupied, value.layers[0].cells.end());
    occupied->value().pop_back();
    ASSERT_EQ(Maps::encode(value).size(), Maps::max_document_bytes - 1);
    EXPECT_NO_THROW(Maps::validate(value));
}

TEST(validation, one_byte_over_cap_and_owning_diagnostic) {
    const auto error = [] {
        auto value = exact_cap();
        auto empty = std::find(value.layers[0].cells.begin(), value.layers[0].cells.end(), std::nullopt);
        if (empty == value.layers[0].cells.end()) throw std::logic_error("missing cap fixture empty");
        *empty = "p:a"; // Encoded null -> quoted three-byte key adds exactly one byte.
        const auto checked = rejected(value, false);
        parity(value, Maps::Code::LimitExceeded, checked.location);
        return checked;
    }();
    const auto copy = error;
    EXPECT_EQ(copy.code, Maps::Code::LimitExceeded);
    EXPECT_EQ(copy.operation, "validate");
    EXPECT_EQ(copy.origin, "<snapshot>");
    EXPECT_FALSE(copy.location.empty());
    EXPECT_NE(std::string(copy.what()).find("encoded document exceeds 8 MiB"), std::string::npos);
}

TEST(validation, failure_preserves_owned_input_and_prior_result) {
    auto value = PatchFixture::base();
    value.layers[1].cells[2] = "bad";
    const auto before = value;
    auto held = PatchFixture::expected();
    const auto previous = held;
    std::optional<Maps::Error> retained;
    try {
        Maps::validate(value);
        held = value;
        FAIL() << "unexpected validation success";
    } catch (const Maps::Error& error) {
        retained = error;
    }
    EXPECT_EQ(value, before);
    EXPECT_EQ(held, previous);
    value = {};
    ASSERT_TRUE(retained);
    EXPECT_EQ(retained->location, "$.layers[1].cells[2]");
    EXPECT_EQ(retained->operation, "validate");
}

TEST(patches, address_admission_precedes_candidate_key_checks) {
    const auto base = PatchFixture::base();
    auto patch = PatchFixture::patch();
    patch.edits[0].tile = "bad";
    patch.edits[1].layer = "missing";
    const auto before = patch;
    try {
        (void)CE::Maps::CellPatches::apply(base, patch);
        FAIL() << "unexpected batch acceptance";
    } catch (const CE::Maps::CellPatches::Error& error) {
        EXPECT_EQ(error.code, CE::Maps::CellPatches::Code::InvalidEdit);
        EXPECT_EQ(error.location, "$.edits[1].layer");
        EXPECT_FALSE(error.cause);
    }
    EXPECT_EQ(patch, before);
    EXPECT_EQ(base, PatchFixture::base());
}
