#pragma once

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
        std::vector<Vertex2DColor> vertices;
        std::shared_ptr<const Assets::DecodedImage> texture;
        RenderAPIs::ClipRegion2D clip;
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
    class RenderTarget final : public Rml::RenderInterface {
        const unsigned int maximum_texture_size_;
        Rml::Vector2i size_;
        RenderAPIs::ClipRect2D scissor_;
        std::unordered_map<Rml::CompiledGeometryHandle, std::vector<Vertex2DColor>> geometry_;
        std::unordered_map<Rml::TextureHandle, std::shared_ptr<const Assets::DecodedImage>> textures_;
        std::vector<RecordedDraw> draws_;
        std::uintptr_t next_geometry_ = 1;
        std::uintptr_t next_texture_ = 1;
        const char* unsupported_ = nullptr;
        bool configured_ = false;
        bool recording_ = false;
        bool scissoring_ = false;

    public:
        explicit RenderTarget(unsigned int maximum_texture_size = 4096);
        void set_view(Rml::Vector2i logical_size);
        void begin_recording();
        [[nodiscard]] RecordedScene finish_recording();
        void discard_recording() noexcept;

        Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
        void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
        void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
        Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override;
        Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> pixels, Rml::Vector2i dimensions) override;
        void ReleaseTexture(Rml::TextureHandle texture) override;
        void EnableScissorRegion(bool enable) override;
        void SetScissorRegion(Rml::Rectanglei region) override;

        // Optional SDK callbacks return their failure handles without interrupting
        // RmlUi's render-stack cleanup. Publication reports the unsupported feature.
        void EnableClipMask(bool enable) override;
        void RenderToClipMask(Rml::ClipMaskOperation, Rml::CompiledGeometryHandle, Rml::Vector2f) override;
        void SetTransform(const Rml::Matrix4f* transform) override;
        Rml::LayerHandle PushLayer() override;
        void CompositeLayers(Rml::LayerHandle, Rml::LayerHandle, Rml::BlendMode, Rml::Span<const Rml::CompiledFilterHandle>) override;
        void PopLayer() override;
        Rml::TextureHandle SaveLayerAsTexture() override;
        Rml::CompiledFilterHandle SaveLayerAsMaskImage() override;
        Rml::CompiledFilterHandle CompileFilter(const Rml::String&, const Rml::Dictionary&) override;
        void ReleaseFilter(Rml::CompiledFilterHandle) override;
        Rml::CompiledShaderHandle CompileShader(const Rml::String&, const Rml::Dictionary&) override;
        void RenderShader(Rml::CompiledShaderHandle, Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override;
        void ReleaseShader(Rml::CompiledShaderHandle) override;

    private:
        [[nodiscard]] std::size_t texture_bytes(Rml::Vector2i dimensions) const;
        Rml::TextureHandle store_texture(Assets::DecodedImage image);
        void reject(const char* feature) noexcept;
    };
}
