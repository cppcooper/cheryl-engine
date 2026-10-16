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
    struct RecordedDraw {
        std::vector<CE::Rendering::Geometry::Vertex2DColor> vertices;
        std::shared_ptr<const CE::Assets::Images::Decoded> texture;
        CE::Rendering::ClipRegion2D clip;
    };

    // Contains no toolkit objects, handles or borrowed vertex/pixel storage.
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
