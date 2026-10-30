#pragma once

#include <cheryl/export.h>

#include "decoded-image.h"
#include "geometry2d.h"
#include "image.h"
#include "sampler.h"
#include "shader.h"

#include <assets/types/primitives/vertex.h>
#include <core/diagnostics.h>
#include <core/rendering/graphics-domain.h>
#include <core/subsystems/event-bus.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
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
    struct CE_API Provider {
    private:
        const CE::Diagnostics::DomainId domain_ = CE::Diagnostics::next_domain_id();
        mutable std::mutex notification_mutex_;
        CE::Events::EventBus::Publisher notification_publisher_;

    public:
        Provider() = default;
        // Copying adapter configuration creates another diagnostic identity;
        // assignment retains the destination's identity.
        Provider(const Provider&) noexcept {}
        Provider& operator=(const Provider&) noexcept { return *this; }
        virtual ~Provider();
        [[nodiscard]] CE::Diagnostics::DomainId diagnostic_id() const noexcept { return domain_; }
        /** Set the weak catalogue route before loading/binding managers. Copies of Provider start unrouted. */
        void publish_events_to(CE::Events::EventBus::Publisher publisher);
        /** Copy the configured route under its mutex; retains no bus/Context. */
        [[nodiscard]] CE::Events::EventBus::Publisher notification_publisher() const;
        /** Forward the renderer's retained native domain, or an empty token when unsupported. */
        [[nodiscard]] virtual CE::Rendering::GraphicsDomain graphics_domain() const noexcept { return {}; }
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

/**
 * @fn CE::Rendering::Resources::Provider::Provider()
 * @brief Create a distinct diagnostic provider identity without binding the global asset-cache domain.
 */

/**
 * @fn CE::Rendering::Resources::Provider::Provider(const Provider&) noexcept
 * @brief Copy adapter configuration with a new diagnostic identity; no asset-cache domain is shared by this operation.
 */

/**
 * @fn CE::Rendering::Resources::Provider& CE::Rendering::Resources::Provider::operator=(const Provider&) noexcept
 * @brief Preserve this provider's diagnostic identity during base assignment.
 * @return This provider. Derived adapter configuration follows the derived type's assignment contract.
 */

/**
 * @fn CE::Rendering::Resources::Provider::~Provider()
 * @brief Release global typed caches if this provider owns their active domain.
 * Cache handles are dropped before releasing the binding; external retained handles survive logically in their original backend domain. Quiesce loading/publication and arrange backend teardown in the owning runtime; this is not native resource migration.
 */

/**
 * @fn CE::Diagnostics::DomainId CE::Rendering::Resources::Provider::diagnostic_id() const noexcept
 * @return Stable identity for this provider instance; copying creates another identity, assignment preserves the destination.
 */

/**
 * @fn CE::Assets::Shaders::Builder* CE::Rendering::Resources::Provider::shader_asset_builder() noexcept
 * @return Borrowed optional material/program builder, or null when unsupported. Retain this provider through use; the base implementation returns null.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Resources::Image> CE::Rendering::Resources::Provider::load_image(const std::filesystem::path& file)
 * @brief Decode a file on the CPU, then create a fresh immutable backend image.
 * @param file File opened during this call; no path/cache entry is published here.
 * @return Retained image from create_image().
 * The upload portion obeys backend owner/current-context requirements. Decode/upload/allocation failures propagate without returning a partial handle.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Resources::Image> CE::Rendering::Resources::Provider::create_image(const CE::Assets::Images::Decoded& image)
 * @brief Upload fresh top-to-bottom RGBA8 contents on the backend loading owner.
 * @param image Nonzero dimensions and exactly four bytes per pixel, borrowed only through this call.
 * @return Independent immutable image; caller may immediately reuse/release CPU pixels.
 * Backend validation/upload failures propagate; this operation publishes no cache entry.
 */

/**
 * @fn std::shared_ptr<const CE::Rendering::Sampling::Sampler> CE::Rendering::Resources::Provider::create_sampler(const CE::Rendering::Sampling::Options& options)
 * @brief Create or reuse an immutable backend sampler on its owner/current context.
 * @param options Valid filtering/wrap/anisotropy request.
 * @return Retained sampler; MaximumSupported may resolve to isotropic filtering.
 * The base validates options then throws failed_operation because sampling overrides are optional. Concrete caching does not promise residency after final release.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Resources::Image> CE::Rendering::Resources::Provider::create_font_atlas(std::span<const unsigned char> alpha, CE::Rendering::Geometry::PixelSize size)
 * @brief Upload one-channel glyph alpha without changing the font baker's row/UV order.
 * @param alpha One byte per pixel, borrowed until this upload returns.
 * @param size Nonzero dimensions matching alpha.size().
 * @return Independent immutable atlas handle.
 * Requires backend owner/current context; malformed input and upload/allocation failures reject without cache publication.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Geometry::Buffer2D> CE::Rendering::Resources::Provider::upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D> vertices, CE::Rendering::Geometry::Topology topology)
 * @brief Copy transient position/UV vertices into immutable backend geometry.
 * @param vertices Nonempty contiguous CPU vertices, borrowed only through the call.
 * @param topology Primitive interpretation retained by the upload.
 * @return Independent retained geometry with matching layout/count/topology.
 * Requires backend upload affinity; validation/native/allocation errors propagate. Atlas cells use strips; whole images/glyphs use independent triangles.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Geometry::Buffer2D> CE::Rendering::Resources::Provider::upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2DColor> vertices, CE::Rendering::Geometry::Topology topology)
 * @brief Copy transient position/UV/RGBA vertices into immutable backend geometry.
 * @param vertices Nonempty contiguous CPU vertices, borrowed only through the call.
 * @param topology Primitive interpretation retained by the upload.
 * @return Independent retained geometry with matching layout/count/topology.
 * Requires backend upload affinity; validation/native/allocation errors propagate. The base throws failed_operation: unsupported adapters must reject rather than discard color.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Geometry::Buffer2D> CE::Rendering::Resources::Provider::upload_geometry(std::shared_ptr<CE::Rendering::Geometry::Vertex2D> vertices, std::uint32_t vertex_count, CE::Rendering::Geometry::Topology topology)
 * @brief Compatibility wrapper borrowing a retained CPU array for the transient span upload.
 * @param vertices Owner of at least vertex_count contiguous vertices.
 * @param vertex_count Nonzero array count, not bytes.
 * @param topology Uploaded primitive interpretation.
 * @return Backend geometry independent of the CPU array.
 * Null/zero input throws invalid_args. The CPU owner is retained only until the delegated upload returns; normal backend affinity/failures apply.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Resources::Program> CE::Rendering::Resources::Provider::link_program(const std::vector<std::filesystem::path>& stages)
 * @brief Compile stage files and return a linked executable on the backend owner/current context.
 * @param stages Stage paths borrowed through the call; backend extension/stage support applies.
 * @return Complete executable handle, never an unlinked recipe.
 * File/compile/link/allocation failures propagate; this primitive performs no cache publication.
 * @see [Resource residency](cheryl-guide:resources/resource-residency)
 */
