#include <assets/types/2d/stbfont.h>
#include <assets/submission/draw2d.h>
#include <backends/opengl/resource-provider.h>
#include <backends/opengl/glfw-backend.h>
#include <core/controls/input-interface.h>
#include <core/controls/input-system.h>
#include <core/display/window-interface.h>
#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/game-framework/game-runtime.h>
#include <core/rendering/camera.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/font-mgr.h>
#include <core/resources/asset-management/material-mgr.h>
#include <core/resources/fileio/fonts-system.h>
#include <internals/exceptions.h>

#include <ext/matrix_transform.hpp>
#include <gainput/gainput.h>

#ifdef CHERYL_DEMO_TGUI
#include "tgui-demo.h"
#endif
#ifdef CHERYL_DEMO_RMLUI
#include "rmlui-demo.h"
#endif

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <exception>
#include <format>
#include <future>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace DemoActions {
    constexpr CE::Input::ActionId Up{1};
    constexpr CE::Input::ActionId Left{2};
    constexpr CE::Input::ActionId Down{3};
    constexpr CE::Input::ActionId Right{4};
    constexpr CE::Input::ActionId Reset{5};
    constexpr CE::Input::ActionId MouseX{6};
    constexpr CE::Input::ActionId MouseY{7};
    constexpr CE::Input::ActionId Click{8};
    constexpr CE::Input::ActionId WheelY{9};
    constexpr CE::Input::ActionId GamepadA{11};
    constexpr CE::Input::ActionId QuitGame{13};
} // namespace DemoActions

class Game : public CE::GFramework::AbstractGame {
    CE::Input::CaptureLease events_;
#ifdef CHERYL_DEMO_TGUI
    std::unique_ptr<DemoUi> ui_;
#endif
#ifdef CHERYL_DEMO_RMLUI
    std::unique_ptr<DemoRmlUi> rml_ui_;
#endif
#if !defined(CHERYL_DEMO_TGUI) && !defined(CHERYL_DEMO_RMLUI)
    static constexpr CE::Input::FocusId text_box = 1;
    CE::Input::CaptureLease text_capture_;
    CE::Input::FocusLease focus_;
    std::u32string text_;
    std::size_t caret_ = 0;
#endif
    CE::Engine::EngineContext& engine_;
    CE::Camera2D camera_;
    std::filesystem::path asset_root_;
    bool load_all_assets_;
    std::shared_ptr<CE::Assets::STBFont> font_;
    std::shared_ptr<const CE::Assets::Material> font_shader_;
    std::future<std::shared_ptr<const CE::Assets::Material>> pending_shader_;
    std::string reload_error_;
    glm::vec2 pan_{0.0f, 0.0f};
    float mouse_x_ = 0.0f;
    float mouse_y_ = 0.0f;
    std::uint64_t clicks_ = 0;
    double wheel_ = 0.0;
    std::uint64_t gamepad_presses_ = 0;
    std::uint64_t updates_ = 0;
    std::uint64_t update_limit_ = 0;
    std::function<void()> stop_;

public:
    Game(CE::Engine::EngineContext& engine, std::filesystem::path asset_root, bool load_all_assets)
    : engine_(engine), asset_root_(std::move(asset_root)), load_all_assets_(load_all_assets) {}

    void stop_after_updates(const std::uint64_t count, std::function<void()> stop) {
        if (count == 0 || !stop)
            throw CE::Exceptions::invalid_args(CE_HERE, "A finite demo run requires an update count and stop callback");
        update_limit_ = count;
        stop_ = std::move(stop);
    }

    [[nodiscard]] std::uint64_t completed_updates() const { return updates_; }

    void init() override {
        camera_.set_framebuffer_size(engine_.window().framebuffer_size());

        const auto shader2d = asset_root_ / "shaders" / "shader2d";
        auto& resources = engine_.resources();
        if (load_all_assets_) {
            CE::Assets::Loader loader(asset_root_);
            loader.load_assets(resources);
        }
        // Environment/bootstrap choices belong to the application, independently of asset manifests.
        const auto font_path = CE::Resources::select_default_system_font(CE::Resources::find_system_fonts());
        if (!font_path)
            throw CE::Exceptions::runtime_exception(CE_HERE, "No supported system font was found");
        CE::Assets::FontMgr::get().load_assets({*font_path}, resources);
        font_ = std::dynamic_pointer_cast<CE::Assets::STBFont>(CE::Assets::FontMgr::get().default_font());
        if (!font_)
            throw CE::Exceptions::runtime_exception(CE_HERE, "No supported system font was found");
        CE::Assets::MaterialMgr::get().load_material(shader2d, resources, font_recipe(shader2d));
        font_shader_ = CE::Assets::MaterialMgr::get().get_asset(shader2d);

        auto& input = engine_.input();
        auto& bindings = input.bindings();
        const auto keyboard = input.keyboard_id();
        (void)bindings.bind_button({keyboard, gainput::KeyW}, DemoActions::Up);
        (void)bindings.bind_button({keyboard, gainput::KeyA}, DemoActions::Left);
        (void)bindings.bind_button({keyboard, gainput::KeyS}, DemoActions::Down);
        (void)bindings.bind_button({keyboard, gainput::KeyD}, DemoActions::Right);
        (void)bindings.bind_button({keyboard, gainput::KeyR}, DemoActions::Reset);
        (void)bindings.bind_button({keyboard, gainput::KeyQ}, DemoActions::QuitGame);

        const auto mouse = input.mouse_id();
        (void)bindings.bind_axis({mouse, gainput::MouseAxisX}, DemoActions::MouseX);
        (void)bindings.bind_axis({mouse, gainput::MouseAxisY}, DemoActions::MouseY);
        (void)bindings.bind_button({mouse, gainput::MouseButtonLeft}, DemoActions::Click);
        (void)bindings.bind_axis(
            {mouse, CE::Input::MouseControl::ScrollY}, DemoActions::WheelY, {1.0f, 0.0f, CE::Input::AxisKind::Relative}
        );
        (void)bindings.bind_button({input.gamepad_id(), gainput::PadButtonA}, DemoActions::GamepadA);
        events_ = input.capture(CE::Input::InputMode::Events);
#ifdef CHERYL_DEMO_TGUI
        ui_ = std::make_unique<DemoUi>(engine_, asset_root_);
#endif
#ifdef CHERYL_DEMO_RMLUI
        rml_ui_ = std::make_unique<DemoRmlUi>(engine_, asset_root_, *font_path);
#endif
    }

    void deinit() override {
#ifdef CHERYL_DEMO_RMLUI
        rml_ui_.reset();
#endif
#ifdef CHERYL_DEMO_TGUI
        ui_.reset(); // Runtime has joined simulation; widgets die before its backend.
#endif
#if !defined(CHERYL_DEMO_TGUI) && !defined(CHERYL_DEMO_RMLUI)
        focus_.reset();
        text_capture_.reset();
#endif
        events_.reset();
        engine_.input().bindings().clear();
        font_.reset();
        pending_shader_ = {};
        font_shader_.reset();
    }

    void update(const CE::GFramework::TickContext& tick) override {
        if (tick.input.button(DemoActions::QuitGame).held()) {
            tick.request_stop();
        }
        if (pending_shader_.valid() && pending_shader_.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
            try {
                font_shader_ = pending_shader_.get();
                reload_error_.clear();
            } catch (const std::exception& error) {
                reload_error_ = error.what(); // Keep the previous complete material generation.
            }
        }
        const auto& actions = tick.input;
        camera_.set_framebuffer_size(tick.framebuffer_size);
        if (actions.button(DemoActions::Reset).pressed()) {
            pan_ = {0.0f, 0.0f};
            camera_.set_view_matrix(glm::mat4(1.0f));
        }
        mouse_x_ = actions.axis(DemoActions::MouseX).current;
        mouse_y_ = actions.axis(DemoActions::MouseY).current;
        clicks_ += actions.button(DemoActions::Click).press_count;
        wheel_ += actions.axis(DemoActions::WheelY).delta();
        gamepad_presses_ += actions.button(DemoActions::GamepadA).press_count;

        // Records carry the owner selected when they were collected, including
        // those still pending when this update requests a focus transfer.
        for (const auto& record : actions.records()) {
            const auto* button = std::get_if<CE::Input::ButtonEvent>(&record.data);
            if (record.to_gameplay && record.device_kind == CE::Input::DeviceKind::Keyboard && button && button->button == gainput::KeyF5 &&
                button->phase == CE::Input::ButtonPhase::Press && !pending_shader_.valid()) {
                const auto key = asset_root_ / "shaders" / "shader2d";
                const auto builder = font_recipe(key);
                pending_shader_ = engine_.platform_dispatcher().submit([key, builder](CE::Engine::EngineContext& platform) {
                    auto& materials = CE::Assets::MaterialMgr::get();
                    materials.reload_material(key, platform.resources(), builder);
                    return materials.get_asset(key);
                });
            }
#if !defined(CHERYL_DEMO_TGUI) && !defined(CHERYL_DEMO_RMLUI)
            if (record.device_kind == CE::Input::DeviceKind::Keyboard && button && button->button == gainput::KeyF2 &&
                button->phase == CE::Input::ButtonPhase::Press) {
                if (focus_.owns_focus()) {
                    focus_.reset();
                    text_capture_.reset();
                } else {
                    text_capture_ = engine_.input().capture(CE::Input::InputMode::Text);
                    focus_ = engine_.input().routing().focus(text_box);
                }
                continue;
            }
            if (record.target != text_box)
                continue;
            if (const auto* text = std::get_if<CE::Input::TextEvent>(&record.data)) {
                text_.insert(caret_, 1, text->codepoint);
                ++caret_;
            } else if (button && button->phase != CE::Input::ButtonPhase::Release) {
                switch (button->button) {
                    case gainput::KeyBackSpace:
                        if (caret_ > 0)
                            text_.erase(--caret_, 1);
                        break;
                    case gainput::KeyDelete:
                        if (caret_ < text_.size())
                            text_.erase(caret_, 1);
                        break;
                    case gainput::KeyLeft:
                        if (caret_ > 0)
                            --caret_;
                        break;
                    case gainput::KeyRight:
                        if (caret_ < text_.size())
                            ++caret_;
                        break;
                    case gainput::KeyHome:
                        caret_ = 0;
                        break;
                    case gainput::KeyEnd:
                        caret_ = text_.size();
                        break;
                    case gainput::KeyEscape:
                    case gainput::KeyReturn:
                        focus_.reset();
                        text_capture_.reset();
                        break;
                    default:
                        break;
                }
            }
#endif
        }

        const glm::vec2 movement{
            static_cast<float>(tick.button_simulation_seconds(DemoActions::Right) - tick.button_simulation_seconds(DemoActions::Left)),
            static_cast<float>(tick.button_simulation_seconds(DemoActions::Up) - tick.button_simulation_seconds(DemoActions::Down))};
        if (glm::length(movement) > 0.0f) {
            // Scale the observed down-time fraction by this update's simulation
            // delta. A completed observed tap still contributes after release.
            pan_ += movement * 240.0f;
            camera_.set_view_matrix(glm::translate(glm::mat4(1.0f), glm::vec3(-pan_, 0.0f)));
        }
#ifdef CHERYL_DEMO_TGUI
        if (ui_->update(tick, {updates_ + 1, clicks_, gamepad_presses_, wheel_, pan_.x, pan_.y})) {
            pan_ = {0.0f, 0.0f};
            camera_.set_view_matrix(glm::mat4(1.0f));
        }
#endif
#ifdef CHERYL_DEMO_RMLUI
        if (rml_ui_->update(tick, {updates_ + 1, clicks_, gamepad_presses_, wheel_, pan_.x, pan_.y})) {
            pan_ = {0.0f, 0.0f};
            camera_.set_view_matrix(glm::mat4(1.0f));
        }
#endif
        if (++updates_ == update_limit_ && stop_)
            stop_();
    }

    void prepare_render_frame(CE::RenderAPIs::RenderFrameWriter& frame) const override {
        const auto size = camera_.framebuffer_size();
        auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
        const CE::Assets::SubmissionContext2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        CE::RenderAPIs::DrawStyle2D text;
        text.material = font_shader_;
        text.model_matrix = glm::translate(
            glm::mat4(1.0f), glm::vec3(static_cast<float>(size.width) * 0.5f - 120.0f, static_cast<float>(size.height) * 0.5f, 0.0f)
        );
        pass.add(CE::Assets::resolve_text(*font_, "Camera target", text, context));

        // Compensate for the view translation so these controls stay fixed on screen.
        text.model_matrix =
            glm::translate(glm::mat4(1.0f), glm::vec3(pan_.x + 24.0f, pan_.y + static_cast<float>(size.height) - 56.0f, 0.0f));
        auto hud = std::format(
            "Cheryl Engine demo\nWASD: pan camera  R: reset  F5: reload shader\n"
            "Mouse: {:.2f}, {:.2f}  Clicks: {}  Wheel: {:.2f}\nGamepad A: {} presses\n"
            "Esc: release focus  Q: quit while gameplay has focus\nReload: {}",
            mouse_x_, mouse_y_, clicks_, wheel_, gamepad_presses_, reload_error_
        );
#ifdef CHERYL_DEMO_TGUI
        hud += std::format("\nF2: TGUI text focus  F3: hide/show TGUI\nTGUI: {}", ui_->error());
#endif
#ifdef CHERYL_DEMO_RMLUI
        hud += std::format("\nF4: RmlUi text focus  F6: hide/show RmlUi\nRmlUi: {}", rml_ui_->error());
#endif
#if !defined(CHERYL_DEMO_TGUI) && !defined(CHERYL_DEMO_RMLUI)
        hud += std::format(
            "\nF2: text focus  Enter/Esc: release focus\nArrows/Home/End: caret\nText [{}]: {}",
            focus_.owns_focus() ? "focused" : "unfocused", text_preview()
        );
#endif
        pass.add(CE::Assets::resolve_text(*font_, hud, text, context));
#ifdef CHERYL_DEMO_TGUI
        ui_->write(frame);
#endif
#ifdef CHERYL_DEMO_RMLUI
        rml_ui_->write(frame);
#endif
    }

private:
    CE::Assets::MaterialMgr::Builder font_recipe(const std::filesystem::path& key) const {
        const auto atlas = font_->glyph_atlas_handle();
        return [key, atlas](CE::Assets::ResourceProvider& provider) {
            using namespace CE::Assets;
            auto* native = dynamic_cast<OpenGLResourceProvider*>(&provider);
            if (!native)
                throw CE::Exceptions::invalid_args(CE_HERE, "The GLFW demo requires an OpenGL material provider");
            PipelineDefinition definition;
            definition.program_sources = {key.string() + ".vert", key.string() + ".frag"};
            definition.parameters = {{"projection", ParameterType::Mat4, true, ParameterSemantic::Projection},
                {"view", ParameterType::Mat4, true, ParameterSemantic::View},
                {"model", ParameterType::Mat4, true, ParameterSemantic::Model},
                {"alpha", ParameterType::Float, true, ParameterSemantic::Alpha},
                {"scale", ParameterType::Float, true, ParameterSemantic::Scale}, {"image", ParameterType::Sampler2D}};
            const GLSLPipelineBindings bindings{{{"projection", "projectionMatrix"}, {"view", "viewMatrix"}, {"model", "modelMatrix"},
                {"alpha", "in_Alpha"}, {"scale", "in_Scale"}, {"image", "mytexture"}}};
            return native->build_material({native->build_pipeline(std::move(definition), bindings), {{"image", ImageBinding{atlas, 0}}}});
        };
    }

#if !defined(CHERYL_DEMO_TGUI) && !defined(CHERYL_DEMO_RMLUI)
    [[nodiscard]] std::string text_preview() const {
        // The current font atlas contains ASCII. Editing retains Unicode scalars;
        // display one fallback per unsupported scalar instead of pretending to shape text.
        std::string preview;
        for (std::size_t i = 0; i <= text_.size(); ++i) {
            if (i == caret_)
                preview += '|';
            if (i < text_.size())
                preview += text_[i] >= 32 && text_[i] <= 126 ? static_cast<char>(text_[i]) : '?';
        }
        return preview;
    }
#endif
};

using CE::GFramework::GameRuntime;

int main(const int argc, char** argv) {
    std::filesystem::path asset_root = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets";
    bool load_all_assets = false;
    unsigned int max_updates = 0;
    auto mode = CE::GFramework::RunMode::Sequential;
    CE::Input::PollingOptions polling;
    CE::GFramework::SimulationTimingOptions timing;
    const auto number = [](const std::string_view text) {
        unsigned int value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size())
            throw CE::Exceptions::invalid_args(CE_HERE, "Timing and polling options require a nonnegative integer");
        return value;
    };
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (std::string_view(argv[i]) == "--full-assets")
            load_all_assets = true;
        else if (std::string_view(argv[i]) == "--concurrent")
            mode = CE::GFramework::RunMode::Concurrent;
        else if (argument == "--input-unlimited")
            polling.policy = CE::Input::PollingPolicy::Unlimited;
        else if (argument.starts_with("--max-updates="))
            max_updates = number(argument.substr(std::string_view("--max-updates=").size()));
        else if (argument == "--fixed")
            timing.mode = CE::GFramework::SimulationMode::Fixed;
        else if (argument == "--variable-catch-up") {
            timing.mode = CE::GFramework::SimulationMode::Fixed;
            timing.recovery = CE::GFramework::LagRecovery::VariableCatchUp;
        } else if (argument.starts_with("--fixed-step-ms=")) {
            timing.mode = CE::GFramework::SimulationMode::Fixed;
            timing.fixed_step = std::chrono::milliseconds(number(argument.substr(std::string_view("--fixed-step-ms=").size())));
        } else if (argument.starts_with("--variable-interval-ms="))
            timing.variable_interval =
                std::chrono::milliseconds(number(argument.substr(std::string_view("--variable-interval-ms=").size())));
        else if (argument.starts_with("--max-fixed-updates="))
            timing.max_fixed_updates = number(argument.substr(std::string_view("--max-fixed-updates=").size()));
        else if (argument.starts_with("--recovery-prefix="))
            timing.fixed_updates_before_recovery = number(argument.substr(std::string_view("--recovery-prefix=").size()));
        else if (argument.starts_with("--recovery-cap-ms="))
            timing.recovery_cap = std::chrono::milliseconds(number(argument.substr(std::string_view("--recovery-cap-ms=").size())));
        else if (argument.starts_with("--input-capacity=")) {
            polling.policy = CE::Input::PollingPolicy::Finite;
            polling.capacity = number(argument.substr(std::string_view("--input-capacity=").size()));
        } else if (argument.starts_with("--input-spacing-ms="))
            polling.spacing = std::chrono::milliseconds(number(argument.substr(std::string_view("--input-spacing-ms=").size())));
        else
            asset_root = argv[i];
    }
    auto engine = CE::Engine::make_glfw_opengl_context();
    Game game(*engine, asset_root, load_all_assets);
    GameRuntime game_runtime(*engine, game, mode, polling, timing);
    if (max_updates != 0)
        game.stop_after_updates(max_updates, [&game_runtime] { game_runtime.stop(); });
    game_runtime.run();
    if (max_updates != 0)
        std::cout << "Completed " << game.completed_updates() << " demo updates\n";
}
