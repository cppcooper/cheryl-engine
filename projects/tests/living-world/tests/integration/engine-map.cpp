#include <cheryl/maps/tilemap.h>
#include <internals/exceptions.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

static_assert(std::is_nothrow_copy_constructible_v<CE::Exceptions::invalid_args>);
static_assert(std::is_nothrow_move_constructible_v<CE::Exceptions::invalid_args>);
static_assert(std::is_nothrow_copy_constructible_v<CE::Exceptions::bad_request>);
static_assert(std::is_nothrow_move_constructible_v<CE::Exceptions::bad_request>);

TEST(living_integration, engine_invalid_map_exception) {
    std::optional<CE::Exceptions::invalid_args> retained;
    try {
        const CE::Tilemaps::Map invalid({0, 1}, 1);
        FAIL() << "Zero columns must be rejected by Engine";
    } catch (const std::exception& error) {
        const auto* category = dynamic_cast<const CE::Exceptions::invalid_args*>(&error);
        ASSERT_NE(category, nullptr);
        retained.emplace(*category);
    }
    ASSERT_TRUE(retained.has_value());
    const std::string diagnostic(retained->what());
    auto moved = std::move(*retained);
    EXPECT_STREQ(moved.what(), diagnostic.c_str());
    EXPECT_NE(diagnostic.find("Tilemap dimensions and layer count must be nonzero"), std::string::npos);
}

TEST(living_integration, engine_bad_map_request_preserves_layers) {
    CE::Tilemaps::Map presentation({1, 1}, 2);
    presentation.set({0}, {0, 0}, {100});
    presentation.set({1}, {0, 0}, {1});
    std::optional<CE::Exceptions::bad_request> retained;
    try {
        presentation.set({2}, {0, 0}, {2});
        FAIL() << "Out-of-range layer must be rejected by Engine";
    } catch (const std::exception& error) {
        const auto* category = dynamic_cast<const CE::Exceptions::bad_request*>(&error);
        ASSERT_NE(category, nullptr);
        retained.emplace(*category);
    }
    ASSERT_TRUE(retained.has_value());
    const std::string diagnostic(retained->what());
    auto moved = std::move(*retained);
    EXPECT_STREQ(moved.what(), diagnostic.c_str());
    EXPECT_NE(diagnostic.find("Tilemap layer or cell coordinates are out of bounds"), std::string::npos);
    EXPECT_EQ(presentation.get({0}, {0, 0}).value, 100U);
    EXPECT_EQ(presentation.get({1}, {0, 0}).value, 1U);
}
