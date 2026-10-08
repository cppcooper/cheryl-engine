#include <assets/types/2d/unicode-text.h>
#include <assets/submission/draw2d.h>
#include <backends/opengl/resource-provider.h>
#include <backends/opengl/startup.h>
#include <core/controls/input-interface.h>
#include <core/display/window-interface.h>
#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/game-framework/game-runtime.h>
#include <core/rendering/camera.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/material-mgr.h>
#include <internals/exceptions.h>

#include <ext/matrix_transform.hpp>
#include <gainput/gainput.h>

#include "asset-demo.h"
#include "demo-options.h"

#ifdef CHERYL_DEMO_TGUI
#include "tgui-demo.h"
#endif
#ifdef CHERYL_DEMO_RMLUI
#include "rmlui-demo.h"
#endif

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <exception>
#include <format>
#include <future>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
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
    constexpr CE::Input::ActionId ToggleSamples{14};
    constexpr CE::Input::ActionId PauseAnimations{15};
    constexpr CE::Input::ActionId ReplayAttack{16};
} // namespace DemoActions

class Game : public CE::GFramework::AbstractGame {
    struct Materials {
        std::shared_ptr<const CE::Assets::Material> text;
        std::shared_ptr<const CE::Assets::Material> images;
    };

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
    DemoAssets assets_;
    std::filesystem::path asset_root_;
    bool load_all_assets_;
    CE::Text::FontSelection font_selection_;
    CE::Text::LayoutOptions text_options_;
    bool unicode_preview_;
    std::optional<CE::Text::FontCollection> fonts_;
    std::optional<CE::Engine::WorkerGroup> text_workers_;
    std::shared_ptr<const CE::Assets::RenderedText> target_text_;
    std::shared_ptr<const CE::Assets::RenderedText> hud_text_;
    std::future<CE::Assets::PreparedText> pending_text_preparation_;
    std::future<std::shared_ptr<const CE::Assets::RenderedText>> pending_text_upload_;
    std::string requested_hud_;
    float requested_width_ = 0;
    std::shared_ptr<const CE::Assets::Material> font_shader_;
    std::shared_ptr<const CE::Assets::Material> image_shader_;
    std::future<Materials> pending_shader_;
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
    Game(
        CE::Engine::EngineContext& engine,
        std::filesystem::path asset_root,
        bool load_all_assets,
        CE::Text::FontSelection fonts,
        CE::Text::LayoutOptions text_options,
        bool unicode_preview
    )
    : engine_(engine), asset_root_(std::move(asset_root)), load_all_assets_(load_all_assets), font_selection_(std::move(fonts)),
      text_options_(std::move(text_options)), unicode_preview_(unicode_preview) {}

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
            try {
                CE::Assets::Loader loader(asset_root_);
                loader.load_assets(resources);
            } catch (const std::exception& error) {
                std::cerr << "Optional full asset load failed; continuing with available demo samples: " << error.what() << '\n';
            }
        }
        // Font bytes/layout are CPU values; initial uploads run on this platform owner.
        fonts_.emplace(CE::Text::FontCollection::load(font_selection_));
        text_workers_.emplace(engine_.make_worker_group({.max_concurrency = 1}));
        target_text_ = std::make_shared<const CE::Assets::RenderedText>(CE::Assets::upload_text(
            CE::Assets::prepare_text(CE::Text::layout_text(*fonts_, "Camera target", text_options_)), resources
        ));
        auto& materials = CE::Assets::MaterialMgr::get();
        materials.load_material(shader2d, resources, material_recipe(shader2d, CE::Assets::PrimitiveTopology::Triangles));
        font_shader_ = materials.get_asset(shader2d);
        // The same shader sources need a separate material/cache key for the
        // four-vertex tile/sprite strips; text retains its triangle pipeline.
        const auto image_key = std::filesystem::path(shader2d.string() + "-strips");
        materials.load_material(image_key, resources, material_recipe(shader2d, CE::Assets::PrimitiveTopology::TriangleStrip));
        image_shader_ = materials.get_asset(image_key);
        assets_.load(asset_root_, resources, *fonts_);

        auto& input = engine_.input();
        auto& bindings = input.bindings();
        const auto keyboard = input.keyboard_id();
        (void)bindings.bind_button({keyboard, gainput::KeyW}, DemoActions::Up);
        (void)bindings.bind_button({keyboard, gainput::KeyA}, DemoActions::Left);
        (void)bindings.bind_button({keyboard, gainput::KeyS}, DemoActions::Down);
        (void)bindings.bind_button({keyboard, gainput::KeyD}, DemoActions::Right);
        (void)bindings.bind_button({keyboard, gainput::KeyR}, DemoActions::Reset);
        (void)bindings.bind_button({keyboard, gainput::KeyQ}, DemoActions::QuitGame);
        (void)bindings.bind_button({keyboard, gainput::KeyF7}, DemoActions::ToggleSamples);
        (void)bindings.bind_button({keyboard, gainput::KeyP}, DemoActions::PauseAnimations);
        (void)bindings.bind_button({keyboard, gainput::KeySpace}, DemoActions::ReplayAttack);

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
        // RmlUi retains its separate file-based font service.
        auto toolkit_font = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets/fonts/DejaVuSans.ttf";
        for (const auto& face : fonts_->faces()) {
            if (face.path) {
                toolkit_font = *face.path;
                break;
            }
        }
        rml_ui_ = std::make_unique<DemoRmlUi>(engine_, asset_root_, std::move(toolkit_font));
#endif
        requested_hud_ = hud_message();
        auto options = hud_options();
        requested_width_ = *options.maximum_width;
        hud_text_ = std::make_shared<const CE::Assets::RenderedText>(CE::Assets::upload_text(
            CE::Assets::prepare_text(CE::Text::layout_text(*fonts_, requested_hud_, options)), resources
        ));
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
        pending_text_preparation_ = {};
        pending_text_upload_ = {};
        hud_text_.reset();
        target_text_.reset();
        assets_.reset();
        text_workers_.reset();
        fonts_.reset();
        pending_shader_ = {};
        image_shader_.reset();
        font_shader_.reset();
    }

    void update(const CE::GFramework::TickContext& tick) override {
        if (tick.input.button(DemoActions::QuitGame).held()) {
            tick.request_stop();
        }
        if (pending_shader_.valid() && pending_shader_.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
            try {
                auto materials = pending_shader_.get();
                font_shader_ = std::move(materials.text);
                image_shader_ = std::move(materials.images);
                reload_error_.clear();
            } catch (const std::exception& error) {
                reload_error_ = error.what(); // Keep both previous material generations.
            }
        }
        const auto& actions = tick.input;
        camera_.set_framebuffer_size(tick.framebuffer_size);
        if (actions.button(DemoActions::Reset).pressed()) {
            pan_ = {0.0f, 0.0f};
            camera_.set_view_matrix(glm::mat4(1.0f));
        }
        if (actions.button(DemoActions::ToggleSamples).pressed())
            assets_.toggle_visible();
        if (actions.button(DemoActions::PauseAnimations).pressed())
            assets_.toggle_pause();
        if (actions.button(DemoActions::ReplayAttack).pressed())
            assets_.replay_attack();
        assets_.advance(std::chrono::duration<double>{tick.delta_seconds});
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
                const auto image_key = std::filesystem::path(key.string() + "-strips");
                const auto text_builder = material_recipe(key, CE::Assets::PrimitiveTopology::Triangles);
                const auto image_builder = material_recipe(key, CE::Assets::PrimitiveTopology::TriangleStrip);
                pending_shader_ = engine_.platform_dispatcher().submit(
                    [key, image_key, text_builder, image_builder](CE::Engine::EngineContext& platform) {
                        auto& materials = CE::Assets::MaterialMgr::get();
                        materials.reload_material(key, platform.resources(), text_builder);
                        materials.reload_material(image_key, platform.resources(), image_builder);
                        return Materials{materials.get_asset(key), materials.get_asset(image_key)};
                    }
                );
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
        refresh_hud();
        if (++updates_ == update_limit_ && stop_)
            stop_();
    }

    void prepare_render_frame(CE::RenderAPIs::RenderFrameWriter& frame) const override {
        const auto size = camera_.framebuffer_size();
        auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
        const CE::Assets::SubmissionContext2D context{
            pass.semantics(), pass.parameters(), pass.constraints(), CE::Assets::ImageParameter2D{"image", 0}};
        CE::RenderAPIs::DrawStyle2D text;
        text.material = font_shader_;
        CE::RenderAPIs::DrawStyle2D images;
        images.material = image_shader_;
        assets_.write(pass, images, text, context);
        text.model_matrix = glm::translate(
            glm::mat4(1.0f), glm::vec3(static_cast<float>(size.width) * 0.5f - 120.0f, static_cast<float>(size.height) * 0.5f, 0.0f)
        );
        pass.add(CE::Assets::resolve_text(*target_text_, text, context));

        // Compensate for the view translation so these controls stay fixed on screen.
        text.model_matrix =
            glm::translate(glm::mat4(1.0f), glm::vec3(pan_.x + 24.0f, pan_.y + static_cast<float>(size.height) - 56.0f, 0.0f));
        pass.add(CE::Assets::resolve_text(*hud_text_, text, context));
#ifdef CHERYL_DEMO_TGUI
        ui_->write(frame);
#endif
#ifdef CHERYL_DEMO_RMLUI
        rml_ui_->write(frame);
#endif
    }

private:
    [[nodiscard]] std::string hud_message() const {
        auto hud = std::format(
            "Cheryl Engine demo\nWASD: pan camera  R: reset  F5: reload shader\n"
            "Mouse: {:.2f}, {:.2f}  Clicks: {}  Wheel: {:.2f}\nGamepad A: {} presses\n"
            "Esc: release focus  Q: quit while gameplay has focus\nReload: {}",
            mouse_x_, mouse_y_, clicks_, wheel_, gamepad_presses_, reload_error_
        );
        hud += '\n' + assets_.status();
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
        if (unicode_preview_) {
            constexpr std::u8string_view samples = u8"\nFrench: Fran\u00e7ais, d\u00e9j\u00e0 vu / e\u0301"
                u8"\nGerman: Gr\u00fc\u00dfe, Stra\u00dfe\nRussian: \u041f\u0440\u0438\u0432\u0435\u0442, \u043c\u0438\u0440!"
                u8"\nMixed RTL: \u05d0\u05d1\u05d2 123 English\n\u05d0\u05d1\u05d2 123"
                u8"\nMissing glyph: \u4e2d\nWrap: This sentence follows the available width when the window is resized.";
            hud.append(reinterpret_cast<const char*>(samples.data()), samples.size());
        }
        return hud;
    }

    [[nodiscard]] CE::Text::LayoutOptions hud_options() const {
        auto options = text_options_;
        options.maximum_width = std::max(1.0f, static_cast<float>(camera_.framebuffer_size().width) - 48.0f);
        return options;
    }

    void refresh_hud() {
        // Only owned values cross worker/platform boundaries. One replacement at a
        // time coalesces changing counters/text without blocking simulation.
        try {
            if (pending_text_upload_.valid() &&
                pending_text_upload_.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
                hud_text_ = pending_text_upload_.get();
            if (pending_text_preparation_.valid() &&
                pending_text_preparation_.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
                pending_text_upload_ = engine_.platform_dispatcher().submit(
                    [prepared = pending_text_preparation_.get()](CE::Engine::EngineContext& platform) mutable {
                        return std::make_shared<const CE::Assets::RenderedText>(
                            CE::Assets::upload_text(std::move(prepared), platform.resources()));
                    }
                );
            }
        } catch (const std::exception& error) {
            std::cerr << "Text replacement failed; keeping the previous text: " << error.what() << '\n';
        }
        if (pending_text_preparation_.valid() || pending_text_upload_.valid())
            return;
        auto message = hud_message();
        auto options = hud_options();
        if (message == requested_hud_ && *options.maximum_width == requested_width_)
            return;
        requested_hud_ = message;
        requested_width_ = *options.maximum_width;
        try {
            pending_text_preparation_ = text_workers_->submit(
                [fonts = *fonts_, message = std::move(message), options = std::move(options)] {
                    return CE::Assets::prepare_text(CE::Text::layout_text(fonts, message, options));
                }
            );
        } catch (const std::exception& error) {
            std::cerr << "Text preparation failed; keeping the previous text: " << error.what() << '\n';
        }
    }

    CE::Assets::MaterialMgr::Builder material_recipe(const std::filesystem::path& key, const CE::Assets::PrimitiveTopology topology) const {
        return [key, topology](CE::Assets::ResourceProvider& provider) {
            using namespace CE::Assets;
            auto* native = dynamic_cast<OpenGLResourceProvider*>(&provider);
            if (!native)
                throw CE::Exceptions::invalid_args(CE_HERE, "The GLFW demo requires an OpenGL material provider");
            PipelineDefinition definition;
            definition.program_sources = {key.string() + ".vert", key.string() + ".frag"};
            definition.topology = topology;
            definition.parameters = {{"projection", ParameterType::Mat4, true, ParameterSemantic::Projection},
                {"view", ParameterType::Mat4, true, ParameterSemantic::View},
                {"model", ParameterType::Mat4, true, ParameterSemantic::Model},
                {"alpha", ParameterType::Float, true, ParameterSemantic::Alpha},
                {"scale", ParameterType::Float, true, ParameterSemantic::Scale}, {"image", ParameterType::Sampler2D}};
            const GLSLPipelineBindings bindings{{{"projection", "projectionMatrix"}, {"view", "viewMatrix"}, {"model", "modelMatrix"},
                {"alpha", "in_Alpha"}, {"scale", "in_Scale"}, {"image", "mytexture"}}};
            return native->build_material({native->build_pipeline(std::move(definition), bindings), {}});
        };
    }

#if !defined(CHERYL_DEMO_TGUI) && !defined(CHERYL_DEMO_RMLUI)
    [[nodiscard]] std::string text_preview() const {
        // The probe still edits logical scalars; display encoding does not claim
        // grapheme-aware caret movement, bidi selection, clipboard or IME.
        std::string preview;
        for (std::size_t i = 0; i <= text_.size(); ++i) {
            if (i == caret_)
                preview += '|';
            if (i == text_.size())
                continue;
            auto scalar = static_cast<std::uint32_t>(text_[i]);
            if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
                scalar = 0xfffd;
            if (scalar < 0x80) {
                preview += static_cast<char>(scalar);
            } else if (scalar < 0x800) {
                preview += static_cast<char>(0xc0 | (scalar >> 6));
                preview += static_cast<char>(0x80 | (scalar & 0x3f));
            } else if (scalar < 0x10000) {
                preview += static_cast<char>(0xe0 | (scalar >> 12));
                preview += static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
                preview += static_cast<char>(0x80 | (scalar & 0x3f));
            } else {
                preview += static_cast<char>(0xf0 | (scalar >> 18));
                preview += static_cast<char>(0x80 | ((scalar >> 12) & 0x3f));
                preview += static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
                preview += static_cast<char>(0x80 | (scalar & 0x3f));
            }
        }
        return preview;
    }
#endif
};

int main(const int argc, char** argv) {
    DemoOptions options;
    auto startup = CE::Engine::make_glfw_opengl_startup("Cheryl demo");
    options.register_with(startup);
    auto result = startup.initialize(argc, argv);
    if (!result.should_start())
        return result.exit_code;
    Game game(
        *result.engine,
        options.asset_root,
        options.load_all_assets,
        std::move(options.font_selection),
        std::move(options.text_options),
        options.unicode_preview
    );
    auto game_runtime = result.make_runtime(game);
    if (options.max_updates != 0)
        game.stop_after_updates(options.max_updates, [&game_runtime] { game_runtime.stop(); });
    game_runtime.run();
    if (options.max_updates != 0)
        std::cout << "Completed " << game.completed_updates() << " demo updates\n";
    return result.exit_code;
}
