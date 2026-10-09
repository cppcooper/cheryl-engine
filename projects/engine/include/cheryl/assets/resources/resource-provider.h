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

namespace CE::Assets {
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
    struct ResourceProvider {
    private:
        const Diagnostics::DomainId domain_ = Diagnostics::next_domain_id();

    public:
        ResourceProvider() = default;
        // Copying adapter configuration creates another diagnostic identity;
        // assignment retains the destination's identity.
        ResourceProvider(const ResourceProvider&) noexcept {}
        ResourceProvider& operator=(const ResourceProvider&) noexcept { return *this; }
        virtual ~ResourceProvider();
        [[nodiscard]] Diagnostics::DomainId diagnostic_id() const noexcept { return domain_; }
        // decode_image() is CPU-only; create_image() and other uploads obey backend thread affinity.
        [[nodiscard]] virtual std::shared_ptr<Image> load_image(const std::filesystem::path& file);
        // Creates a fresh immutable image from owned top-to-bottom RGBA pixels.
        // Caller retains/reuses the input; failure publishes no cache entry here.
        [[nodiscard]] virtual std::shared_ptr<Image> create_image(const DecodedImage& image) = 0;
        // Optional immutable sampling override. Unsupported backends reject;
        // MaximumSupported anisotropy may resolve to isotropic filtering.
        [[nodiscard]] virtual std::shared_ptr<const Sampler> create_sampler(const SamplerOptions& options);
        // One alpha byte per pixel in the font baker's row/UV order; dimensions must
        // be nonzero and match the span. Copies input before returning on the upload owner.
        [[nodiscard]] virtual std::shared_ptr<Image> create_font_atlas(std::span<const unsigned char> alpha, PixelSize size) = 0;
        // Atlas grids upload triangle strips; whole images and glyphs upload independent triangles.
        [[nodiscard]] virtual std::shared_ptr<Geometry2D>
        upload_geometry(std::span<const Vertex2D> vertices, PrimitiveTopology topology) = 0;
        // Optional colored layout. Backends which do not implement it reject the
        // request rather than silently discarding color or changing its layout.
        [[nodiscard]] virtual std::shared_ptr<Geometry2D>
        upload_geometry(std::span<const Vertex2DColor> vertices, PrimitiveTopology topology);
        // Compatibility owner: retain CPU storage only until the transient upload returns.
        [[nodiscard]] std::shared_ptr<Geometry2D>
        upload_geometry(std::shared_ptr<Vertex2D> vertices, std::uint32_t vertex_count, PrimitiveTopology topology);
        // A Shader is executable; the provider compiles its stages and links them before returning.
        [[nodiscard]] virtual std::shared_ptr<Shader> link_program(const std::vector<std::filesystem::path>& stages) = 0;
    };
}
