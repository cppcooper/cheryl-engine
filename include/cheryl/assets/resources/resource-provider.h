#pragma once

#include "decoded-image.h"
#include "geometry2d.h"
#include "shader.h"

#include <assets/types/primitives/vertex.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace CE::Assets {
    // Uploads transient CPU data into resources owned by the selected backend.
    // Implementations must finish copying the supplied pixels and vertices before returning.
    struct ResourceProvider {
        virtual ~ResourceProvider();
        // decode_image() is CPU-only; create_image() and other uploads obey backend thread affinity.
        [[nodiscard]] virtual std::shared_ptr<Image> load_image(const std::filesystem::path& file);
        [[nodiscard]] virtual std::shared_ptr<Image> create_image(const DecodedImage& image) = 0;
        [[nodiscard]] virtual std::shared_ptr<Image> create_font_atlas(std::span<const unsigned char> alpha, PixelSize size) = 0;
        // Atlas grids upload triangle strips; whole images and glyphs upload independent triangles.
        [[nodiscard]] virtual std::shared_ptr<Geometry2D> upload_geometry(std::span<const Vertex2D> vertices,
                                                                          PrimitiveTopology topology) = 0;
        // Compatibility owner: retain CPU storage only until the transient upload returns.
        [[nodiscard]] std::shared_ptr<Geometry2D>
        upload_geometry(std::shared_ptr<Vertex2D> vertices, std::uint32_t vertex_count, PrimitiveTopology topology);
        // A Shader is executable; the provider compiles its stages and links them before returning.
        [[nodiscard]] virtual std::shared_ptr<Shader> link_program(const std::vector<std::filesystem::path>& stages) = 0;
    };
}
