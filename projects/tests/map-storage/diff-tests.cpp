#include <cheryl/core/resources/map-storage/cell-patch.h>

#include "patch-fixture.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <utility>

namespace {
    namespace Maps = CE::Maps;
    namespace Patches = CE::Maps::CellPatches;

    Maps::Snapshot mixed_target() {
        return {"garden:north", 2, 3,
                {{"soil", {"garden:earth", std::nullopt, "garden:sand",
                           "garden:water", "garden:earth", "garden:earth"}},
                 {"markers", {"garden:flower", "garden:flag", std::nullopt,
                              std::nullopt, std::nullopt, std::nullopt}}},
                {{"gate", "soil", 0, 2}, {"lookout", "markers", 1, 0}}};
    }

    Maps::Snapshot grid(std::uint32_t width = 32, std::uint32_t height = 64) {
        return {"diff:grid", width, height,
                {{"cells", std::vector<std::optional<std::string>>(std::size_t{width} * height)}}, {}};
    }

    Patches::Error refuses(const Maps::Snapshot& base, const Maps::Snapshot& target,
                           Patches::Code code, const std::string& location) {
        const auto original_base = base;
        const auto original_target = target;
        auto held = PatchFixture::patch();
        const auto previous = held;
        try {
            held = Patches::diff(base, target);
            ADD_FAILURE() << "unexpected diff acceptance";
        } catch (const Patches::Error& error) {
            EXPECT_EQ(error.code, code);
            EXPECT_EQ(error.operation, "diff");
            EXPECT_EQ(error.location, location);
            EXPECT_NE(std::string(error.what()).find(location), std::string::npos);
            EXPECT_EQ(base, original_base);
            EXPECT_EQ(target, original_target);
            EXPECT_EQ(held, previous);
            return error;
        }
        return {code, "unexpected", location, "test did not observe refusal"};
    }
}

TEST(diff, independent_literal_target) {
    const auto base = PatchFixture::base();
    const auto target = PatchFixture::expected();
    const auto patch = Patches::diff(base, target);
    const std::vector<Patches::Edit> expected{{"soil", 1, 0, "garden:sand"}, {"markers", 0, 1, "garden:flower"}};
    EXPECT_EQ(patch.edits, expected);
    EXPECT_EQ(patch.witness.canonical_bytes(), Maps::encode(base));
    EXPECT_EQ(Patches::apply(base, patch), PatchFixture::expected());
    EXPECT_EQ(base, PatchFixture::base());
    EXPECT_EQ(target, PatchFixture::expected());
    EXPECT_EQ(Patches::diff(base, target), patch);
}

TEST(diff, authored_layer_then_row_order) {
    const auto patch = Patches::diff(PatchFixture::base(), mixed_target());
    const std::vector<Patches::Edit> expected{
        {"soil", 1, 0, std::nullopt}, {"soil", 0, 1, "garden:sand"},
        {"markers", 0, 0, "garden:flower"}, {"markers", 0, 2, std::nullopt}};
    EXPECT_EQ(patch.edits, expected); // A later layer's earlier coordinate remains later.
    EXPECT_EQ(Patches::apply(PatchFixture::base(), patch), mixed_target());
}

TEST(diff, reverse_null_transitions) {
    const auto patch = Patches::diff(mixed_target(), PatchFixture::base());
    const std::vector<Patches::Edit> expected{
        {"soil", 1, 0, "garden:stone"}, {"soil", 0, 1, "garden:earth"},
        {"markers", 0, 0, std::nullopt}, {"markers", 0, 2, "garden:tree"}};
    EXPECT_EQ(patch.edits, expected);
    EXPECT_EQ(Patches::apply(mixed_target(), patch), PatchFixture::base());
}

TEST(diff, equal_and_aliased_inputs_yield_bound_empty_patch) {
    auto base = PatchFixture::base();
    const auto patch = Patches::diff(base, base);
    EXPECT_TRUE(patch.edits.empty());
    EXPECT_EQ(patch.witness.canonical_bytes(), Maps::encode(PatchFixture::base()));
    base = {};
    auto result = Patches::apply(PatchFixture::base(), patch);
    EXPECT_EQ(result, PatchFixture::base());
    result.layers[0].cells[0].reset();
    EXPECT_EQ(Patches::apply(PatchFixture::base(), patch), PatchFixture::base());
}

TEST(diff, rejects_metadata_and_layer_changes) {
    const auto base = PatchFixture::base();
    auto target = PatchFixture::expected();
    target.id = "garden:south";
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.id");
    target = PatchFixture::expected();
    target.width = 3;
    target.height = 2;
    target.spawns[0].y = 1;
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.width");
    target = PatchFixture::expected();
    target.height = 4;
    for (auto& layer : target.layers) layer.cells.resize(8);
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.height");
    target = PatchFixture::expected();
    std::swap(target.layers[0], target.layers[1]);
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.layers[0].id");
    target = PatchFixture::expected();
    target.layers[1].id = "new-markers";
    target.spawns[1].layer = "new-markers";
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.layers[1].id");
    target = PatchFixture::expected();
    target.layers.pop_back();
    target.spawns.pop_back();
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.layers");
}

TEST(diff, rejects_every_spawn_field_and_order) {
    const auto base = PatchFixture::base();
    auto target = PatchFixture::expected();
    target.spawns.clear();
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.spawns");
    target = PatchFixture::expected();
    std::swap(target.spawns[0], target.spawns[1]);
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.spawns[0].name");
    target = PatchFixture::expected();
    target.spawns[0].name = "entrance";
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.spawns[0].name");
    target = PatchFixture::expected();
    target.spawns[0].layer = "markers";
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.spawns[0].layer");
    target = PatchFixture::expected();
    target.spawns[0].x = 1;
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.spawns[0].x");
    target = PatchFixture::expected();
    target.spawns[0].y = 1;
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.spawns[0].y");
}

TEST(diff, invalid_base_context) {
    auto base = PatchFixture::base();
    base.width = 0;
    refuses(base, PatchFixture::expected(), Patches::Code::InvalidBase, "$.base.width");
    base = PatchFixture::base();
    base.layers[0].cells.pop_back();
    refuses(base, PatchFixture::expected(), Patches::Code::InvalidBase, "$.base.layers[0].cells");
    base = PatchFixture::base();
    base.layers[0].cells[0] = "bad";
    const auto error = refuses(base, {}, Patches::Code::InvalidBase, "$.base");
    ASSERT_TRUE(error.cause);
    EXPECT_EQ(error.cause->operation, "encode");
    EXPECT_EQ(error.cause->location, "$.layers[0].cells[0]");
}

TEST(diff, invalid_target_context) {
    const auto base = PatchFixture::base();
    auto target = PatchFixture::expected();
    target.height = 0;
    refuses(base, target, Patches::Code::InvalidTarget, "$.target.height");
    target = PatchFixture::expected();
    target.layers[1].cells.pop_back();
    refuses(base, target, Patches::Code::InvalidTarget, "$.target.layers[1].cells");
    target = PatchFixture::expected();
    target.layers[1].cells[2] = "bad";
    const auto error = refuses(base, target, Patches::Code::InvalidTarget, "$.target");
    ASSERT_TRUE(error.cause);
    EXPECT_EQ(error.cause->operation, "validate");
    EXPECT_EQ(error.cause->location, "$.layers[1].cells[2]");
    target = PatchFixture::expected();
    target.spawns[0].layer = "missing";
    refuses(base, target, Patches::Code::InvalidTarget, "$.target");
}

TEST(diff, validation_precedes_metadata_and_change_count) {
    auto base = grid();
    auto target = base;
    std::fill_n(target.layers[0].cells.begin(), 1025, "garden:sand");
    target.id = "different:map";
    refuses(base, target, Patches::Code::UnsupportedChange, "$.target.id");
    target.layers[0].cells[1024] = "bad";
    const auto error = refuses(base, target, Patches::Code::InvalidTarget, "$.target");
    ASSERT_TRUE(error.cause);
    EXPECT_EQ(error.cause->location, "$.layers[0].cells[1024]");
}

TEST(diff, exact_cell_and_change_caps) {
    const auto base = grid(1024, 64);
    auto target = base;
    EXPECT_TRUE(Patches::diff(base, target).edits.empty());
    std::fill_n(target.layers[0].cells.begin(), 1024, "garden:sand");
    const auto patch = Patches::diff(base, target);
    ASSERT_EQ(patch.edits.size(), Patches::max_edits);
    EXPECT_EQ(patch.edits.front(), (Patches::Edit{"cells", 0, 0, "garden:sand"}));
    EXPECT_EQ(patch.edits.back(), (Patches::Edit{"cells", 1023, 0, "garden:sand"}));
    EXPECT_EQ(Patches::apply(base, patch), target);
    auto rows = grid();
    auto rows_target = rows;
    std::fill_n(rows_target.layers[0].cells.begin(), 1024, "garden:sand");
    const auto row_patch = Patches::diff(rows, rows_target);
    EXPECT_EQ(row_patch.edits[31], (Patches::Edit{"cells", 31, 0, "garden:sand"}));
    EXPECT_EQ(row_patch.edits[32], (Patches::Edit{"cells", 0, 1, "garden:sand"}));
    EXPECT_EQ(row_patch.edits.back(), (Patches::Edit{"cells", 31, 31, "garden:sand"}));
    EXPECT_EQ(Patches::apply(rows, row_patch), rows_target);
}

TEST(diff, rejects_change_1025) {
    const auto base = grid();
    auto target = base;
    std::fill_n(target.layers[0].cells.begin(), 1025, "garden:sand");
    refuses(base, target, Patches::Code::LimitExceeded, "$.target.layers[0].cells[1024]");
}

TEST(diff, rejects_patch_cell_budget_before_metadata) {
    const auto outside = grid(1024, 65); // Valid under the larger public codec budget.
    EXPECT_NO_THROW(Maps::validate(outside));
    refuses(outside, PatchFixture::expected(), Patches::Code::LimitExceeded, "$.base");
    refuses(PatchFixture::base(), outside, Patches::Code::LimitExceeded, "$.target");
}

TEST(diff, byte_caps_for_both_inputs) {
    auto base = grid(1024, 64);
    const std::string key = "p:" + std::string(126, 'a');
    std::fill_n(base.layers[0].cells.begin(), 63200, key);
    EXPECT_NO_THROW(Maps::validate(base));
    auto target = base;
    target.layers[0].cells[63200] = key;
    const auto valid = Patches::diff(base, target);
    ASSERT_EQ(valid.edits.size(), 1);
    EXPECT_EQ(Patches::apply(base, valid), target);
    std::fill_n(target.layers[0].cells.begin() + 63200, 1024, key);
    const auto error = refuses(base, target, Patches::Code::LimitExceeded, "$.target");
    ASSERT_TRUE(error.cause);
    EXPECT_EQ(error.cause->code, Maps::Code::LimitExceeded);
    EXPECT_EQ(error.cause->operation, "validate");
    std::fill(base.layers[0].cells.begin(), base.layers[0].cells.end(), key);
    const auto base_error = refuses(base, PatchFixture::expected(), Patches::Code::LimitExceeded, "$.base");
    ASSERT_TRUE(base_error.cause);
    EXPECT_EQ(base_error.cause->operation, "encode");
}

TEST(diff, owned_input_and_patch_lifetimes) {
    Patches::Patch held;
    Maps::Snapshot result;
    {
        auto base = PatchFixture::base();
        auto target = PatchFixture::expected();
        held = Patches::diff(base, target);
        result = Patches::apply(base, held);
        base.id = "destroyed:base";
        target.layers.clear();
        target.spawns.clear();
    }
    const auto independent = held;
    EXPECT_EQ(Patches::apply(PatchFixture::base(), independent), PatchFixture::expected());
    held.witness = {};
    held.edits[0].tile = "garden:clay";
    EXPECT_EQ(result, PatchFixture::expected());
    EXPECT_EQ(independent.edits[0].tile, "garden:sand");
    EXPECT_EQ(independent.witness.canonical_bytes(), Maps::encode(PatchFixture::base()));
}

TEST(diff, altered_original_base_refuses_application) {
    const auto patch = Patches::diff(PatchFixture::base(), PatchFixture::expected());
    for (int variant = 0; variant < 3; ++variant) {
        auto changed = PatchFixture::base();
        if (variant == 0) changed.layers[0].cells[4] = "garden:clay";
        else if (variant == 1) std::swap(changed.layers[0], changed.layers[1]);
        else std::swap(changed.spawns[0], changed.spawns[1]);
        const auto original = changed;
        const auto original_patch = patch;
        auto held = PatchFixture::expected();
        try {
            held = Patches::apply(changed, patch);
            FAIL() << "changed base unexpectedly accepted";
        } catch (const Patches::Error& error) {
            EXPECT_EQ(error.code, Patches::Code::BaseMismatch);
            EXPECT_EQ(error.operation, "apply");
            EXPECT_EQ(error.location, "$.base");
        }
        EXPECT_EQ(changed, original);
        EXPECT_EQ(patch, original_patch);
        EXPECT_EQ(held, PatchFixture::expected());
    }
}

TEST(diff, owning_diagnostics_survive_inputs_and_original_error) {
    std::optional<Patches::Error> retained;
    {
        auto target = PatchFixture::expected();
        target.layers[1].cells[2] = "bad";
        retained = refuses(PatchFixture::base(), target, Patches::Code::InvalidTarget, "$.target");
    }
    const auto copy = *retained;
    retained.reset();
    EXPECT_EQ(copy.operation, "diff");
    EXPECT_EQ(copy.location, "$.target");
    EXPECT_NE(std::string(copy.what()).find("diff $.target"), std::string::npos);
    ASSERT_TRUE(copy.cause);
    EXPECT_EQ(copy.cause->origin, "<snapshot>");
    EXPECT_EQ(copy.cause->location, "$.layers[1].cells[2]");
    EXPECT_EQ(copy.cause->operation, "validate");
}
