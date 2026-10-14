#pragma once

#include "decoded-image.h"
#include "geometry2d.h"
#include "image.h"
#include "sampler.h"
#include "shader.h"

#include <assets/types/primitives/vertex.h>
#include <core/diagnostics.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace CE::Assets::Shaders {
    struct Builder;
}

namespace CE::Rendering::Resources {
    /** Creates resources in one selected backend domain. Uploads obey that
     * backend's owner-thread/current-context rules and copy transient CPU data
     * before returning; returned handles do not borrow pixel/vertex storage.
     * Uploaded contents are immutable for retained frames. A changed image or
     * geometry requires a new handle and explicit publication by its owner.
     * There is no generic in-place update, atomic batch, or residency-budget API.
     * Global asset managers permit one active provider/loading owner; constructing
     * another Loader does not create an independent cache/resource domain.
     * Logical shared handles may outlive cache/provider teardown, but native use
     * requires their original live backend domain. Final release follows backend
     * retirement; it is not a promise of immediate native deletion.
     */
    struct Provider {
    private:
        const CE::Diagnostics::DomainId domain_ = CE::Diagnostics::next_domain_id();

    public:
        Provider() = default;
        // Copying adapter configuration creates another diagnostic identity;
        // assignment retains the destination's identity.
        Provider(const Provider&) noexcept {}
        Provider& operator=(const Provider&) noexcept { return *this; }
        virtual ~Provider();
        [[nodiscard]] CE::Diagnostics::DomainId diagnostic_id() const noexcept { return domain_; }
        // Borrowed optional adapter; it cannot outlive this provider.
        [[nodiscard]] virtual CE::Assets::Shaders::Builder* shader_asset_builder() noexcept { return nullptr; }
        // decode_image() is CPU-only; create_image() and other uploads obey backend thread affinity.
        [[nodiscard]] virtual std::shared_ptr<Image> load_image(const std::filesystem::path& file);
        // Creates a fresh immutable image from owned top-to-bottom RGBA pixels.
        // Caller retains/reuses the input; failure publishes no cache entry here.
        [[nodiscard]] virtual std::shared_ptr<Image> create_image(const CE::Assets::Images::Decoded& image) = 0;
        // Optional immutable sampling override. Unsupported backends reject;
        // MaximumSupported anisotropy may resolve to isotropic filtering.
        [[nodiscard]] virtual std::shared_ptr<const CE::Rendering::Sampling::Sampler>
        create_sampler(const CE::Rendering::Sampling::Options& options);
        // One alpha byte per pixel in the font baker's row/UV order; dimensions must
        // be nonzero and match the span. Copies input before returning on the upload owner.
        [[nodiscard]] virtual std::shared_ptr<Image>
        create_font_atlas(std::span<const unsigned char> alpha, CE::Rendering::Geometry::PixelSize size) = 0;
        // Atlas grids upload triangle strips; whole images and glyphs upload independent triangles.
        [[nodiscard]] virtual std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D> vertices, CE::Rendering::Geometry::Topology topology) = 0;
        // Optional colored layout. Backends which do not implement it reject the
        // request rather than silently discarding color or changing its layout.
        [[nodiscard]] virtual std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2DColor> vertices, CE::Rendering::Geometry::Topology topology);
        // Compatibility owner: retain CPU storage only until the transient upload returns.
        [[nodiscard]] std::shared_ptr<CE::Rendering::Geometry::Buffer2D> upload_geometry(
            std::shared_ptr<CE::Rendering::Geometry::Vertex2D> vertices,
            std::uint32_t vertex_count,
            CE::Rendering::Geometry::Topology topology
        );
        // A CE::Rendering::Resources::Program is executable; the provider compiles its stages and links them before returning.
        [[nodiscard]] virtual std::shared_ptr<Program> link_program(const std::vector<std::filesystem::path>& stages) = 0;
    };
}
