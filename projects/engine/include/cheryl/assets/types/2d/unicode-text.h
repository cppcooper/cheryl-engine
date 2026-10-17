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
}

namespace CE::Text::Rendering {
    struct RasterOptions {
        std::uint32_t maximum_page_extent = 1024; // 4..4096; pages crop to their used padded bounds.
        bool positioned_runs = false; // Additional geometry for explicit whole-text transform submission.
    };
    struct PreparedPage {
        CE::Rendering::Geometry::PixelSize size;
        std::vector<unsigned char> alpha;
        std::vector<CE::Rendering::Geometry::Vertex2D> vertices;
    };
    struct GlyphDraw {
        std::size_t page{}, index{}; // Six-vertex quad within this generation's page.
        float x{}, y{};
    };
    struct DrawRun {
        std::size_t page{}, first_vertex{}, vertex_count{}; // Range in positioned geometry; page selects its atlas.
    };
    struct PageResources {
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D> geometry;
        std::shared_ptr<CE::Rendering::Resources::Image> atlas;
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
        friend CE_API Generation upload(Prepared, CE::Rendering::Resources::Provider&);

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

        CE_API Generation(
            ShapedText layout,
            std::vector<PageResources> pages,
            std::vector<GlyphDraw> draws,
            std::shared_ptr<CE::Rendering::Geometry::Buffer2D> positioned_geometry,
            std::vector<DrawRun> runs,
            bool positioned_runs
        );
        friend CE_API Generation upload(Prepared, CE::Rendering::Resources::Provider&);

    public:
        [[nodiscard]] const ShapedText& layout() const noexcept { return layout_; }
        [[nodiscard]] std::span<const PageResources> pages() const noexcept { return pages_; }
        [[nodiscard]] std::span<const GlyphDraw> draws() const noexcept { return draws_; }
        [[nodiscard]] bool has_positioned_runs() const noexcept { return positioned_runs_; }
        [[nodiscard]] const std::shared_ptr<CE::Rendering::Geometry::Buffer2D>& positioned_geometry() const noexcept {
            return positioned_geometry_;
        }
        [[nodiscard]] std::span<const DrawRun> runs() const noexcept { return runs_; }
    };

    /** Rasterize a shaped message into owned CPU atlas pages and geometry.
     * No provider or graphics context is used, so preparation can run on a worker.
     * Page extents outside 4..4096, unsupported glyphs or glyphs exceeding padded
     * page bounds reject preparation; rasterization and allocation failures propagate.
     * @param layout Owned layout consumed by value, retaining the selected font snapshot.
     * @param options Maximum atlas-page extent and optional positioned-run geometry.
     * @return Complete CPU preparation ready for upload_text(), with no native resources.
     */
    [[nodiscard]] CE_API Prepared prepare(ShapedText layout, RasterOptions options = {});
    /** Upload one immutable text generation on the provider's owner/current context.
     * Failure returns no generation; earlier successful transient uploads retire
     * through their backend rather than an atomic native rollback. Mismatched
     * provider resources are rejected; provider and allocation failures propagate.
     * @param prepared Owned CPU preparation consumed by value; pass a copy for retry
     * or move it into an owner-dispatch request.
     * @param provider Backend receiving the atlas and geometry uploads, borrowed for this call.
     * @return Owned layout and resource handles; native use retains the provider-domain limits.
     */
    [[nodiscard]] CE_API Generation upload(Prepared prepared, CE::Rendering::Resources::Provider& provider);
}
