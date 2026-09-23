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
        // TODO: Split CPU-side file decoding/preparation from backend upload and document thread affinity.
        // Parsing and decoding are worker-pool candidates, while an OpenGL provider must marshal context-bound
        // resource creation to the thread that owns the current rendering context.
        [[nodiscard]] virtual std::shared_ptr<Image> load_image(const std::filesystem::path& file) = 0;
        [[nodiscard]] virtual std::shared_ptr<Image> create_font_atlas(std::span<const unsigned char> alpha,
                                                                        PixelSize size) = 0;
        // TODO: The contract says vertex data is transient and fully copied before return, but
        // shared_ptr communicates retainable ownership. Consider a span/view or explicit upload
        // buffer once the allocator/lifetime boundary can express that non-owning contract cleanly.
        [[nodiscard]] virtual std::shared_ptr<Geometry2D> upload_geometry(std::shared_ptr<Vertex2D> vertices,
                                                                           std::uint32_t vertex_count) = 0;
        // TODO: Distinguish compiled stages from linked executable programs in the type system.
        // Returning Shader for both lets a stage-only resource masquerade as a usable material and
        // leaves stage-cache versus program-cache semantics ambiguous.
        [[nodiscard]] virtual std::shared_ptr<Shader> compile_stage(const std::filesystem::path& file) = 0;
        [[nodiscard]] virtual std::shared_ptr<Shader> link_program(
            const std::vector<std::filesystem::path>& stages) = 0;
    };
}
