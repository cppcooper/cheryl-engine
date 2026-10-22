#pragma once

#include <cheryl/export.h>

#include <assets/resources/decoded-image.h>
#include <assets/types/primitives/vertex.h>
#include <core/rendering/clip-region.h>

#include <RmlUi/Core/RenderInterface.h>

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace CE::UI::RmlUi {
    /** Owned expanded colored triangles and retained immutable CPU pixels for one clipped draw. */
    struct RecordedDraw {
        std::vector<CE::Rendering::Geometry::Vertex2DColor> vertices; ///< Owned triangle-list vertices in logical UI coordinates, with color/UV values.
        std::shared_ptr<const CE::Assets::Images::Decoded> texture; ///< Optional retained top-to-bottom RGBA pixels; absent for solid draws.
        CE::Rendering::ClipRegion2D clip; ///< Logical rectangular clip plus the logical reference extent.
    };

    // Contains no toolkit objects, handles or borrowed vertex/pixel storage.
    /** Complete CPU recording independent of toolkit objects and source vertex/pixel storage. Copies may cross to platform for upload.
     * @see [UI recording](cheryl-guide:modules/rmlui#recording-and-publication) */
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

    // CPU-only RmlUi renderer. Its handles belong to the UI owner; only completed
    // recordings cross to the platform. Generated textures are premultiplied;
    // file images are premultiplied once. Provider sampling is smooth/clamped.
    /** RmlUi6.3 CPU geometry/texture bridge on the UI owner. Handles belong to this recorder; completed recordings own their copies. File images are premultiplied once; generated textures already use premultiplied RGBA. Unsupported effects are reported at recording boundaries without interrupting toolkit cleanup. */
    class CE_TYPE_API RenderTarget final : public Rml::RenderInterface {
        const unsigned int maximum_texture_size_;
        Rml::Vector2i size_;
        CE::Rendering::ClipRect2D scissor_;
        std::unordered_map<Rml::CompiledGeometryHandle, std::vector<CE::Rendering::Geometry::Vertex2DColor>> geometry_;
        std::unordered_map<Rml::TextureHandle, std::shared_ptr<const CE::Assets::Images::Decoded>> textures_;
        std::vector<RecordedDraw> draws_;
        std::uintptr_t next_geometry_ = 1;
        std::uintptr_t next_texture_ = 1;
        const char* unsupported_ = nullptr;
        bool configured_ = false;
        bool recording_ = false;
        bool scissoring_ = false;

    public:
        CE_UI_RMLUI_API explicit RenderTarget(unsigned int maximum_texture_size = 4096);
        CE_UI_RMLUI_API void set_view(Rml::Vector2i logical_size);
        CE_UI_RMLUI_API void begin_recording();
        [[nodiscard]] CE_UI_RMLUI_API RecordedScene finish_recording();
        CE_UI_RMLUI_API void discard_recording() noexcept;

        CE_UI_RMLUI_API Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
        CE_UI_RMLUI_API void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
        CE_UI_RMLUI_API void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
        CE_UI_RMLUI_API Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override;
        CE_UI_RMLUI_API Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> pixels, Rml::Vector2i dimensions) override;
        CE_UI_RMLUI_API void ReleaseTexture(Rml::TextureHandle texture) override;
        CE_UI_RMLUI_API void EnableScissorRegion(bool enable) override;
        CE_UI_RMLUI_API void SetScissorRegion(Rml::Rectanglei region) override;

        // Optional SDK callbacks return their failure handles without interrupting
        // RmlUi's render-stack cleanup. Publication reports the unsupported feature.
        CE_UI_RMLUI_API void EnableClipMask(bool enable) override;
        CE_UI_RMLUI_API void RenderToClipMask(Rml::ClipMaskOperation, Rml::CompiledGeometryHandle, Rml::Vector2f) override;
        CE_UI_RMLUI_API void SetTransform(const Rml::Matrix4f* transform) override;
        CE_UI_RMLUI_API Rml::LayerHandle PushLayer() override;
        CE_UI_RMLUI_API void CompositeLayers(Rml::LayerHandle, Rml::LayerHandle, Rml::BlendMode, Rml::Span<const Rml::CompiledFilterHandle>) override;
        CE_UI_RMLUI_API void PopLayer() override;
        CE_UI_RMLUI_API Rml::TextureHandle SaveLayerAsTexture() override;
        CE_UI_RMLUI_API Rml::CompiledFilterHandle SaveLayerAsMaskImage() override;
        CE_UI_RMLUI_API Rml::CompiledFilterHandle CompileFilter(const Rml::String&, const Rml::Dictionary&) override;
        CE_UI_RMLUI_API void ReleaseFilter(Rml::CompiledFilterHandle) override;
        CE_UI_RMLUI_API Rml::CompiledShaderHandle CompileShader(const Rml::String&, const Rml::Dictionary&) override;
        CE_UI_RMLUI_API void RenderShader(Rml::CompiledShaderHandle, Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override;
        CE_UI_RMLUI_API void ReleaseShader(Rml::CompiledShaderHandle) override;

    private:
        [[nodiscard]] CE_UI_RMLUI_API std::size_t texture_bytes(Rml::Vector2i dimensions) const;
        CE_UI_RMLUI_API Rml::TextureHandle store_texture(CE::Assets::Images::Decoded image);
        CE_UI_RMLUI_API void reject(const char* feature) noexcept;
    };
}

/** @fn float CE::UI::RmlUi::RecordedScene::width() const
 * Return copied logical recording width; zero for a default empty recording.
 */

/** @fn float CE::UI::RmlUi::RecordedScene::height() const
 * Return copied logical recording height; zero for a default empty recording.
 */

/** @fn std::span<const CE::UI::RmlUi::RecordedDraw> CE::UI::RmlUi::RecordedScene::draws() const
 * Borrow ordered draws; retain this recording alive and unmoved while using the view. Draw copies own vertices and share immutable pixel storage.
 */

/** @fn CE::UI::RmlUi::RenderTarget::RenderTarget(unsigned int maximum_texture_size)
 * Require RmlUi6.3 and a positive int-representable texture bound; version mismatch or invalid bound throws. Session adds the font atlas's minimum 1024 bound.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::set_view(Rml::Vector2i logical_size)
 * Copy nonnegative logical dimensions outside an active recording; zero extents produce no visible draws.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::begin_recording()
 * Clear staged draws and reset scissoring for the configured view. Reject missing view, an active recording or a latched unsupported feature.
 */

/** @fn CE::UI::RmlUi::RecordedScene CE::UI::RmlUi::RenderTarget::finish_recording()
 * Transfer complete owned staged draws and stop recording. Requires an active recording; a latched unsupported feature discards staged work and throws failed_operation.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::discard_recording() noexcept
 * Discard staged draws and stop recording without clearing a latched unsupported-feature error. Earlier completed recordings remain independent.
 */

/** @fn Rml::CompiledGeometryHandle CE::UI::RmlUi::RenderTarget::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
 * Copy/expand indexed triangles and flip texture V into owned colored vertices. Empty indices return 0. Invalid indices, incomplete triangles, nonfinite coordinates, handle exhaustion and allocation failures throw. Returned handles are local to this RenderTarget until release.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
 * Append a translated copy of live compiled geometry during recording, with current rectangular clip and retained CPU texture. Texture 0 means solid. Foreign/released handles or invalid translation reject; empty clip emits nothing. Repeating UVs latch an unsupported-sampler error for publication.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
 * Erase a local compiled geometry handle; unknown handles are ignored. Completed draw copies remain valid.
 */

/** @fn Rml::TextureHandle CE::UI::RmlUi::RenderTarget::LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source)
 * Decode an image from a UTF-8 path and premultiply RGB once before retaining CPU pixels. Set output dimensions on success; decode/unavailable/oversized images return 0 and zero dimensions. Inconsistent metadata or storage/handle failures can throw.
 */

/** @fn Rml::TextureHandle CE::UI::RmlUi::RenderTarget::GenerateTexture(Rml::Span<const Rml::byte> pixels, Rml::Vector2i dimensions)
 * Copy already premultiplied RGBA pixels. Require positive bounded dimensions and exactly width*height*4 bytes; invalid input, exhausted handles or allocation failures throw. Caller storage is not retained.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::ReleaseTexture(Rml::TextureHandle texture)
 * Erase local texture ownership; unknown handles are ignored. Completed draws retain their immutable CPU image.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::EnableScissorRegion(bool enable)
 * Select whether subsequent draws use the copied rectangular scissor, intersected with the logical viewport.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::SetScissorRegion(Rml::Rectanglei region)
 * Copy ordered logical scissor edges; inverted rectangles reject. Empty rectangles suppress subsequent clipped draws.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::EnableClipMask(bool enable)
 * Disabling is accepted; enabling latches unsupported clip-mask failure for the next recording boundary.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation)
 * Latch unsupported clip-mask failure without throwing through the toolkit render-stack cleanup path.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::SetTransform(const Rml::Matrix4f* transform)
 * Accept null or identity transforms. Nonidentity transforms latch an unsupported transform/clipping-contract failure; no native matrix pointer is retained.
 */

/** @fn Rml::LayerHandle CE::UI::RmlUi::RenderTarget::PushLayer()
 * Latch unsupported offscreen-layer failure and return 0, permitting toolkit cleanup to finish.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blend, Rml::Span<const Rml::CompiledFilterHandle> filters)
 * Latch unsupported offscreen-layer failure; no compositing or borrowed filter retention occurs.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::PopLayer()
 * Latch unsupported offscreen-layer failure without throwing during stack cleanup.
 */

/** @fn Rml::TextureHandle CE::UI::RmlUi::RenderTarget::SaveLayerAsTexture()
 * Latch unsupported offscreen-texture failure and return 0.
 */

/** @fn Rml::CompiledFilterHandle CE::UI::RmlUi::RenderTarget::SaveLayerAsMaskImage()
 * Latch unsupported mask-image failure and return 0.
 */

/** @fn Rml::CompiledFilterHandle CE::UI::RmlUi::RenderTarget::CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters)
 * Latch unsupported filter failure and return 0; no parameter data is retained.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::ReleaseFilter(Rml::CompiledFilterHandle filter)
 * No-op cleanup for unsupported filters; no filter handles are created by this bridge.
 */

/** @fn Rml::CompiledShaderHandle CE::UI::RmlUi::RenderTarget::CompileShader(const Rml::String& name, const Rml::Dictionary& parameters)
 * Latch unsupported custom-shader failure and return 0; no shader parameters are retained.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
 * Latch unsupported custom-shader failure without interrupting toolkit cleanup.
 */

/** @fn void CE::UI::RmlUi::RenderTarget::ReleaseShader(Rml::CompiledShaderHandle shader)
 * No-op cleanup for unsupported shaders; no shader handles are created by this bridge.
 */
