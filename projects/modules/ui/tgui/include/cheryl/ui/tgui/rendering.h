#pragma once

#include <cheryl/export.h>

#include <assets/resources/decoded-image.h>
#include <assets/types/primitives/vertex.h>
#include <core/rendering/clip-region.h>

#include <TGUI/Backend/Renderer/BackendRenderer.hpp>
#include <TGUI/Backend/Renderer/BackendRenderTarget.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace CE::UI::TGUI {
    // CPU-only toolkit objects on the simulation/UI owner. Recordings copy
    // smoothing independently of immutable pixel snapshots for platform upload.
    /** UI-owned CPU texture bridge; loaded pixels are snapshotted and immutable for completed recordings. */
    class CE_TYPE_API Texture final : public tgui::BackendTexture {
        const unsigned int maximum_size_;
        std::shared_ptr<const CE::Assets::Images::Decoded> snapshot_;

    public:
        CE_UI_TGUI_API explicit Texture(unsigned int maximum_size);
        CE_UI_TGUI_API bool loadTextureOnly(tgui::Vector2u size, const std::uint8_t* pixels, bool smooth) override;
        CE_UI_TGUI_API void setSmooth(bool smooth) override;
        [[nodiscard]] std::shared_ptr<const CE::Assets::Images::Decoded> snapshot() const { return snapshot_; }
    };

    /** CPU texture factory for the pinned TGUI backend; performs no graphics uploads. */
    class CE_TYPE_API Renderer final : public tgui::BackendRenderer {
        const unsigned int maximum_size_;

    public:
        CE_UI_TGUI_API explicit Renderer(unsigned int maximum_size);
        [[nodiscard]] CE_UI_TGUI_API std::shared_ptr<tgui::BackendTexture> createTexture() override;
        [[nodiscard]] unsigned int getMaximumTextureSize() override { return maximum_size_; }
    };

    /** Owned expanded colored triangles and retained immutable CPU pixels for one clipped draw. */
    struct RecordedDraw {
        std::vector<CE::Rendering::Geometry::Vertex2DColor> vertices; ///< Owned triangle-list vertices in logical UI coordinates, with color/UV values.
        std::shared_ptr<const CE::Assets::Images::Decoded> texture; ///< Optional retained top-to-bottom RGBA pixels; absent for solid draws.
        CE::Rendering::ClipRegion2D clip; ///< Logical rectangular clip plus the logical reference extent.
        bool smooth = true; ///< Copied filtering policy independent of the retained pixel snapshot.
    };

    // No toolkit objects or borrowed vertex/pixel data cross the upload boundary.
    /** Complete CPU recording independent of toolkit objects and source vertex/pixel storage. Copies may cross to platform for upload.
     * @see [UI recording](cheryl-guide:modules/tgui#recording-and-publication) */
    class RecordedScene final {
        friend class RenderTarget;
        float width_ = 0;
        float height_ = 0;
        std::vector<RecordedDraw> draws_;

    public:
        [[nodiscard]] float width() const { return width_; }
        [[nodiscard]] float height() const { return height_; }
        [[nodiscard]] std::span<const RecordedDraw> draws() const { return draws_; }
    };

    /** UI-owner CPU triangle/rectangular-clip recorder. Configure logical view and pixel rounding before recording; unsupported native clearing rejects. */
    class CE_TYPE_API RenderTarget final : public tgui::BackendRenderTarget {
        tgui::Vector2f pixel_scale_{1, 1};
        CE::Rendering::ClipRect2D clip_rectangle_;
        std::vector<RecordedDraw> draws_;
        bool configured_ = false;
        bool recording_ = false;

    public:
        // targetSize and viewport are logical window coordinates. pixel_scale
        // supplies copied framebuffer/logical ratios for toolkit pixel rounding.
        CE_UI_TGUI_API void setView(tgui::FloatRect view, tgui::FloatRect viewport, tgui::Vector2f targetSize) override;
        CE_UI_TGUI_API void set_pixel_scale(tgui::Vector2f scale);
        CE_UI_TGUI_API void begin_recording();
        [[nodiscard]] CE_UI_TGUI_API RecordedScene finish_recording();
        CE_UI_TGUI_API void discard_recording() noexcept;
        // Starts a recording, invokes toolkit drawing, and discards it on error.
        // finish_recording() transfers a successful result to its CPU owner.
        CE_UI_TGUI_API void drawGui(const std::shared_ptr<tgui::RootContainer>& root) override;
        CE_UI_TGUI_API void addClippingLayer(const tgui::RenderStates& states, tgui::FloatRect rect) override;
        CE_UI_TGUI_API void removeClippingLayer() override;
        // Null indices select a triangle list in vertex order, used for glyphs.
        // Otherwise expand the indexed triangles into the same owned format.
        CE_UI_TGUI_API void drawVertexArray(
            const tgui::RenderStates& states,
            const tgui::Vertex* vertices,
            std::size_t vertexCount,
            const unsigned int* indices,
            std::size_t indexCount,
            const std::shared_ptr<tgui::BackendTexture>& texture
        ) override;
        // The application owns platform clearing; toolkit mainLoop is not used.
        CE_UI_TGUI_API void setClearColor(const tgui::Color& color) override;
        CE_UI_TGUI_API void clearScreen() override;

    private:
        CE_UI_TGUI_API void updateClipping(tgui::FloatRect clipRect, tgui::FloatRect clipViewport) override;
        CE_UI_TGUI_API void require_recording() const;
        [[nodiscard]] CE_UI_TGUI_API bool drawable() const;
    };
}

/** @fn float CE::UI::TGUI::RecordedScene::width() const
 * Return copied logical recording width; zero for a default empty recording.
 */

/** @fn float CE::UI::TGUI::RecordedScene::height() const
 * Return copied logical recording height; zero for a default empty recording.
 */

/** @fn std::span<const CE::UI::TGUI::RecordedDraw> CE::UI::TGUI::RecordedScene::draws() const
 * Borrow ordered draws; retain this recording alive and unmoved while using the view. Draw copies own vertices and share immutable pixel storage.
 */

/** @fn CE::UI::TGUI::Texture::Texture(unsigned int maximum_size)
 * Require a positive maximum dimension representable by int; invalid bounds throw invalid_args.
 */

/** @fn bool CE::UI::TGUI::Texture::loadTextureOnly(tgui::Vector2u size, const std::uint8_t* pixels, bool smooth)
 * Copy transient top-to-bottom straight-alpha RGBA pixels before returning true. Require nonnull pixels, positive dimensions within the configured bound and representable byte count; invalid input/allocation failures throw. Completed recordings retain earlier snapshots independently.
 */

/** @fn void CE::UI::TGUI::Texture::setSmooth(bool smooth)
 * Change the toolkit filtering preference without mutating the immutable pixel snapshot; later recordings copy the new value.
 */

/** @fn std::shared_ptr<const CE::Assets::Images::Decoded> CE::UI::TGUI::Texture::snapshot() const
 * Return a retained immutable CPU image, null before loading; survives this native Texture.
 */

/** @fn CE::UI::TGUI::Renderer::Renderer(unsigned int maximum_size)
 * Configure a positive representable texture bound for subsequently created CPU textures; invalid arguments throw.
 */

/** @fn std::shared_ptr<tgui::BackendTexture> CE::UI::TGUI::Renderer::createTexture()
 * Allocate a new unloaded Cheryl Texture with this renderer's bound. No GPU resources are created.
 */

/** @fn unsigned int CE::UI::TGUI::Renderer::getMaximumTextureSize()
 * Return the configured CPU dimension limit, not a native GPU capability query.
 */

/** @fn void CE::UI::TGUI::RenderTarget::setView(tgui::FloatRect view, tgui::FloatRect viewport, tgui::Vector2f targetSize)
 * Copy logical view, viewport and target dimensions outside recording. Require finite nonnegative dimensions and ordered rectangles; invalid/unrepresentable view-to-pixel scale rejects. Zero extents suppress draws.
 */

/** @fn void CE::UI::TGUI::RenderTarget::set_pixel_scale(tgui::Vector2f scale)
 * Set positive finite copied framebuffer/logical ratios for toolkit pixel rounding outside recording. Does not read a window or change logical coordinates.
 */

/** @fn void CE::UI::TGUI::RenderTarget::begin_recording()
 * Start a fresh recording with full viewport clipping. Requires configured view and no active recording; clears previous staged draws.
 */

/** @fn CE::UI::TGUI::RecordedScene CE::UI::TGUI::RenderTarget::finish_recording()
 * Transfer a complete owned recording and stop recording. Requires active recording and a balanced clip stack; otherwise throws. Copy/retain the result for independent upload.
 */

/** @fn void CE::UI::TGUI::RenderTarget::discard_recording() noexcept
 * Discard staged draws and clip layers, restore viewport clip and leave recording inactive. Completed recordings are unaffected.
 */

/** @fn void CE::UI::TGUI::RenderTarget::drawGui(const std::shared_ptr<tgui::RootContainer>& root)
 * Begin recording and invoke the nonnull native root's drawing when drawable. An exception or unbalanced clip stack discards incomplete work and propagates; finish_recording() transfers successful output.
 */

/** @fn void CE::UI::TGUI::RenderTarget::addClippingLayer(const tgui::RenderStates& states, tgui::FloatRect rect)
 * Intersect a validated logical rectangle during recording. Finite transforms with supported axis-aligned/quarter-turn clipping only; unsupported rotation rejects. Failure preserves the preceding clip stack/region.
 */

/** @fn void CE::UI::TGUI::RenderTarget::removeClippingLayer()
 * Pop a clip layer during recording; missing recording or an empty stack throws failed_operation.
 */

/** @fn void CE::UI::TGUI::RenderTarget::drawVertexArray(const tgui::RenderStates& states, const tgui::Vertex* vertices, std::size_t vertexCount, const unsigned int* indices, std::size_t indexCount, const std::shared_ptr<tgui::BackendTexture>& texture)
 * During recording, expand indexed triangles, or a null-index vertex-order triangle list, into owned colored vertices. Empty draws are ignored. Require complete triangles, valid bounds/finite positions and a loaded Cheryl texture when textured. Copy input storage before return, flip texture V for atlas orientation and retain CPU pixels/filtering. Invalid input/allocation failure throws; no GPU work occurs.
 */

/** @fn void CE::UI::TGUI::RenderTarget::setClearColor(const tgui::Color& color)
 * Always throw failed_operation: framebuffer clear state belongs to the application/platform renderer, outside CPU toolkit recording.
 */

/** @fn void CE::UI::TGUI::RenderTarget::clearScreen()
 * Always throw failed_operation: the application clears its framebuffer; this bridge only records toolkit draws.
 */
