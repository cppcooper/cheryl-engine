#pragma once

#include <text/layout.h>
#include <assets/resources/geometry2d.h>
#include <assets/resources/image.h>
#include <assets/types/primitives/vertex.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace CE::Assets {
    struct ResourceProvider;
    struct TextRasterOptions {
        std::uint32_t maximum_page_extent = 1024; // 4..4096; pages crop to their used padded bounds.
    };
    struct PreparedTextPage {
        PixelSize size;
        std::vector<unsigned char> alpha;
        std::vector<Vertex2D> vertices;
    };
    struct TextGlyphDraw {
        std::size_t page{}, index{}; // Six-vertex quad within this generation's page.
        float x{}, y{};
    };
    struct TextPageResources {
        std::shared_ptr<Geometry2D> geometry;
        std::shared_ptr<Image> atlas;
    };
    class RenderedText;

    /** Owned CPU layout, grayscale pages and geometry. Only distinct visible glyphs
     * used by this message are admitted; spaces/empty outlines keep their advances
     * without quads. No provider is retained or invoked during preparation.
     */
    class PreparedText {
        Text::ShapedText layout_;
        std::vector<PreparedTextPage> pages_;
        std::vector<TextGlyphDraw> draws_;

        explicit PreparedText(Text::ShapedText layout);
        friend PreparedText prepare_text(Text::ShapedText, TextRasterOptions);
        friend RenderedText upload_text(PreparedText, ResourceProvider&);

    public:
        [[nodiscard]] const Text::ShapedText& layout() const noexcept { return layout_; }
        [[nodiscard]] std::span<const PreparedTextPage> pages() const noexcept { return pages_; }
        [[nodiscard]] std::span<const TextGlyphDraw> draws() const noexcept { return draws_; }
    };

    /** One complete immutable geometry/atlas generation paired with its placements.
     * CPU submission copies selected handles into packets; replacing this value never
     * mutates resources retained by earlier frames. Native use follows provider lifetime.
     */
    class RenderedText {
        Text::ShapedText layout_;
        std::vector<TextPageResources> pages_;
        std::vector<TextGlyphDraw> draws_;

        RenderedText(Text::ShapedText layout, std::vector<TextPageResources> pages, std::vector<TextGlyphDraw> draws);
        friend RenderedText upload_text(PreparedText, ResourceProvider&);

    public:
        [[nodiscard]] const Text::ShapedText& layout() const noexcept { return layout_; }
        [[nodiscard]] std::span<const TextPageResources> pages() const noexcept { return pages_; }
        [[nodiscard]] std::span<const TextGlyphDraw> draws() const noexcept { return draws_; }
    };

    [[nodiscard]] PreparedText prepare_text(Text::ShapedText layout, TextRasterOptions options = {});
    // Pass a copy for retry or move-owned preparation into a dispatcher request.
    // Upload obeys provider affinity. Failure returns no generation; earlier successful
    // transient uploads retire through their backend rather than an atomic native rollback.
    [[nodiscard]] RenderedText upload_text(PreparedText prepared, ResourceProvider& provider);
}
