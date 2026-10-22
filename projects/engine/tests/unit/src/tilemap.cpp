#include <cheryl/maps/tilemap.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
    namespace Maps = CE::Tilemaps;
    using CE::Exceptions::bad_request;
    using CE::Exceptions::invalid_args;

    static_assert(!std::is_convertible_v<Maps::TileId, Maps::LayerIndex>);
    static_assert(!std::is_convertible_v<Maps::TileId, std::uint32_t>);
    static_assert(std::is_nothrow_move_constructible_v<Maps::Map>);
    static_assert(std::is_nothrow_move_assignable_v<Maps::Map>);

    std::vector<Maps::TileId> snapshot(const Maps::Map& map) {
        std::vector<Maps::TileId> result;
        for (std::uint32_t layer = 0; layer < map.layer_count(); ++layer)
            for (std::uint32_t row = 0; row < map.extent().rows; ++row)
                for (std::uint32_t column = 0; column < map.extent().columns; ++column)
                    result.push_back(map.get({layer}, {column, row}));
        return result;
    }

    Maps::Map fixture() {
        Maps::Map map({3, 2}, 2);
        std::uint32_t value = 1;
        for (std::uint32_t layer = 0; layer < 2; ++layer)
            for (std::uint32_t row = 0; row < 2; ++row)
                for (std::uint32_t column = 0; column < 3; ++column)
                    map.set({layer}, {column, row}, {value++});
        return map;
    }

    void expect_moved_from(Maps::Map& map) {
        EXPECT_EQ(map.extent(), (Maps::Extent{}));
        EXPECT_EQ(map.layer_count(), 0u);
        EXPECT_THROW(static_cast<void>(map.get({0}, {0, 0})), bad_request);
        EXPECT_THROW(map.set({0}, {0, 0}, {1}), bad_request);
    }

    // Independent oracle: enumerate every cell, intersect its actual computed
    // rectangle, and bound the positive-area intersections. No division/search.
    Maps::CellRange intersections(const Maps::Extent extent, const Maps::CellSize size, const Maps::LocalRect rect) {
        if (rect.left == rect.right || rect.top == rect.bottom)
            return {};
        Maps::CellRange result{};
        bool found = false;
        for (std::uint32_t row = 0; row < extent.rows; ++row) {
            const auto top = static_cast<double>(row) * size.height;
            const auto bottom = static_cast<double>(row + 1) * size.height;
            for (std::uint32_t column = 0; column < extent.columns; ++column) {
                const auto left = static_cast<double>(column) * size.width;
                const auto right = static_cast<double>(column + 1) * size.width;
                if (std::max(left, rect.left) < std::min(right, rect.right) && std::max(top, rect.top) < std::min(bottom, rect.bottom)) {
                    if (!found) {
                        result = {column, column + 1, row, row + 1};
                        found = true;
                    } else {
                        result.first_column = std::min(result.first_column, column);
                        result.end_column = std::max(result.end_column, column + 1);
                        result.first_row = std::min(result.first_row, row);
                        result.end_row = std::max(result.end_row, row + 1);
                    }
                }
            }
        }
        return result;
    }

    std::vector<double> edge_probes(const std::uint32_t count, const double size) {
        const auto infinity = std::numeric_limits<double>::infinity();
        std::vector<double> result{-size, (static_cast<double>(count) + 1.0) * size};
        for (std::uint32_t i = 0; i <= count; ++i) {
            const auto edge = static_cast<double>(i) * size;
            result.push_back(std::nextafter(edge, -infinity));
            result.push_back(edge);
            result.push_back(std::nextafter(edge, infinity));
        }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }
}

TEST(tilemap, caller) {
    Maps::Map map({3, 2}, 2);
    map.set({1}, {2, 1}, {42});
    const auto tile = map.get({1}, {2, 1});
    const auto visible = Maps::visible_cells(map.extent(), {0.1, 0.1}, {0.0, 0.0, 0.2, 0.1});
    EXPECT_EQ(tile, (Maps::TileId{42}));
    EXPECT_EQ(visible, (Maps::CellRange{0, 2, 0, 1}));
}

TEST(tilemap, empty) {
    const Maps::Map map({3, 2}, 2);
    EXPECT_EQ(map.extent(), (Maps::Extent{3, 2}));
    EXPECT_EQ(map.layer_count(), 2u);
    EXPECT_EQ(snapshot(map), std::vector<Maps::TileId>(12));
}

TEST(tilemap, edits) {
    auto map = fixture();
    const auto original = snapshot(map);
    std::vector<Maps::TileId> authored;
    for (std::uint32_t i = 1; i <= 12; ++i)
        authored.push_back({i});
    ASSERT_EQ(original, authored);

    for (const Maps::LayerIndex layer : {Maps::LayerIndex{0}, Maps::LayerIndex{1}}) {
        for (const Maps::Point point : {Maps::Point{0, 0}, Maps::Point{2, 0}, Maps::Point{0, 1}, Maps::Point{2, 1}}) {
            const auto old = map.get(layer, point);
            map.set(layer, point, {std::numeric_limits<std::uint32_t>::max()});
            EXPECT_EQ(map.get(layer, point), (Maps::TileId{std::numeric_limits<std::uint32_t>::max()}));
            auto all = snapshot(map);
            EXPECT_EQ(std::count(all.begin(), all.end(), Maps::TileId{std::numeric_limits<std::uint32_t>::max()}), 1);
            map.set(layer, point, {});
            EXPECT_EQ(map.get(layer, point), (Maps::TileId{}));
            map.set(layer, point, old);
            EXPECT_EQ(snapshot(map), original);
        }
    }
    map.set({0}, {1, 1}, {5}); // Same-value edits are harmless.
    EXPECT_EQ(snapshot(map), original);
}

TEST(tilemap, invalid_access) {
    auto map = fixture();
    const auto original = snapshot(map);
    const auto low = std::numeric_limits<std::int64_t>::min();
    const auto high = std::numeric_limits<std::int64_t>::max();
    for (const Maps::Point point :
         {Maps::Point{-1, 0}, Maps::Point{0, -1}, Maps::Point{3, 0}, Maps::Point{0, 2}, Maps::Point{low, 0}, Maps::Point{0, low},
          Maps::Point{high, 0}, Maps::Point{0, high}}) {
        for (const Maps::LayerIndex layer : {Maps::LayerIndex{0}, Maps::LayerIndex{1}}) {
            EXPECT_THROW(static_cast<void>(map.get(layer, point)), bad_request);
            EXPECT_THROW(map.set(layer, point, {42}), bad_request);
            EXPECT_EQ(snapshot(map), original);
        }
    }
    for (const Maps::LayerIndex layer : {Maps::LayerIndex{-1}, Maps::LayerIndex{2}, Maps::LayerIndex{low}, Maps::LayerIndex{high}}) {
        EXPECT_THROW(static_cast<void>(map.get(layer, {0, 0})), bad_request);
        EXPECT_THROW(map.set(layer, {0, 0}, {42}), bad_request);
        EXPECT_EQ(snapshot(map), original);
    }
}

TEST(tilemap, invalid_sizes) {
    EXPECT_THROW(static_cast<void>(Maps::Map({0, 2}, 2)), invalid_args);
    EXPECT_THROW(static_cast<void>(Maps::Map({3, 0}, 2)), invalid_args);
    EXPECT_THROW(static_cast<void>(Maps::Map({3, 2}, 0)), invalid_args);
    const auto maximum = std::numeric_limits<std::uint32_t>::max();
    // Two maximum axes exceed vector storage; a third maximum factor also
    // overflows size_t on supported 32/64-bit targets. Neither case may allocate.
    EXPECT_THROW(static_cast<void>(Maps::Map({maximum, maximum}, 1)), invalid_args);
    EXPECT_THROW(static_cast<void>(Maps::Map({maximum, maximum}, maximum)), invalid_args);
}

TEST(tilemap, copies) {
    Maps::Map independent({3, 2}, 2);
    Maps::Map retained({1, 1}, 1);
    const auto empty = snapshot(independent);
    {
        auto source = fixture();
        const auto original = snapshot(source);
        Maps::Map copy(source);
        EXPECT_EQ(snapshot(copy), original);
        copy.set({1}, {2, 1}, {99});
        EXPECT_EQ(snapshot(source), original);
        EXPECT_EQ(snapshot(independent), empty);
        retained = source;
        source.set({0}, {0, 0}, {});
        EXPECT_EQ(snapshot(retained), original);
    }
    EXPECT_EQ(retained.extent(), (Maps::Extent{3, 2}));
    EXPECT_EQ(retained.layer_count(), 2u);
    EXPECT_EQ(retained.get({1}, {2, 1}), (Maps::TileId{12}));
    retained.set({1}, {2, 1}, {77});
    independent.set({1}, {2, 1}, {88});
    EXPECT_EQ(retained.get({1}, {2, 1}), (Maps::TileId{77}));
    EXPECT_EQ(independent.get({1}, {2, 1}), (Maps::TileId{88}));
}

TEST(tilemap, assignment) {
    auto map = fixture();
    const auto original = snapshot(map);
    auto* self = &map;
    map = *self;
    EXPECT_EQ(snapshot(map), original);

    Maps::Map replacement({2, 3}, 1);
    replacement.set({0}, {1, 2}, {25});
    map = replacement;
    EXPECT_EQ(map.extent(), (Maps::Extent{2, 3}));
    EXPECT_EQ(map.layer_count(), 1u);
    EXPECT_EQ(snapshot(map), snapshot(replacement));
    EXPECT_THROW(static_cast<void>(map.get({1}, {0, 0})), bad_request);
    map.set({0}, {1, 2}, {26});
    EXPECT_EQ(replacement.get({0}, {1, 2}), (Maps::TileId{25}));
}

TEST(tilemap, moves) {
    auto source = fixture();
    const auto original = snapshot(source);
    Maps::Map destination(std::move(source));
    EXPECT_EQ(destination.extent(), (Maps::Extent{3, 2}));
    EXPECT_EQ(destination.layer_count(), 2u);
    EXPECT_EQ(snapshot(destination), original);
    expect_moved_from(source);
    Maps::Map empty_copy(source);
    expect_moved_from(empty_copy);
    source = Maps::Map({1, 2}, 1);
    source.set({0}, {0, 1}, {9});
    EXPECT_EQ(source.get({0}, {0, 1}), (Maps::TileId{9}));
    EXPECT_EQ(snapshot(destination), original);
}

TEST(tilemap, move_assignment) {
    auto source = fixture();
    const auto original = snapshot(source);
    Maps::Map destination({1, 1}, 1);
    destination = std::move(source);
    EXPECT_EQ(destination.extent(), (Maps::Extent{3, 2}));
    EXPECT_EQ(destination.layer_count(), 2u);
    EXPECT_EQ(snapshot(destination), original);
    expect_moved_from(source);
    auto* self = &destination;
    destination = std::move(*self);
    EXPECT_EQ(snapshot(destination), original);
    source = destination;
    EXPECT_EQ(snapshot(source), original);
    Maps::Map transferred(std::move(source));
    destination = source; // Copy assignment from a moved-from value is safely empty.
    expect_moved_from(destination);
    destination = std::move(source); // Moving an already moved-from value is also safe.
    expect_moved_from(destination);
    expect_moved_from(source);
    EXPECT_EQ(snapshot(transferred), original);
}

TEST(tilemap, ranges) {
    const Maps::Extent extent{8, 6};
    const Maps::CellSize size{16, 16};
    EXPECT_EQ(Maps::visible_cells(extent, size, {0, 0, 16, 16}), (Maps::CellRange{0, 1, 0, 1}));
    EXPECT_EQ(Maps::visible_cells(extent, size, {16, 0, 32, 16}), (Maps::CellRange{1, 2, 0, 1}));
    EXPECT_EQ(Maps::visible_cells(extent, size, {15.5, 0, 16.5, 16}), (Maps::CellRange{0, 2, 0, 1}));
    EXPECT_EQ(Maps::visible_cells(extent, size, {-8, -8, 8, 8}), (Maps::CellRange{0, 1, 0, 1}));
    EXPECT_EQ(Maps::visible_cells(extent, size, {112, 80, 128, 96}), (Maps::CellRange{7, 8, 5, 6}));
    EXPECT_EQ(Maps::visible_cells(extent, size, {128, 0, 144, 16}), (Maps::CellRange{}));
    EXPECT_EQ(Maps::visible_cells(extent, size, {0, 0, 0, 16}), (Maps::CellRange{}));
    EXPECT_EQ(Maps::visible_cells(extent, size, {-1, -1, 129, 97}), (Maps::CellRange{0, 8, 0, 6}));
}

TEST(tilemap, outside) {
    const Maps::Extent extent{3, 2};
    const Maps::CellSize size{16, 16};
    for (const Maps::LocalRect rect :
         {Maps::LocalRect{-16, 0, 0, 32}, Maps::LocalRect{48, 0, 64, 32}, Maps::LocalRect{0, -16, 48, 0}, Maps::LocalRect{0, 32, 48, 48},
          Maps::LocalRect{-16, -16, 0, 0}, Maps::LocalRect{48, -16, 64, 0}, Maps::LocalRect{-16, 32, 0, 48},
          Maps::LocalRect{48, 32, 64, 48}, Maps::LocalRect{-64, -64, -32, -32}, Maps::LocalRect{64, 64, 96, 96},
          Maps::LocalRect{8, 8, 8, 16}, Maps::LocalRect{8, 8, 16, 8}}) {
        const auto result = Maps::visible_cells(extent, size, rect);
        EXPECT_TRUE(result.empty());
        EXPECT_EQ(result, (Maps::CellRange{}));
    }
}

TEST(tilemap, invalid_views) {
    const Maps::Extent extent{3, 2};
    const Maps::CellSize size{16, 16};
    const Maps::LocalRect rect{0, 0, 16, 16};
    EXPECT_THROW(static_cast<void>(Maps::visible_cells({0, 2}, size, rect)), invalid_args);
    EXPECT_THROW(static_cast<void>(Maps::visible_cells({3, 0}, size, rect)), invalid_args);
    EXPECT_THROW(static_cast<void>(Maps::visible_cells(extent, size, {2, 0, 1, 1})), invalid_args);
    EXPECT_THROW(static_cast<void>(Maps::visible_cells(extent, size, {0, 2, 1, 1})), invalid_args);
    const auto infinity = std::numeric_limits<double>::infinity();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    for (const auto value : {0.0, -1.0, infinity, -infinity, nan}) {
        EXPECT_THROW(static_cast<void>(Maps::visible_cells(extent, {value, 16}, rect)), invalid_args);
        EXPECT_THROW(static_cast<void>(Maps::visible_cells(extent, {16, value}, rect)), invalid_args);
    }
    for (const auto member : {&Maps::LocalRect::left, &Maps::LocalRect::top, &Maps::LocalRect::right, &Maps::LocalRect::bottom}) {
        for (const auto value : {infinity, -infinity, nan}) {
            auto invalid = rect;
            invalid.*member = value;
            EXPECT_THROW(static_cast<void>(Maps::visible_cells(extent, size, invalid)), invalid_args);
        }
    }
    const auto maximum = std::numeric_limits<double>::max();
    EXPECT_THROW(static_cast<void>(Maps::visible_cells(extent, {maximum, 1}, rect)), invalid_args);
    EXPECT_THROW(static_cast<void>(Maps::visible_cells(extent, {1, maximum}, rect)), invalid_args);
    EXPECT_THROW(static_cast<void>(Maps::visible_cells(extent, {maximum, 1}, {})), invalid_args);
}

TEST(tilemap, extreme_ranges) {
    const auto maximum = std::numeric_limits<double>::max();
    EXPECT_EQ(Maps::visible_cells({3, 2}, {16, 16}, {-maximum, -maximum, maximum, maximum}), (Maps::CellRange{0, 3, 0, 2}));
    EXPECT_EQ(Maps::visible_cells({3, 2}, {16, 16}, {maximum / 2, 0, maximum, 16}), (Maps::CellRange{}));
    EXPECT_EQ(Maps::visible_cells({3, 2}, {16, 16}, {-maximum, 0, -maximum / 2, 16}), (Maps::CellRange{}));
    const auto cells = std::numeric_limits<std::uint32_t>::max();
    EXPECT_EQ(Maps::visible_cells({cells, cells}, {1, 1}, {-maximum, -maximum, maximum, maximum}), (Maps::CellRange{0, cells, 0, cells}));
    EXPECT_EQ(
        Maps::visible_cells(
            {cells, cells}, {0.1, 0.3},
            {static_cast<double>(cells - 1) * 0.1, static_cast<double>(cells - 1) * 0.3, static_cast<double>(cells) * 0.1,
             static_cast<double>(cells) * 0.3}
        ),
        (Maps::CellRange{cells - 1, cells, cells - 1, cells})
    );
}

TEST(tilemap, computed_edges) {
    const Maps::Extent extent{64, 8};
    const Maps::CellSize size{0.1, 0.1};
    // Regression: floor((43*0.1)/0.1) can be 42; ceil((3*0.1)/0.1) can be 4.
    const Maps::LocalRect rect{43 * size.width, 2 * size.height, 44 * size.width, 3 * size.height};
    EXPECT_EQ(Maps::visible_cells(extent, size, rect), (Maps::CellRange{43, 44, 2, 3}));
    EXPECT_EQ(Maps::visible_cells(extent, size, rect), intersections(extent, size, rect));
    auto expanded = rect;
    expanded.left = std::nextafter(rect.left, -std::numeric_limits<double>::infinity());
    expanded.bottom = std::nextafter(rect.bottom, std::numeric_limits<double>::infinity());
    EXPECT_EQ(Maps::visible_cells(extent, size, expanded), (Maps::CellRange{42, 44, 2, 4}));
    EXPECT_EQ(Maps::visible_cells(extent, size, expanded), intersections(extent, size, expanded));
}

TEST(tilemap, intersection_oracle) {
    const Maps::Extent extent{5, 3};
    const auto tiny = std::numeric_limits<double>::denorm_min();
    const std::array sizes{Maps::CellSize{16, 16},         Maps::CellSize{0.1, 0.1},
                           Maps::CellSize{0.3, 0.7},       Maps::CellSize{1.0 / 3.0, 1.0 / 7.0},
                           Maps::CellSize{tiny, tiny * 3}, Maps::CellSize{1e100, 2e100}};
    std::size_t checked = 0;
    for (const auto size : sizes) {
        const auto xs = edge_probes(extent.columns, size.width);
        const auto ys = edge_probes(extent.rows, size.height);
        for (std::size_t left = 0; left < xs.size(); ++left)
            for (std::size_t right = left; right < xs.size(); ++right)
                for (std::size_t top = 0; top < ys.size(); ++top)
                    for (std::size_t bottom = top; bottom < ys.size(); ++bottom) {
                        const Maps::LocalRect rect{xs[left], ys[top], xs[right], ys[bottom]};
                        const auto actual = Maps::visible_cells(extent, size, rect);
                        const auto expected = intersections(extent, size, rect);
                        ASSERT_EQ(actual, expected) << "size=" << size.width << ',' << size.height << " rectangle=" << rect.left << ','
                                                    << rect.top << ',' << rect.right << ',' << rect.bottom;
                        ++checked;
                    }
    }
    EXPECT_GT(checked, 10000u);
}
