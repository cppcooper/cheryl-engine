#pragma once

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
    // CPU-only toolkit objects. Use these on the simulation/UI owner. The
    // application supplies a supported bound and a provider whose default RGBA
    // sampling is smoothed and clamp-to-edge. Nearest sampling is rejected.
    class Texture final : public tgui::BackendTexture {
        const unsigned int maximum_size_;
        std::shared_ptr<const Assets::DecodedImage> snapshot_;

    public:
        explicit Texture(unsigned int maximum_size);
        bool loadTextureOnly(tgui::Vector2u size, const std::uint8_t* pixels, bool smooth) override;
        void setSmooth(bool smooth) override;
        [[nodiscard]] std::shared_ptr<const Assets::DecodedImage> snapshot() const { return snapshot_; }
    };

    class Renderer final : public tgui::BackendRenderer {
        const unsigned int maximum_size_;

    public:
        explicit Renderer(unsigned int maximum_size);
        [[nodiscard]] std::shared_ptr<tgui::BackendTexture> createTexture() override;
        [[nodiscard]] unsigned int getMaximumTextureSize() override { return maximum_size_; }
    };

    struct RecordedDraw {
        std::vector<Vertex2DColor> vertices;
        std::shared_ptr<const Assets::DecodedImage> texture;
        RenderAPIs::ClipRegion2D clip;
    };

    // No toolkit objects or borrowed vertex/pixel data cross the upload boundary.
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

    class RenderTarget final : public tgui::BackendRenderTarget {
        tgui::Vector2f pixel_scale_{1, 1};
        tgui::FloatRect clip_viewport_;
        std::vector<RecordedDraw> draws_;
        bool configured_ = false;
        bool recording_ = false;

    public:
        // targetSize and viewport are logical window coordinates. pixel_scale
        // supplies copied framebuffer/logical ratios for toolkit pixel rounding.
        void setView(tgui::FloatRect view, tgui::FloatRect viewport, tgui::Vector2f targetSize) override;
        void set_pixel_scale(tgui::Vector2f scale);
        void begin_recording();
        [[nodiscard]] RecordedScene finish_recording();
        void discard_recording() noexcept;
        // Starts a recording, invokes toolkit drawing, and discards it on error.
        // finish_recording() transfers a successful result to its CPU owner.
        void drawGui(const std::shared_ptr<tgui::RootContainer>& root) override;
        void addClippingLayer(const tgui::RenderStates& states, tgui::FloatRect rect) override;
        void removeClippingLayer() override;
        void drawVertexArray(
            const tgui::RenderStates& states,
            const tgui::Vertex* vertices,
            std::size_t vertexCount,
            const unsigned int* indices,
            std::size_t indexCount,
            const std::shared_ptr<tgui::BackendTexture>& texture
        ) override;
        // The application owns platform clearing; toolkit mainLoop is not used.
        void setClearColor(const tgui::Color& color) override;
        void clearScreen() override;

    private:
        void updateClipping(tgui::FloatRect clipRect, tgui::FloatRect clipViewport) override;
        void require_recording() const;
        [[nodiscard]] bool drawable() const;
    };
}
