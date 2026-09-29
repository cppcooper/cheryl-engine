#include <assets/types/2d/stbfont.h>
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
#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/fileio/fonts-system.h>
#include <internals/exceptions.h>

#include <ext/matrix_transform.hpp>
#include <gainput/gainput.h>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <format>
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
} // namespace DemoActions

class Game : public CE::GFramework::AbstractGame {
public:
    Game(CE::Engine::EngineContext& engine, std::filesystem::path asset_root, bool load_all_assets)
        : engine_(engine), asset_root_(std::move(asset_root)), load_all_assets_(load_all_assets) {}

    void init() override {
        camera_.set_framebuffer_size(engine_.window().framebuffer_size());

        const auto shader2d = asset_root_ / "shaders" / "shader2d";
        auto& resources = engine_.resources();
        if (load_all_assets_) {
            CE::Assets::Loader::get(asset_root_).load_assets(resources);
        }
        else {
            const auto font_path = CE::Resources::select_default_system_font(CE::Resources::find_system_fonts());
            if (!font_path)
                throw CE::Exceptions::runtime_exception(CE_HERE, "No supported system font was found");
            CE::Assets::FontMgr::get().load_assets({*font_path}, resources);
            CE::Assets::ShaderMgr::get().load_program(shader2d, {shader2d.string() + ".vert", shader2d.string() + ".frag"}, resources);
        }
        font_ = std::dynamic_pointer_cast<CE::Assets::STBFont>(CE::Assets::FontMgr::get().default_font());
        font_shader_ = CE::Assets::ShaderMgr::get().get_asset(shader2d);
        if (!font_)
            throw CE::Exceptions::runtime_exception(CE_HERE, "No supported system font was found");
        if (!font_shader_)
            throw CE::Exceptions::runtime_exception(CE_HERE, "The shader2d program was not loaded");

        auto& input = engine_.input();
        auto& bindings = input.bindings();
        const auto keyboard = input.keyboard_id();
        (void)bindings.bind_button({keyboard, gainput::KeyW}, DemoActions::Up);
        (void)bindings.bind_button({keyboard, gainput::KeyA}, DemoActions::Left);
        (void)bindings.bind_button({keyboard, gainput::KeyS}, DemoActions::Down);
        (void)bindings.bind_button({keyboard, gainput::KeyD}, DemoActions::Right);
        (void)bindings.bind_button({keyboard, gainput::KeyR}, DemoActions::Reset);

        const auto mouse = input.mouse_id();
        (void)bindings.bind_axis({mouse, gainput::MouseAxisX}, DemoActions::MouseX);
        (void)bindings.bind_axis({mouse, gainput::MouseAxisY}, DemoActions::MouseY);
        (void)bindings.bind_button({mouse, gainput::MouseButtonLeft}, DemoActions::Click);
        (void)bindings.bind_axis({mouse, CE::Input::MouseControl::ScrollY}, DemoActions::WheelY,
                                 {1.0f, 0.0f, CE::Input::AxisKind::Relative});
        (void)bindings.bind_button({input.gamepad_id(), gainput::PadButtonA}, DemoActions::GamepadA);
        events_ = input.capture(CE::Input::InputMode::Events);
    }

    void deinit() override {
        focus_.reset();
        text_capture_.reset();
        events_.reset();
        engine_.input().bindings().clear();
        font_.reset();
        font_shader_.reset();
    }

    void update(const CE::GFramework::TickContext& tick) override {
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
            if (record.device_kind == CE::Input::DeviceKind::Keyboard && button && button->button == gainput::KeyF2 &&
                button->phase == CE::Input::ButtonPhase::Press) {
                if (focus_.owns_focus()) {
                    focus_.reset();
                    text_capture_.reset();
                }
                else {
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
            }
            else if (button && button->phase != CE::Input::ButtonPhase::Release) {
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
        }

        const glm::vec2 movement{static_cast<float>(actions.button(DemoActions::Right).down_duration.count() -
                                                    actions.button(DemoActions::Left).down_duration.count()),
                                 static_cast<float>(actions.button(DemoActions::Up).down_duration.count() -
                                                    actions.button(DemoActions::Down).down_duration.count())};
        if (glm::length(movement) > 0.0f) {
            // Movement uses observed down-time; a completed tap still moves even
            // though the current button state is released at this update.
            pan_ += movement * 240.0f;
            camera_.set_view_matrix(glm::translate(glm::mat4(1.0f), glm::vec3(-pan_, 0.0f)));
        }
    }

    void prepare_render_frame(CE::RenderAPIs::RenderFrameWriter& frame) const override {
        const auto size = camera_.framebuffer_size();
        auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
        pass.reserve_draws(2);
        CE::RenderAPIs::DrawStyle text;
        text.material = font_shader_;
        text.model_matrix = glm::translate(
            glm::mat4(1.0f), glm::vec3(static_cast<float>(size.width) * 0.5f - 120.0f, static_cast<float>(size.height) * 0.5f, 0.0f));
        pass.add(CE::RenderAPIs::TextDraw{font_, "Camera target", text});

        // Compensate for the view translation so these controls stay fixed on screen.
        text.model_matrix =
            glm::translate(glm::mat4(1.0f), glm::vec3(pan_.x + 24.0f, pan_.y + static_cast<float>(size.height) - 56.0f, 0.0f));
        pass.add(CE::RenderAPIs::TextDraw{font_,
                                          std::format("Cheryl Engine demo\nWASD: pan camera  R: reset\n"
                                                      "Mouse: {:.2f}, {:.2f}  Clicks: {}  Wheel: {:.2f}\nGamepad A: {} presses\n"
                                                      "F2: text focus  Enter/Esc: leave  Arrows/Home/End: caret\nText [{}]: {}",
                                                      mouse_x_, mouse_y_, clicks_, wheel_, gamepad_presses_,
                                                      focus_.owns_focus() ? "focused" : "unfocused", text_preview()),
                                          text});
    }

private:
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

    static constexpr CE::Input::FocusId text_box = 1;
    CE::Input::CaptureLease events_;
    CE::Input::CaptureLease text_capture_;
    CE::Input::FocusLease focus_;
    std::u32string text_;
    std::size_t caret_ = 0;
    CE::Engine::EngineContext& engine_;
    CE::Camera2D camera_;
    std::filesystem::path asset_root_;
    bool load_all_assets_;
    std::shared_ptr<CE::Assets::STBFont> font_;
    std::shared_ptr<CE::Assets::Shader> font_shader_;
    glm::vec2 pan_{0.0f, 0.0f};
    float mouse_x_ = 0.0f;
    float mouse_y_ = 0.0f;
    std::uint64_t clicks_ = 0;
    double wheel_ = 0.0;
    std::uint64_t gamepad_presses_ = 0;
};

using CE::GFramework::GameRuntime;

int main(const int argc, char** argv) {
    std::filesystem::path asset_root = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets";
    bool load_all_assets = false;
    auto mode = CE::GFramework::RunMode::Sequential;
    CE::Input::PollingOptions polling;
    const auto number = [](const std::string_view text) {
        unsigned int value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size())
            throw CE::Exceptions::invalid_args(CE_HERE, "Input polling options require a nonnegative integer");
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
        else if (argument.starts_with("--input-capacity=")) {
            polling.policy = CE::Input::PollingPolicy::Finite;
            polling.capacity = number(argument.substr(std::string_view("--input-capacity=").size()));
        }
        else if (argument.starts_with("--input-spacing-ms="))
            polling.spacing = std::chrono::milliseconds(number(argument.substr(std::string_view("--input-spacing-ms=").size())));
        else
            asset_root = argv[i];
    }
    auto engine = CE::Engine::make_glfw_opengl_context(CE::Input::InputSystem::get());
    Game game(*engine, asset_root, load_all_assets);
    GameRuntime game_runtime(*engine, game, mode, polling);
    game_runtime.run();
}
