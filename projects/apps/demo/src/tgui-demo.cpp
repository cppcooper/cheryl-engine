#include "tgui-demo.h"

#include <backends/opengl/resource-provider.h>
#include <core/engine/engine-context.h>
#include <internals/exceptions.h>
#include <ui/tgui/input.h>
#include <ui/tgui/scene.h>
#include <ui/tgui/session.h>

#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Panel.hpp>
#include <TGUI/Widgets/Picture.hpp>
#include <TGUI/Widgets/ScrollablePanel.hpp>

#include <array>
#include <exception>
#include <format>
#include <future>
#include <string>
#include <utility>

namespace {
    CE::UI::TGUI::Materials materials(CE::Assets::ResourceProvider& provider, const std::filesystem::path& root) {
        using namespace CE::Assets;
        auto* native = dynamic_cast<OpenGLResourceProvider*>(&provider);
        if (!native)
            throw CE::Exceptions::invalid_args(CE_HERE, "The demo UI requires an OpenGL material provider");
        PipelineDefinition definition;
        definition.vertex_layout = VertexLayout2D::Position3UV2Color4;
        definition.topology = PrimitiveTopology::Triangles;
        definition.state = {BlendMode::StraightAlpha, DepthMode::Disabled, false, CullMode::None};
        definition.parameters = {{"projection", ParameterType::Mat4, true, ParameterSemantic::Projection}};
        definition.program_sources = {root / "shaders/tgui.vert", root / "shaders/tgui-solid.frag"};
        const GLSLPipelineBindings solid_bindings{{{"projection", "projectionMatrix"}}};
        CE::UI::TGUI::Materials result;
        result.solid = native->build_material({native->build_pipeline(definition, solid_bindings), {}});
        definition.program_sources[1] = root / "shaders/tgui-textured.frag";
        definition.parameters.push_back({"image", ParameterType::Sampler2D});
        const GLSLPipelineBindings textured_bindings{{{"projection", "projectionMatrix"}, {"image", "mytexture"}}};
        result.textured = native->build_material({native->build_pipeline(std::move(definition), textured_bindings), {}});
        return result;
    }

    tgui::Texture image(const bool alternate) {
        constexpr unsigned int extent = 16;
        std::array<std::uint8_t, extent * extent * 4> pixels{};
        constexpr std::array<tgui::Color, 4> colors{tgui::Color{240, 90, 85}, tgui::Color{80, 210, 140}, tgui::Color{75, 145, 245},
                                                    tgui::Color{245, 205, 85}};
        for (unsigned int y = 0; y < extent; ++y)
            for (unsigned int x = 0; x < extent; ++x) {
                const auto color = colors[((x >= extent / 2) + 2 * (y >= extent / 2) + (alternate ? 1 : 0)) % colors.size()];
                const auto offset = (y * extent + x) * 4;
                pixels[offset] = color.getRed();
                pixels[offset + 1] = color.getGreen();
                pixels[offset + 2] = color.getBlue();
                pixels[offset + 3] = 255;
            }
        tgui::Texture texture;
        texture.loadFromPixelData({extent, extent}, pixels.data(), {}, {}, true);
        return texture;
    }

    tgui::Label::Ptr label(const tgui::String& text, const unsigned int size = 16) {
        auto widget = tgui::Label::create(text);
        widget->setTextSize(size);
        widget->getRenderer()->setTextColor({235, 240, 250});
        return widget;
    }
}

struct DemoUi::State {
    CE::Input::iInputSystem& input;
    CE::Engine::PlatformDispatcher::Submission platform;
    CE::UI::TGUI::SceneUploader uploader;
    CE::UI::TGUI::Materials materials;
    // External widget handles are declared after session and destroyed first.
    std::unique_ptr<CE::UI::TGUI::Session> session;
    tgui::Panel::Ptr panel;
    tgui::Label::Ptr status_label;
    tgui::EditBox::Ptr field;
    tgui::Picture::Ptr picture;
    tgui::Button::Ptr edge;
    CE::UI::TGUI::Scene scene;
    std::future<CE::UI::TGUI::Scene> pending;
    std::string upload_error;
    bool visible = true;
    bool reset_requested = false;
    bool alternate_image = false;

    State(CE::Engine::EngineContext& engine, const std::filesystem::path& root)
    : input(engine.input()),
      platform(engine.platform_dispatcher().submission()),
      uploader(engine.resources()),
      materials(::materials(engine.resources(), root)) {}

    void create_widgets(const CE::GFramework::TickContext& tick) {
        session = std::make_unique<CE::UI::TGUI::Session>(input, 2);
        session->set_view(tick.logical_size, tick.framebuffer_size);
        auto& gui = session->gui();
        gui.setTextSize(16);
        gui.setKeyboardNavigationEnabled(true);
        panel = tgui::Panel::create({426, 500});
        panel->setPosition({"100% - 450", 24});
        panel->getRenderer()->setBackgroundColor({28, 36, 54, 225});
        panel->getRenderer()->setBorderColor({95, 125, 170});
        panel->getRenderer()->setBorders(1);
        gui.add(panel, "demo-panel");

        auto title = label("Cheryl UI", 24);
        title->setPosition({18, 12});
        panel->add(title);
        auto instructions = label("F2: edit   Esc: release focus   F3: hide/show");
        instructions->setPosition({18, 48});
        panel->add(instructions);
        status_label = label("");
        status_label->setPosition({18, 80});
        panel->add(status_label);

        auto reset = tgui::Button::create("Reset camera");
        reset->setPosition({18, 148});
        reset->setSize({180, 32});
        reset->onPress([this] { reset_requested = true; });
        panel->add(reset);
        field = tgui::EditBox::create();
        field->setPosition({18, 198});
        field->setSize({388, 34});
        field->setDefaultText("Click here and type");
        field->getRenderer()->setBackgroundColor({240, 244, 250});
        panel->add(field, "text-field");
        auto editing = label("Arrows, Home/End and Backspace work here");
        editing->setTextSize(14);
        editing->setPosition({18, 240});
        panel->add(editing);

        auto scroll = tgui::ScrollablePanel::create({388, 134}, {350, 310});
        scroll->setPosition({18, 270});
        scroll->getRenderer()->setBackgroundColor({45, 60, 85, 180});
        scroll->setHorizontalScrollbarPolicy(tgui::Scrollbar::Policy::Never);
        for (unsigned int i = 0; i < 8; ++i) {
            auto row = tgui::Panel::create({330, 34});
            row->setPosition({8, 8 + i * 38.0f});
            row->getRenderer()->setBackgroundColor({70, 105, 145, static_cast<std::uint8_t>(i % 2 ? 150 : 90)});
            auto text = label(std::format("Scroll item {}", i + 1));
            text->setPosition({8, 6});
            row->add(text);
            scroll->add(row);
        }
        panel->add(scroll);

        auto image_panel = tgui::Panel::create({388, 68});
        image_panel->setPosition({18, 416});
        image_panel->getRenderer()->setBackgroundColor({85, 75, 115, 130});
        picture = tgui::Picture::create(image(false));
        picture->setPosition({8, 8});
        picture->setSize({52, 52});
        image_panel->add(picture);
        auto change = tgui::Button::create("Change image");
        change->setPosition({82, 17});
        change->setSize({180, 32});
        change->onPress([this] {
            alternate_image = !alternate_image;
            picture->getRenderer()->setTexture(image(alternate_image));
        });
        image_panel->add(change);
        panel->add(image_panel);

        // A small translucent panel overlaps the main panel's upper edge.
        auto badge = tgui::Panel::create({100, 28});
        badge->setPosition({308, -10});
        badge->getRenderer()->setBackgroundColor({80, 140, 170, 170});
        auto badge_text = label("TGUI", 16);
        badge_text->setPosition({26, 4});
        badge->add(badge_text);
        panel->add(badge);

        edge = tgui::Button::create("?");
        edge->setPosition({"100% - 46", "100% - 46"});
        edge->setSize({28, 28});
        auto tooltip = label("This tooltip stays inside the window.");
        tooltip->setTextSize(14);
        tooltip->getRenderer()->setBackgroundColor({30, 45, 65, 240});
        edge->setToolTip(tooltip);
        gui.add(edge, "edge-tooltip");
    }

    void handle_input(const CE::GFramework::TickContext& tick) {
        using namespace CE::Input;
        for (const auto& record : tick.input.records()) {
            const auto* button = std::get_if<ButtonEvent>(&record.data);
            if (button && record.device_kind == DeviceKind::Keyboard && button->phase == ButtonPhase::Press) {
                if (button->key == KeyboardKey::F2 && visible) {
                    if (session->owns_keyboard_focus())
                        session->release_keyboard_focus();
                    else {
                        session->request_keyboard_focus();
                        field->setFocused(true);
                    }
                } else if (button->key == KeyboardKey::F3) {
                    visible = !visible;
                    panel->setVisible(visible);
                    edge->setVisible(visible);
                    if (!visible)
                        session->release_keyboard_focus();
                } else if (button->key == KeyboardKey::Escape && session->owns_keyboard_focus())
                    session->release_keyboard_focus();
            }
            if (visible && tick.logical_size.width > 0 && tick.logical_size.height > 0 && button &&
                record.device_kind == DeviceKind::Mouse && button->mouse_button == MouseButton::Left &&
                button->phase == ButtonPhase::Press) {
                const auto event = CE::UI::TGUI::translate_event(record);
                if (event) {
                    auto& gui = session->gui();
                    const auto point = gui.mapPixelToCoords({event->mouseButton.x, event->mouseButton.y});
                    if (gui.getWidgetAtPos(point, true)) {
                        if (!session->owns_keyboard_focus())
                            session->request_keyboard_focus();
                    } else
                        session->release_keyboard_focus();
                }
            }
            session->handle_input(std::span{&record, 1}, visible);
        }
        // Empty updates still notice external focus preemption.
        if (tick.input.records().empty())
            session->handle_input({}, visible);
    }
};

DemoUi::DemoUi(CE::Engine::EngineContext& engine, const std::filesystem::path& asset_root)
: state_(std::make_unique<State>(engine, asset_root)) {}

DemoUi::~DemoUi() = default;

bool DemoUi::update(const CE::GFramework::TickContext& tick, const DemoUiStatus& status) {
    auto& state = *state_;
    if (!state.session)
        state.create_widgets(tick);
    else
        state.session->set_view(tick.logical_size, tick.framebuffer_size);
    state.handle_input(tick);
    state.session->update_time(tick.delta_seconds);
    state.status_label->setText(
        std::format(
            "Updates: {}   Camera: {:.0f}, {:.0f}\nClicks: {}   Wheel: {:.2f}\nGamepad A: {} presses", status.updates, status.pan_x,
            status.pan_y, status.clicks, status.wheel, status.gamepad_presses
        )
    );
    try {
        if (CE::UI::TGUI::adopt_scene(state.pending, state.scene))
            state.upload_error.clear();
        // No queue growth: keep one complete scene while its replacement waits.
        if (!state.pending.valid())
            state.pending = state.uploader.submit(state.platform, state.session->record(), state.materials);
    } catch (const std::exception& error) {
        state.upload_error = error.what(); // The last complete scene remains usable.
    }
    return std::exchange(state.reset_requested, false);
}

void DemoUi::write(CE::RenderAPIs::RenderFrameWriter& frame) const {
    if (state_->visible)
        state_->scene.write(frame);
}

std::string_view DemoUi::error() const {
    return state_->upload_error;
}
