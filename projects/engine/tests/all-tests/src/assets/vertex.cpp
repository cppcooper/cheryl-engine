#include <gtest/gtest.h>

#include <assets/types/primitives/vertex.h>
#include <assets/resources/resource-provider.h>
#include <internals/exceptions.h>

#include <array>
#include <cstddef>
#include <stdexcept>

namespace {
    struct ProviderWithoutColor final : CE::Rendering::Resources::Provider {
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded&) override { return {}; }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, CE::Rendering::Geometry::PixelSize) override {
            return {};
        }
        using CE::Rendering::Resources::Provider::upload_geometry;
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D>, CE::Rendering::Geometry::Topology) override {
            return {};
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override { return {}; }
    };

    float signed_area_twice(const CE::Rendering::Geometry::Triangle& triangle) {
        const auto& a = triangle.vertices[0];
        const auto& b = triangle.vertices[1];
        const auto& c = triangle.vertices[2];
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }
}

TEST(geometry_primitives, quad_faces) {
    // A standalone image keeps two independent triangles and their UVs.
    CE::Rendering::Geometry::Quad quad{};
    quad.vertices = {{{0, 0, 0, 0, 0}, {2, 0, 0, 1, 0}, {2, 3, 0, 1, 1}, {0, 0, 0, 0, 0}, {2, 3, 0, 1, 1}, {0, 3, 0, 0, 1}}};
    CE::Rendering::Geometry::Triangle first{};
    first.vertices = {quad.vertices[0], quad.vertices[1], quad.vertices[2]};
    CE::Rendering::Geometry::Triangle second{};
    second.vertices = {quad.vertices[3], quad.vertices[4], quad.vertices[5]};

    EXPECT_EQ(quad.vertices.size(), std::size_t{6});
    EXPECT_GT(signed_area_twice(first), 0.0f);
    EXPECT_GT(signed_area_twice(second), 0.0f);
    EXPECT_FLOAT_EQ(quad.vertices[0].x, quad.vertices[3].x);
    EXPECT_FLOAT_EQ(quad.vertices[2].u, quad.vertices[4].u);
}

TEST(geometry_primitives, optional_color) {
    ProviderWithoutColor provider;
    const std::array<CE::Rendering::Geometry::Vertex2DColor, 3> vertices{};
    EXPECT_THROW((void)provider.upload_geometry(vertices, CE::Rendering::Geometry::Topology::Triangles), CE::Exceptions::failed_operation);
    EXPECT_EQ(sizeof(CE::Rendering::Geometry::Vertex2D), 5 * sizeof(float));
    EXPECT_EQ(sizeof(CE::Rendering::Geometry::Vertex2DColor), 9 * sizeof(float));
    EXPECT_FLOAT_EQ(vertices[0].r, 1.0f);
    EXPECT_FLOAT_EQ(vertices[0].a, 1.0f);
}

TEST(geometry_primitives, quad_strip) {
    // An atlas cell has a separate four-vertex layout, built from a Triangle
    // followed by its fourth corner. Its two faces retain the same front.
    CE::Rendering::Geometry::Triangle first{};
    first.vertices = {{{0, 0, 0, 0, 0}, {2, 0, 0, 1, 0}, {0, 3, 0, 0, 1}}};
    const CE::Rendering::Geometry::QuadTriangleStrip quad_strip{first, {2, 3, 0, 1, 1}};

    ASSERT_EQ(quad_strip.vertices().size(), std::size_t{4});
    EXPECT_FLOAT_EQ(quad_strip.vertices()[3].u, 1.0f);
    EXPECT_GT(signed_area_twice(quad_strip.triangles.triangle(0)), 0.0f);
    EXPECT_GT(signed_area_twice(quad_strip.triangles.triangle(1)), 0.0f);
    EXPECT_FLOAT_EQ(quad_strip.triangles.triangle(1).vertices[0].y, 3.0f);
}

TEST(geometry_primitives, longer_strip) {
    // Start with a Triangle, then add only one new vertex for each next face.
    CE::Rendering::Geometry::Triangle first{};
    first.vertices = {{{0, 0, 0, 0, 0}, {1, 0, 0, 1, 0}, {0, 1, 0, 0, 1}}};
    const std::array<CE::Rendering::Geometry::Vertex2D, 2> remaining{{{1, 1, 0, 1, 1}, {0, 2, 0, 0, 1}}};
    const CE::Rendering::Geometry::TriangleStrip<3> strip{first, remaining};

    // Even and odd faces reverse the shared edge so all three face forward.
    ASSERT_EQ(strip.vertices.size(), std::size_t{5});
    for (std::size_t face = 0; face < 3; ++face) {
        SCOPED_TRACE(face);
        EXPECT_GT(signed_area_twice(strip.triangle(face)), 0.0f);
    }
    EXPECT_FLOAT_EQ(strip.triangle(2).vertices[2].y, 2.0f);
    EXPECT_THROW(static_cast<void>(strip.triangle(3)), std::out_of_range);
}
