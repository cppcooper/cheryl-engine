#pragma once

#include <cheryl/export.h>

#include <text/layout.h>
#include <assets/resources/geometry2d.h>
#include <assets/resources/image.h>
#include <assets/types/primitives/vertex.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace CE::Rendering::Resources {
    struct Provider;
    class Material;
}

namespace CE::Text::Rendering {
    /** CPU atlas/optional positioned-run preparation policy, independent of a resource provider. */
    struct RasterOptions {
        std::uint32_t maximum_page_extent = 1024; ///< Maximum page dimension 4..4096; pages crop to used padded bounds.
        bool positioned_runs = false; ///< Also prepare geometry supporting one whole-text transform with authored draw order.
    };
    /** Owned CPU grayscale atlas page and per-glyph geometry. */
    struct PreparedPage {
        CE::Rendering::Geometry::PixelSize size; ///< Atlas width/height in pixels.
        std::vector<unsigned char> alpha; ///< Tightly packed top-to-bottom single-channel coverage bytes.
        std::vector<CE::Rendering::Geometry::Vertex2D> vertices; ///< Six-vertex local glyph quads with page UVs.
    };
    /** One visible glyph placement within this immutable generation. */
    struct GlyphDraw {
        std::size_t page{} /**< Page-array index. */, index{}; ///< Six-vertex glyph quad index within that page.
        float x{} /**< Local glyph X. */, y{}; ///< Local Y-up glyph offset.
    };
    /** Consecutive positioned triangle-list vertices sharing one atlas, in authored glyph order. */
    struct DrawRun {
        std::size_t page{} /**< Atlas page. */, first_vertex{} /**< First positioned vertex. */, vertex_count{}; ///< Vertex range length.
    };
    /** Uploaded immutable glyph geometry and grayscale atlas in one backend domain. */
    struct PageResources {
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry; ///< Retained six-vertex-per-glyph triangle-list geometry.
        std::shared_ptr<CE::Rendering::Resources::Image> atlas; ///< Retained single-page font atlas.
    };
    class Generation;

    /** Owned CPU layout, grayscale pages and geometry. Only distinct visible glyphs
     * used by this message are admitted; spaces/empty outlines keep their advances
     * without quads. Optional positioned geometry repeats quads in authored order
     * for run submission. No provider is retained or invoked during preparation.
     */
    class Prepared {
        ShapedText layout_;
        std::vector<PreparedPage> pages_;
        std::vector<GlyphDraw> draws_;
        std::vector<CE::Rendering::Geometry::Vertex2D> positioned_vertices_;
        std::vector<DrawRun> runs_;
        bool positioned_runs_ = false;

        CE_API explicit Prepared(ShapedText layout);
        friend CE_API Prepared prepare(ShapedText, RasterOptions);
        friend CE_API Generation
        upload(Prepared, CE::Rendering::Resources::Provider&, std::shared_ptr<const CE::Rendering::Resources::Material>);

    public:
        [[nodiscard]] const ShapedText& layout() const noexcept { return layout_; }
        [[nodiscard]] std::span<const PreparedPage> pages() const noexcept { return pages_; }
        [[nodiscard]] std::span<const GlyphDraw> draws() const noexcept { return draws_; }
        [[nodiscard]] bool has_positioned_runs() const noexcept { return positioned_runs_; }
        [[nodiscard]] std::span<const CE::Rendering::Geometry::Vertex2D> positioned_vertices() const noexcept {
            return positioned_vertices_;
        }
        [[nodiscard]] std::span<const DrawRun> runs() const noexcept { return runs_; }
    };

    /** One complete immutable geometry/atlas generation paired with its placements.
     * CPU submission copies selected handles into packets; replacing this value never
     * mutates resources retained by earlier frames. Native use follows provider lifetime.
     */
    class Generation {
        ShapedText layout_;
        std::vector<PageResources> pages_;
        std::vector<GlyphDraw> draws_;
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D> positioned_geometry_;
        std::vector<DrawRun> runs_;
        bool positioned_runs_ = false;
        std::shared_ptr<const CE::Rendering::Resources::Material> material_;

        CE_API Generation(
            ShapedText layout,
            std::vector<PageResources> pages,
            std::vector<GlyphDraw> draws,
            std::shared_ptr<CE::Rendering::Geometry::Buffer2D> positioned_geometry,
            std::vector<DrawRun> runs,
            bool positioned_runs,
            std::shared_ptr<const CE::Rendering::Resources::Material> material
        );
        friend CE_API Generation
        upload(Prepared, CE::Rendering::Resources::Provider&, std::shared_ptr<const CE::Rendering::Resources::Material>);

    public:
        [[nodiscard]] const ShapedText& layout() const noexcept { return layout_; }
        [[nodiscard]] std::span<const PageResources> pages() const noexcept { return pages_; }
        [[nodiscard]] std::span<const GlyphDraw> draws() const noexcept { return draws_; }
        [[nodiscard]] bool has_positioned_runs() const noexcept { return positioned_runs_; }
        [[nodiscard]] const std::shared_ptr<CE::Rendering::Geometry::Buffer2D>& positioned_geometry() const noexcept {
            return positioned_geometry_;
        }
        [[nodiscard]] std::span<const DrawRun> runs() const noexcept { return runs_; }
        [[nodiscard]] const std::shared_ptr<const CE::Rendering::Resources::Material>& material() const noexcept { return material_; }
    };

    [[nodiscard]] CE_API Prepared prepare(ShapedText layout, RasterOptions options = {});
    [[nodiscard]] CE_API Generation upload(
        Prepared prepared,
        CE::Rendering::Resources::Provider& provider,
        std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
    );
}

/** @fn const ShapedText& CE::Text::Rendering::Prepared::layout() const noexcept
 * Borrow the retained immutable shaping result. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn std::span<const PreparedPage> CE::Text::Rendering::Prepared::pages() const noexcept
 * Borrow owned CPU alpha pages and glyph vertices. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn std::span<const GlyphDraw> CE::Text::Rendering::Prepared::draws() const noexcept
 * Borrow visible glyph placements in authored order. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn bool CE::Text::Rendering::Prepared::has_positioned_runs() const noexcept
 * Return whether positioned-run preparation was requested, even if no visible glyphs exist.
 */

/** @fn std::span<const CE::Rendering::Geometry::Vertex2D> CE::Text::Rendering::Prepared::positioned_vertices() const noexcept
 * Borrow optional positioned whole-text vertices; empty without runs or visible glyphs. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn std::span<const DrawRun> CE::Text::Rendering::Prepared::runs() const noexcept
 * Borrow ordered positioned-geometry ranges by atlas page. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn const ShapedText& CE::Text::Rendering::Generation::layout() const noexcept
 * Borrow the shaping result retained with these uploaded resources. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn std::span<const PageResources> CE::Text::Rendering::Generation::pages() const noexcept
 * Borrow immutable page resources; copy their handles for independent retention. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn std::span<const GlyphDraw> CE::Text::Rendering::Generation::draws() const noexcept
 * Borrow visible glyph placements matching the uploaded page indices. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn bool CE::Text::Rendering::Generation::has_positioned_runs() const noexcept
 * Return whether positioned-run geometry was requested.
 */

/** @fn const std::shared_ptr<CE::Rendering::Geometry::Buffer2D>& CE::Text::Rendering::Generation::positioned_geometry() const noexcept
 * Borrow optional retained positioned geometry; copy the handle for independent retention. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn std::span<const DrawRun> CE::Text::Rendering::Generation::runs() const noexcept
 * Borrow authored positioned draw ranges sharing page atlases. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn const std::shared_ptr<const CE::Rendering::Resources::Material>& CE::Text::Rendering::Generation::material() const noexcept
 * Borrow the selected optional material; an empty no-draw generation may have none. Keep this value alive and unmoved while using a returned view/reference.
 */

/** @fn CE::Text::Rendering::Prepared CE::Text::Rendering::prepare(ShapedText layout, RasterOptions options)
 * Consume/copy an owned shaping result and rasterize only its used visible grayscale outline glyphs on CPU. Spaces/empty outlines keep advances without quads. Page extent outside4..4096, oversized/unsupported glyphs and invalid references reject; rasterization/allocation failures propagate. No provider is retained, so this may run on a worker.
 * @return Complete immutable CPU preparation ready for upload().
 * @see [Preparation and upload](cheryl-guide:assets/text-layout#preparation-upload-and-retained-submission)
 */

/** @fn CE::Text::Rendering::Generation CE::Text::Rendering::upload(Prepared prepared, CE::Rendering::Resources::Provider& provider, std::shared_ptr<const CE::Rendering::Resources::Material> material)
 * Upload one complete retained generation on the provider owner/current context. Empty material requests the provider default for nonempty draws. Copy prepared for retry or move it into a platform submission. Invalid/mismatched resources and provider/allocation failures throw without returning a generation; earlier transient native uploads retire normally, not via atomic rollback. Existing generations remain unchanged.
 */
