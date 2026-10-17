#include "controls.h"

#include <cheryl/backends/opengl/resource-provider.h>
#include <cheryl/core/engine/engine-context.h>

#include <TGUI/Widgets/Button.hpp>

#include <stdexcept>
#include <utility>
#include <variant>

namespace {
    // [materials]
    CE::UI::TGUI::Materials materials(CE::Rendering::Resources::Provider& provider, const std::filesystem::path& root) {
        auto* native = dynamic_cast<CE::Rendering::OpenGL::ResourceProvider*>(&provider);
        if (!native)
            throw std::invalid_argument("Tutorial UI requires the selected OpenGL resource provider");
        CE::Rendering::Pipelines::Definition definition;
        definition.vertex_layout = CE::Rendering::Geometry::Layout2D::Position3UV2Color4;
        definition.topology = CE::Rendering::Geometry::Topology::Triangles;
        definition.state = {
            CE::Rendering::Pipelines::Blend::StraightAlpha, CE::Rendering::Pipelines::Depth::Disabled, false,
            CE::Rendering::Pipelines::Cull::None
        };
        definition.parameters = {
            {"projection", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Projection}
        };
        definition.program_sources = {root / "graphics/shaders/tgui.vert", root / "graphics/shaders/tgui-solid.frag"};
        const CE::Rendering::OpenGL::PipelineBindings solid_bindings{{{"projection", "projectionMatrix"}}};
        CE::UI::TGUI::Materials result;
        result.solid = native->build_material({native->build_pipeline(definition, solid_bindings), {}});
        definition.program_sources[1] = root / "graphics/shaders/tgui-textured.frag";
        definition.parameters.push_back({"image", CE::Rendering::Parameters::Type::Sampler2D});
        const CE::Rendering::OpenGL::PipelineBindings textured_bindings{{{"projection", "projectionMatrix"}, {"image", "mytexture"}}};
        result.textured = native->build_material({native->build_pipeline(std::move(definition), textured_bindings), {}});
        return result;
    }
    // [/materials]
}

namespace Tutorial {
    Controls::Controls(CE::Runtime::Context& engine, const std::filesystem::path& assets)
    : input_(engine.input()),
      platform_(engine.platform_dispatcher().submission()),
      uploader_(engine.resources()),
      materials_(::materials(engine.resources(), assets)) {}

    // [widgets]
    void Controls::create_widgets(const CE::Game::TickContext& tick) {
        session_ = std::make_unique<CE::UI::TGUI::Session>(input_, 2);
        session_->set_view(tick.logical_size, tick.framebuffer_size);

        auto button = tgui::Button::create("Apply");
        button->setSize({120, 32});
        button->setPosition({16, 80});
        button->onPress([this] { apply_requested_ = true; });
        session_->gui().add(button);

        field_ = tgui::EditBox::create();
        field_->setSize({240, 32});
        field_->setPosition({16, 124});
        field_->setDefaultText("Type a name");
        session_->gui().add(field_);
        session_->request_keyboard_focus();
        field_->setFocused(true);
    }
    // [/widgets]

    // [update]
    bool Controls::update(const CE::Game::TickContext& tick) {
        if (!session_)
            create_widgets(tick);
        else
            session_->set_view(tick.logical_size, tick.framebuffer_size);

        session_->handle_input(tick.input.records(), true, [&](const CE::Input::Events::Record& record) {
            const auto* button = std::get_if<CE::Input::Events::Button>(&record.data);
            if (button && record.device_kind == CE::Input::DeviceKind::Keyboard) {
                if (button->key == CE::Input::KeyboardKey::Escape) {
                    if (button->phase == CE::Input::Events::ButtonPhase::Press && session_->owns_keyboard_focus()) {
                        ignore_quit_until_release_ = true;
                        session_->release_keyboard_focus();
                    } else if (button->phase == CE::Input::Events::ButtonPhase::Release)
                        ignore_quit_until_release_ = false;
                } else if (button->key == CE::Input::KeyboardKey::F2 && button->phase == CE::Input::Events::ButtonPhase::Press) {
                    session_->request_keyboard_focus();
                    field_->setFocused(true);
                }
            }
            return true;
        });
        session_->update_time(tick.delta_seconds);
        const bool applied = std::exchange(apply_requested_, false);
        if (applied)
            field_->setText("Applied");

        static_cast<void>(CE::UI::TGUI::adopt_scene(pending_, scene_));
        if (!pending_.valid())
            pending_ = uploader_.submit(platform_, session_->record(), materials_);
        return applied;
    }
    // [/update]

    void Controls::write(CE::Rendering::FrameWriter& frame) const {
        scene_.write(frame);
    }

    // [cleanup]
    void Controls::close_after_quiescence() {
        field_.reset();
        if (session_) {
            session_->close_after_quiescence();
            session_.reset();
        }
        scene_ = CE::UI::TGUI::Scene{};
    }
    // [/cleanup]
}
