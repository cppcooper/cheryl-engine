#include "cell-patch.h"
#include "patch-fixture.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace {
    namespace Maps = CE::Maps;
    namespace Patches = CellPatches;

    // Assignment must not replace a held successful result when apply rejects.
    Patches::Error refuses(const Maps::Snapshot& base, const Patches::Patch& patch,
                           Patches::Code code, const std::string& location) {
        const auto original = base;
        const auto original_patch = patch;
        auto held = PatchFixture::expected();
        try {
            held = Patches::apply(base, patch);
            ADD_FAILURE() << "unexpected patch success";
        } catch (const Patches::Error& error) {
            EXPECT_EQ(error.code, code);
            EXPECT_EQ(error.operation, "apply");
            EXPECT_EQ(error.location, location);
            EXPECT_NE(std::string(error.what()).find(location), std::string::npos);
            EXPECT_EQ(base, original);
            EXPECT_EQ(patch, original_patch);
            EXPECT_EQ(held, PatchFixture::expected());
            return error;
        }
        return {code, "unexpected", location, "test did not observe rejection"};
    }

    Maps::Snapshot grid(std::uint32_t width, std::uint32_t height) {
        return {"patch:grid", width, height,
                {{"cells", std::vector<std::optional<std::string>>(std::size_t{width} * height)}}, {}};
    }
}

TEST(patches, literal_two_cell_result) {
    const auto base = PatchFixture::base();
    const auto patch = PatchFixture::patch();
    const auto result = Patches::apply(base, patch);
    EXPECT_EQ(result, PatchFixture::expected()); // Every cell/empty, field, spawn and order.
    EXPECT_EQ(result.id, base.id);
    EXPECT_EQ(result.width, base.width);
    EXPECT_EQ(result.height, base.height);
    EXPECT_EQ(result.spawns, base.spawns);
    ASSERT_EQ(result.layers.size(), 2);
    EXPECT_EQ(result.layers[0].id, "soil");
    EXPECT_EQ(result.layers[1].id, "markers");
    EXPECT_EQ(base, PatchFixture::base());
    EXPECT_EQ(patch, PatchFixture::patch());
}

TEST(patches, witness_is_full_canonical_owned_bytes) {
    auto source = PatchFixture::base();
    auto witness = Patches::capture(source);
    const auto bytes = Maps::encode(source);
    EXPECT_EQ(witness.canonical_bytes(), bytes);
    auto copy = witness;
    source = {};
    witness = {};
    EXPECT_EQ(copy.canonical_bytes(), bytes);
    std::istringstream input{std::string(copy.canonical_bytes())};
    EXPECT_EQ(Maps::parse(input, "owned-witness"), PatchFixture::base());
    EXPECT_EQ(Patches::apply(PatchFixture::base(), {copy, {}}), PatchFixture::base());
}

TEST(patches, empty_batch_is_exact_and_independent) {
    auto base = PatchFixture::base();
    const Patches::Patch patch{Patches::capture(base), {}};
    auto result = Patches::apply(base, patch);
    EXPECT_EQ(result, PatchFixture::base());
    result.layers[0].cells[0] = "garden:clay";
    result.spawns[0].name = "changed";
    EXPECT_EQ(base, PatchFixture::base());
    base.layers[0].cells[5] = "garden:clay";
    refuses(base, patch, Patches::Code::BaseMismatch, "$.base");
}

TEST(patches, clear_and_noop_semantics) {
    const auto base = PatchFixture::base();
    const Patches::Patch patch{Patches::capture(base),
        {{"soil", 1, 0, std::nullopt}, {"markers", 0, 0, std::nullopt},
         {"soil", 0, 0, "garden:earth"}}};
    auto expected = PatchFixture::base();
    expected.layers[0].cells[1] = std::nullopt;
    EXPECT_EQ(Patches::apply(base, patch), expected);
}

TEST(patches, duplicate_address_rejected_even_for_identical_noop) {
    auto patch = PatchFixture::patch();
    patch.edits = {{"soil", 0, 0, "garden:earth"}, {"soil", 0, 0, "garden:earth"}};
    refuses(PatchFixture::base(), patch, Patches::Code::InvalidEdit, "$.edits[1]");
}

TEST(patches, same_coordinates_on_distinct_layers) {
    auto patch = PatchFixture::patch();
    patch.edits = {{"soil", 0, 0, "garden:sand"}, {"markers", 0, 0, "garden:flower"}};
    auto expected = PatchFixture::base();
    expected.layers[0].cells[0] = "garden:sand";
    expected.layers[1].cells[0] = "garden:flower";
    EXPECT_EQ(Patches::apply(PatchFixture::base(), patch), expected);
}

TEST(patches, unknown_and_overlong_layer) {
    auto patch = PatchFixture::patch();
    for (const std::string& layer : {std::string{}, std::string{"Soil"}, std::string{"missing"},
                                    std::string(Maps::max_key_bytes + 1, 's')}) {
        patch.edits[0].layer = layer;
        refuses(PatchFixture::base(), patch, Patches::Code::InvalidEdit, "$.edits[0].layer");
    }
}

TEST(patches, coordinates_bound_before_indexing) {
    auto patch = PatchFixture::patch();
    for (auto x : {2u, std::numeric_limits<std::uint32_t>::max()}) {
        patch.edits[0].x = x;
        refuses(PatchFixture::base(), patch, Patches::Code::InvalidEdit, "$.edits[0].x");
    }
    patch.edits[0].x = 1;
    for (auto y : {3u, std::numeric_limits<std::uint32_t>::max()}) {
        patch.edits[0].y = y;
        refuses(PatchFixture::base(), patch, Patches::Code::InvalidEdit, "$.edits[0].y");
    }
}

TEST(patches, last_coordinate_and_maximum_key) {
    auto patch = PatchFixture::patch();
    const std::string key = "garden:" + std::string(121, 'a');
    ASSERT_EQ(key.size(), Maps::max_key_bytes);
    patch.edits = {{"markers", 1, 2, key}};
    auto expected = PatchFixture::base();
    expected.layers[1].cells[5] = key;
    EXPECT_EQ(Patches::apply(PatchFixture::base(), patch), expected);
}

TEST(patches, invalid_keys_reuse_public_validation) {
    auto patch = PatchFixture::patch();
    for (const std::string& key : {std::string{}, std::string{"plain"}, std::string{"Garden:tile"},
                                  std::string{"garden:a/b"}, std::string{"garden:\xc3\xa9"},
                                  std::string{"garden:a\0b", 10}}) {
        patch.edits[0].tile = key;
        const auto error = refuses(PatchFixture::base(), patch, Patches::Code::InvalidEdit, "$.edits[0].tile");
        ASSERT_TRUE(error.cause);
        EXPECT_EQ(error.cause->code, Maps::Code::InvalidValue);
        EXPECT_EQ(error.cause->operation, "encode");
        EXPECT_EQ(error.cause->location, "$.layers[0].cells[0]");
        EXPECT_EQ(error.cause->origin, "<snapshot>");
    }
    patch.edits[0].tile = std::string(Maps::max_key_bytes + 1, 'a');
    const auto error = refuses(PatchFixture::base(), patch, Patches::Code::InvalidEdit, "$.edits[0].tile");
    EXPECT_FALSE(error.cause); // Size rejected before a bounded probe copy.
}

TEST(patches, unrelated_cell_change_refused) {
    auto base = PatchFixture::base();
    base.layers[0].cells[4] = "garden:clay";
    refuses(base, PatchFixture::patch(), Patches::Code::BaseMismatch, "$.base");
}

TEST(patches, spawn_only_change_refused) {
    auto base = PatchFixture::base();
    base.spawns[0].x = 1;
    refuses(base, PatchFixture::patch(), Patches::Code::BaseMismatch, "$.base");
    base = PatchFixture::base();
    base.spawns[0].name = "entrance";
    refuses(base, PatchFixture::patch(), Patches::Code::BaseMismatch, "$.base");
}

TEST(patches, layer_reordering_refused) {
    auto base = PatchFixture::base();
    std::swap(base.layers[0], base.layers[1]);
    refuses(base, PatchFixture::patch(), Patches::Code::BaseMismatch, "$.base");
}

TEST(patches, identity_and_shape_change_refused) {
    auto base = PatchFixture::base();
    base.id = "garden:south";
    refuses(base, PatchFixture::patch(), Patches::Code::BaseMismatch, "$.base");
    base = PatchFixture::base();
    base.width = 3;
    base.height = 2;
    base.spawns[0].y = 1; // Still valid, and same cell counts.
    refuses(base, PatchFixture::patch(), Patches::Code::BaseMismatch, "$.base");
}

TEST(patches, default_and_moved_from_witnesses_refused) {
    refuses(PatchFixture::base(), {{}, {}}, Patches::Code::InvalidWitness, "$.witness");
    auto witness = Patches::capture(PatchFixture::base());
    auto moved = std::move(witness);
    EXPECT_TRUE(witness.canonical_bytes().empty());
    refuses(PatchFixture::base(), {witness, {}}, Patches::Code::InvalidWitness, "$.witness");
    auto assigned = Patches::capture(PatchFixture::expected());
    assigned = std::move(moved);
    refuses(PatchFixture::base(), {moved, {}}, Patches::Code::InvalidWitness, "$.witness");
    auto* self = &assigned;
    assigned = std::move(*self);
    EXPECT_EQ(Patches::apply(PatchFixture::base(), {assigned, {}}), PatchFixture::base());
}

TEST(patches, invalid_base_owned_codec_cause) {
    auto base = PatchFixture::base();
    base.layers[0].cells[3] = "not-qualified";
    const auto error = refuses(base, PatchFixture::patch(), Patches::Code::InvalidBase, "$.base");
    ASSERT_TRUE(error.cause);
    EXPECT_EQ(error.cause->location, "$.layers[0].cells[3]");
    EXPECT_EQ(error.cause->code, Maps::Code::InvalidValue);
    EXPECT_THROW((void)Patches::capture(base), Patches::Error);
    base = PatchFixture::base();
    base.layers[1].id = "soil";
    refuses(base, PatchFixture::patch(), Patches::Code::InvalidBase, "$.base");
}

TEST(patches, shape_and_count_admission) {
    auto base = PatchFixture::base();
    base.width = 0;
    refuses(base, PatchFixture::patch(), Patches::Code::InvalidBase, "$.base.width");
    base.width = std::numeric_limits<std::uint32_t>::max();
    refuses(base, PatchFixture::patch(), Patches::Code::InvalidBase, "$.base.width");
    base = PatchFixture::base();
    base.height = 0;
    refuses(base, PatchFixture::patch(), Patches::Code::InvalidBase, "$.base.height");
    base = PatchFixture::base();
    base.layers.clear();
    refuses(base, PatchFixture::patch(), Patches::Code::InvalidBase, "$.base.layers");
    base = PatchFixture::base();
    base.layers.resize(Maps::max_layers + 1);
    refuses(base, PatchFixture::patch(), Patches::Code::InvalidBase, "$.base.layers");
    base = PatchFixture::base();
    base.spawns.resize(Maps::max_spawns + 1);
    refuses(base, PatchFixture::patch(), Patches::Code::InvalidBase, "$.base.spawns");
    base = PatchFixture::base();
    base.layers[0].cells.pop_back();
    refuses(base, PatchFixture::patch(), Patches::Code::InvalidBase, "$.base.layers[0].cells");
}

TEST(patches, checked_total_cell_products) {
    auto base = PatchFixture::base();
    base.width = 1024;
    base.height = 1024; // No allocation of a million-cell snapshot is necessary.
    refuses(base, PatchFixture::patch(), Patches::Code::LimitExceeded, "$.base");
    base.height = 64; // Single layer meets cap; two layers do not.
    refuses(base, PatchFixture::patch(), Patches::Code::LimitExceeded, "$.base.layers");
    EXPECT_THROW((void)Patches::capture(base), Patches::Error);
}

TEST(patches, exact_cell_and_batch_caps_succeed) {
    const auto base = grid(1024, 64);
    ASSERT_EQ(base.layers[0].cells.size(), Patches::max_base_cells);
    Patches::Patch patch{Patches::capture(base), {}};
    for (std::uint32_t x = 0; x < Patches::max_edits; ++x)
        patch.edits.push_back({"cells", x, 0, "garden:sand"});
    auto expected = grid(1024, 64);
    std::fill_n(expected.layers[0].cells.begin(), 1024, "garden:sand");
    EXPECT_EQ(Patches::apply(base, patch), expected);
}

TEST(patches, oversized_batch_refused_before_other_validation) {
    Patches::Patch patch{{}, std::vector<Patches::Edit>(Patches::max_edits + 1)};
    refuses({}, patch, Patches::Code::LimitExceeded, "$.edits");
}

TEST(patches, candidate_encoding_limit_preserves_inputs_and_held_result) {
    auto base = grid(1024, 64);
    const std::string key = "garden:" + std::string(121, 'a');
    constexpr std::size_t occupied = 63200;
    std::fill_n(base.layers[0].cells.begin(), occupied, key);
    const auto base_size = Maps::encode(base).size();
    ASSERT_LT(base_size, Maps::max_document_bytes);
    // Each replacement expands null (4 bytes) to a quoted 128-byte ASCII key.
    ASSERT_GT(base_size + Patches::max_edits * (130 - 4), Maps::max_document_bytes);
    Patches::Patch patch{Patches::capture(base), {}};
    for (std::size_t i = occupied; i < occupied + Patches::max_edits; ++i)
        patch.edits.push_back({"cells", static_cast<std::uint32_t>(i % base.width),
                               static_cast<std::uint32_t>(i / base.width), key});
    const auto error = refuses(base, patch, Patches::Code::LimitExceeded, "$.candidate");
    ASSERT_TRUE(error.cause);
    EXPECT_EQ(error.cause->code, Maps::Code::LimitExceeded);
    EXPECT_EQ(error.cause->operation, "encode");
    EXPECT_FALSE(error.cause->location.empty());
}

TEST(patches, invalid_overencoded_base_refused) {
    auto base = grid(1024, 64);
    std::fill(base.layers[0].cells.begin(), base.layers[0].cells.end(),
              "garden:" + std::string(121, 'a'));
    const auto error = refuses(base, PatchFixture::patch(), Patches::Code::LimitExceeded, "$.base");
    ASSERT_TRUE(error.cause);
    EXPECT_EQ(error.cause->code, Maps::Code::LimitExceeded);
    EXPECT_THROW((void)Patches::capture(base), Patches::Error);
}

TEST(patches, independent_source_patch_and_result_lifetimes) {
    Maps::Snapshot held;
    Patches::Patch retained;
    {
        auto source = PatchFixture::base();
        auto original = PatchFixture::patch();
        retained = original;
        held = Patches::apply(source, retained);
        source.layers[0].cells[1] = "garden:clay";
        source.spawns.clear();
        original.edits[0].tile = "garden:clay";
        original.witness = {};
        EXPECT_EQ(held, PatchFixture::expected());
        EXPECT_EQ(retained, PatchFixture::patch());
    }
    auto another = Patches::apply(PatchFixture::base(), retained);
    retained.edits.clear();
    retained.witness = {};
    another.layers[0].cells[1] = "garden:clay";
    another.spawns[0].name = "changed";
    EXPECT_EQ(held, PatchFixture::expected());
}

TEST(patches, diagnostics_survive_all_inputs) {
    const auto error = [] {
        auto base = PatchFixture::base();
        auto patch = PatchFixture::patch();
        patch.edits[1].tile = "bad";
        return refuses(base, patch, Patches::Code::InvalidEdit, "$.edits[1].tile");
    }();
    const auto copy = error;
    EXPECT_EQ(copy.operation, "apply");
    EXPECT_EQ(copy.location, "$.edits[1].tile");
    EXPECT_NE(std::string(copy.what()).find("invalid logical tile key"), std::string::npos);
    ASSERT_TRUE(copy.cause);
    EXPECT_EQ(copy.cause->origin, "<snapshot>");
    EXPECT_EQ(copy.cause->location, "$.layers[0].cells[0]");
    EXPECT_FALSE(copy.cause->byte_offset);
    EXPECT_FALSE(copy.cause->io_error);
}
