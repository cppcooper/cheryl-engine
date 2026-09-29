#pragma once

#include <assets/types/2d/graphic.h>
#include <assets/types/2d/sprite.h>
#include <assets/types/2d/stbfont.h>
#include <assets/types/2d/tileset.h>

#include <glm.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace CE::RenderAPIs {
    /** Values resolved by simulation for one draw. Position, rotation, and scale
     * are already in model_matrix; the renderer does not read an entity transform.
     * The material is retained until the frame is consumed.
     */
    struct DrawStyle {
        std::shared_ptr<Assets::Shader> material;
        glm::mat4 model_matrix{1.0f};
        float alpha = 1.0f;
    };

    struct SpriteDraw {
        std::shared_ptr<const Assets::Sprite> sprite;
        Assets::CellIndex cell{};
        DrawStyle style;
    };

    struct TileDraw {
        std::shared_ptr<const Assets::Tileset> tileset;
        Assets::CellIndex cell{};
        DrawStyle style;
    };

    struct GraphicDraw {
        std::shared_ptr<const Assets::Graphic> graphic;
        DrawStyle style;
    };

    struct TextDraw {
        std::shared_ptr<const Assets::STBFont> font;
        std::string text;
        DrawStyle style;
    };

    using DrawCommand = std::variant<SpriteDraw, TileDraw, GraphicDraw, TextDraw>;

    /** One ordered pass. Matrices are copied from the simulation's camera; the
     * renderer never observes a live CameraBase or animation playback cursor.
     */
    struct RenderPass {
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
        bool depth_test = false;
        std::vector<DrawCommand> draws;
    };

    class RenderFrameWriter;
    class RenderPassWriter;

    /** One reusable storage slot. Only the active passes are published; inactive pass
     * objects keep their draw-vector capacity for later ticks. Runtime ownership must
     * prevent writing or recycling while the renderer reads this frame.
     */
    class RenderFrame final {
    public:
        RenderFrame() = default;
        RenderFrame(const RenderFrame&) = delete;
        RenderFrame& operator=(const RenderFrame&) = delete;
        RenderFrame(RenderFrame&&) = delete;
        RenderFrame& operator=(RenderFrame&&) = delete;

        [[nodiscard]] std::span<const RenderPass> passes() const { return {passes_.data(), active_passes_}; }

        // After rendering or supersession, release command handles on the graphics
        // thread while its context is current, then return the slot to simulation.
        void recycle() {
            for (std::size_t i = 0; i < active_passes_; ++i) passes_[i].draws.clear();
            active_passes_ = 0;
        }

    private:
        friend class RenderFrameWriter;
        friend class RenderPassWriter;

        std::vector<RenderPass> passes_;
        std::size_t active_passes_ = 0;
    };

    /** A short-lived handle to one pass. Its index stays valid if adding another pass
     * grows the frame's outer vector. Use it only while the frame is being prepared.
     */
    class RenderPassWriter final {
    public:
        void reserve_draws(std::size_t count) { frame_.passes_[index_].draws.reserve(count); }

        template <typename Draw>
        void add(Draw&& draw) {
            frame_.passes_[index_].draws.emplace_back(std::forward<Draw>(draw));
        }

    private:
        friend class RenderFrameWriter;
        RenderPassWriter(RenderFrame& frame, std::size_t index) : frame_(frame), index_(index) {}

        RenderFrame& frame_;
        std::size_t index_;
    };

    /** Writes into a free slot after update(). Camera matrices are copied once per
     * pass; draw commands are constructed in the slot's retained vector storage.
     */
    class RenderFrameWriter final {
    public:
        explicit RenderFrameWriter(RenderFrame& frame) : frame_(frame) {
            if (frame_.active_passes_ != 0)
                throw std::logic_error("Render frame must be recycled before writing again");
        }

        void reserve_passes(std::size_t count) { frame_.passes_.reserve(count); }

        [[nodiscard]] RenderPassWriter begin_pass(const glm::mat4& projection, const glm::mat4& view,
                                                  bool depth_test = false) {
            const auto index = frame_.active_passes_;
            if (index == frame_.passes_.size()) frame_.passes_.emplace_back();
            auto& pass = frame_.passes_[index];
            pass.projection = projection;
            pass.view = view;
            pass.depth_test = depth_test;
            ++frame_.active_passes_;
            return RenderPassWriter(frame_, index);
        }

    private:
        RenderFrame& frame_;
    };
}
