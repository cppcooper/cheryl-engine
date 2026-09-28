#pragma once

#include <assets/2d/graphic.h>
#include <assets/2d/sprite.h>
#include <assets/2d/stbfont.h>
#include <assets/2d/tileset.h>

#include <glm.hpp>

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace CE::RenderAPIs {
    /** Values that vary per draw. The material is retained until the frame is consumed;
     * only the renderer may use it to change graphics state.
     */
    struct DrawStyle {
        std::shared_ptr<Assets::Shader> material;
        glm::mat4 model_matrix{1.0f};
        float scale = 1.0f;
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
        glm::vec3 position{0.0f};
        float angle = 0.0f;
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

    /** Move completed passes into the frame before handing it to the renderer.
     * Each command owns its per-draw values and retains its shared asset handles.
     */
    class RenderFrame final {
    public:
        explicit RenderFrame(std::vector<RenderPass> passes) : passes_(std::move(passes)) {}

        [[nodiscard]] const std::vector<RenderPass>& passes() const { return passes_; }

    private:
        std::vector<RenderPass> passes_;
    };
}
