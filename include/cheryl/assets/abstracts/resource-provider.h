#pragma once

#include "geometry2d.h"
#include "shader.h"

#include <assets/primitives/vertex.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace CE::Assets {
    // Uploads transient CPU data into resources owned by the selected backend.
    // Implementations must finish copying the supplied pixels and vertices before returning.
    struct ResourceProvider {
        virtual ~ResourceProvider() = default;
        [[nodiscard]] virtual std::shared_ptr<Image> load_image(const std::filesystem::path& file) = 0;
        [[nodiscard]] virtual std::shared_ptr<Image> create_font_atlas(std::span<const unsigned char> alpha,
                                                                        PixelSize size) = 0;
        [[nodiscard]] virtual std::shared_ptr<Geometry2D> upload_geometry(std::shared_ptr<Vertex2D> vertices,
                                                                           std::uint32_t vertex_count) = 0;
        [[nodiscard]] virtual std::shared_ptr<Shader> compile_stage(const std::filesystem::path& file) = 0;
        [[nodiscard]] virtual std::shared_ptr<Shader> link_program(
            const std::vector<std::filesystem::path>& stages) = 0;
    };
}
