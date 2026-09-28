#include <gtest/gtest.h>

#include <assets/types/primitives/vertex.h>

#include <array>
#include <cstddef>
#include <stdexcept>

namespace {
    float signed_area_twice(const CE::Triangle& triangle) {
        const auto& a = triangle.vertices[0];
        const auto& b = triangle.vertices[1];
        const auto& c = triangle.vertices[2];
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }
}

TEST(geometry_primitives, quad_faces) {
    // A standalone image keeps two independent triangles and their UVs.
    CE::Quad quad{};
    quad.vertices = {{{0, 0, 0, 0, 0}, {2, 0, 0, 1, 0}, {2, 3, 0, 1, 1},
                      {0, 0, 0, 0, 0}, {2, 3, 0, 1, 1}, {0, 3, 0, 0, 1}}};
    CE::Triangle first{};
    first.vertices = {quad.vertices[0], quad.vertices[1], quad.vertices[2]};
    CE::Triangle second{};
    second.vertices = {quad.vertices[3], quad.vertices[4], quad.vertices[5]};

    EXPECT_EQ(quad.vertices.size(), std::size_t{6});
    EXPECT_GT(signed_area_twice(first), 0.0f);
    EXPECT_GT(signed_area_twice(second), 0.0f);
    EXPECT_FLOAT_EQ(quad.vertices[0].x, quad.vertices[3].x);
    EXPECT_FLOAT_EQ(quad.vertices[2].u, quad.vertices[4].u);
}

TEST(geometry_primitives, quad_strip) {
    // An atlas cell has a separate four-vertex layout, built from a Triangle
    // followed by its fourth corner. Its two faces retain the same front.
    CE::Triangle first{};
    first.vertices = {{{0, 0, 0, 0, 0}, {2, 0, 0, 1, 0}, {0, 3, 0, 0, 1}}};
    const CE::QuadTriangleStrip quad_strip{first, {2, 3, 0, 1, 1}};

    ASSERT_EQ(quad_strip.vertices().size(), std::size_t{4});
    EXPECT_FLOAT_EQ(quad_strip.vertices()[3].u, 1.0f);
    EXPECT_GT(signed_area_twice(quad_strip.triangles.triangle(0)), 0.0f);
    EXPECT_GT(signed_area_twice(quad_strip.triangles.triangle(1)), 0.0f);
    EXPECT_FLOAT_EQ(quad_strip.triangles.triangle(1).vertices[0].y, 3.0f);
}

TEST(geometry_primitives, longer_strip) {
    // Start with a Triangle, then add only one new vertex for each next face.
    CE::Triangle first{};
    first.vertices = {{{0, 0, 0, 0, 0}, {1, 0, 0, 1, 0}, {0, 1, 0, 0, 1}}};
    const std::array<CE::Vertex2D, 2> remaining{{{1, 1, 0, 1, 1}, {0, 2, 0, 0, 1}}};
    const CE::TriangleStrip<3> strip{first, remaining};

    // Even and odd faces reverse the shared edge so all three face forward.
    ASSERT_EQ(strip.vertices.size(), std::size_t{5});
    for (std::size_t face = 0; face < 3; ++face) {
        SCOPED_TRACE(face);
        EXPECT_GT(signed_area_twice(strip.triangle(face)), 0.0f);
    }
    EXPECT_FLOAT_EQ(strip.triangle(2).vertices[2].y, 2.0f);
    EXPECT_THROW(static_cast<void>(strip.triangle(3)), std::out_of_range);
}
