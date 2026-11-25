#include "rmlui-demo.h"
#include "ui-routing.h"
#ifdef CHERYL_DEMO_CONSOLE
#include "developer-console.h"
#include "developer-console-model.h"
#include "developer-console-routing.h"
#endif

#include <core/engine/engine-context.h>
#include <internals/exceptions.h>
#include <ui/rmlui/session.h>
#include <ui/rmlui/scene.h>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/EventListener.h>

#include <exception>
#include <format>
#include <functional>
#include <future>
#include <string>
#include <utility>

namespace {

    class Callback final : public Rml::EventListener {
        std::function<void()> action_;

    public:
        explicit Callback(std::function<void()> action)
        : action_(std::move(action)) {}
        void ProcessEvent(Rml::Event&) override { action_(); }
    };

    Rml::Element& element(Rml::ElementDocument& document, const char* id) {
        auto* element = document.GetElementById(id);
        if (!element)
            throw CE::Exceptions::failed_operation(CE_HERE, "The demo RML document is missing element: " + std::string{id});
        return *element;
    }
}

struct DemoRmlUi::RmlUiState {
    CE::Runtime::Context& engine;
    CE::Runtime::PlatformDispatcher::Submission platform;
    CE::UI::RmlUi::SceneUploader uploader;
    const std::filesystem::path root;
    std::unique_ptr<CE::UI::RmlUi::Session> session;
#ifdef CHERYL_DEMO_CONSOLE
    DemoConsole::Application* console;
    DemoConsole::Model* console_model = nullptr;
    std::unique_ptr<DemoConsole::View> console_view;
    std::unique_ptr<DemoConsole::Routing> console_routing;
#endif
    Rml::ElementDocument* document = nullptr;
    Rml::Element* panel = nullptr;
    Rml::Element* status_label = nullptr;
    Rml::ElementFormControlInput* field = nullptr;
    Rml::Element* picture = nullptr;
    std::unique_ptr<Callback> reset_callback;
    std::unique_ptr<Callback> image_callback;
    CE::UI::RmlUi::Scene scene;
    std::future<CE::UI::RmlUi::Scene> pending;
    std::string upload_error;
    bool visible = true;
    bool reset_requested = false;
    bool alternate_image = false;

    RmlUiState(CE::Runtime::Context& engine, const std::filesystem::path& root, DemoConsole::Application* selected_console)
    : engine(engine), platform(engine.platform_dispatcher().submission()), uploader(engine.resources()), root(root)
#ifdef CHERYL_DEMO_CONSOLE
      , console(selected_console)
#endif
    {
        static_cast<void>(selected_console);
    }

    ~RmlUiState() {
#ifdef CHERYL_DEMO_CONSOLE
        console_routing.reset();
        console_view.reset();
        console_model = nullptr;
#endif
        // Runtime has joined simulation. Listeners use SDK observer storage and
        // must detach/die while Core still lives, before final session teardown.
        if (document) {
            if (auto* reset = document->GetElementById("reset"); reset && reset_callback)
                reset->RemoveEventListener("click", reset_callback.get());
            if (auto* image = document->GetElementById("change-image"); image && image_callback)
                image->RemoveEventListener("click", image_callback.get());
        }
        reset_callback.reset();
        image_callback.reset();
        document = nullptr;
        panel = status_label = picture = nullptr;
        field = nullptr;
        if (session)
            session->close_after_quiescence();
    }

    void create_widgets() {
        session = std::make_unique<CE::UI::RmlUi::Session>(engine, 3, CE::UI::RmlUi::SessionOptions{.automatic_view = true});
        const auto source = (root / "graphics/ui/demo.rml").u8string();
        document = session->context().LoadDocument(Rml::String(source.begin(), source.end()));
        if (!document)
            throw CE::Exceptions::failed_operation(CE_HERE, "Cannot load the demo RML document");
        panel = &element(*document, "panel");
        status_label = &element(*document, "status");
        picture = &element(*document, "image");
        field = dynamic_cast<Rml::ElementFormControlInput*>(&element(*document, "field"));
        if (!field)
            throw CE::Exceptions::failed_operation(CE_HERE, "The demo RML field must be a native text input");
#ifndef CHERYL_DEMO_CONSOLE
        reset_callback = std::make_unique<Callback>([this] { reset_requested = true; });
        image_callback = std::make_unique<Callback>([this] {
            alternate_image = !alternate_image;
            picture->SetAttribute(
                "src", std::string{alternate_image ? "../textures/ui/quadrants-alt.png" : "../textures/ui/quadrants.png"}
            );
        });
        element(*document, "reset").AddEventListener("click", reset_callback.get());
        element(*document, "change-image").AddEventListener("click", image_callback.get());
#endif
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
#ifdef CHERYL_DEMO_CONSOLE
        if (console) {
            console_model = &console->model();
            console_view = std::make_unique<DemoConsole::View>(*session, *console_model);
        }
        console_routing = std::make_unique<DemoConsole::Routing>(*session, *document, console_view.get());
#endif
    }

    bool console_visible() const {
#ifdef CHERYL_DEMO_CONSOLE
        return console_view && console_view->visible();
#else
        return false;
#endif
    }

    bool original_visible() const {
#ifdef CHERYL_DEMO_CONSOLE
        return console_routing && console_routing->original_visible();
#else
        return visible;
#endif
    }

    bool inside_panel(const CE::Input::Events::Pointer& position) const {
        auto* hit = session->context().GetElementAtPoint({static_cast<float>(position.x), static_cast<float>(position.y)});
        for (; hit; hit = hit->GetParentNode())
            if (hit == panel)
                return true;
        return false;
    }

    bool handle_input(const CE::Game::TickContext& tick, DemoTguiControls* tgui) {
        using namespace CE::Input;
        const bool drawable = DemoUiRouting::drawable(tick.logical_size, tick.framebuffer_size);
#ifdef CHERYL_DEMO_CONSOLE
        return console_routing->handle_input(tick.input.records(), drawable, tgui);
#else
        static_cast<void>(tgui);
        session->handle_input(tick.input.records(), drawable && visible, [&](const CE::Input::Events::Record& record) {
            const auto* button = std::get_if<CE::Input::Events::Button>(&record.data);
            if (button && record.device_kind == DeviceKind::Keyboard && button->phase == CE::Input::Events::ButtonPhase::Press) {
                if (button->key == KeyboardKey::F4 && visible && drawable) {
                    if (session->owns_keyboard_focus() && session->context().GetFocusElement() == field)
                        session->release_keyboard_focus();
                    else {
                        if (!session->owns_keyboard_focus())
                            session->request_keyboard_focus();
                        if (!field->Focus(true)) {
                            session->release_keyboard_focus();
                            throw CE::Exceptions::failed_operation(CE_HERE, "Cannot focus the demo RmlUi text input");
                        }
                    }
                } else if (button->key == KeyboardKey::F6) {
                    visible = !visible;
                    if (visible)
                        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
                    else {
                        document->Hide();
                        if (!console_visible())
                            session->release_keyboard_focus();
                    }
                } else if (button->key == KeyboardKey::Escape) {
                    session->release_keyboard_focus();
                }
            }
            if (drawable && (visible || console_visible()) && button &&
                record.device_kind == DeviceKind::Mouse && button->mouse_button == MouseButton::Left &&
                button->phase == CE::Input::Events::ButtonPhase::Press && button->position) {
                if (inside_panel(*button->position)) {
                    if (!session->owns_keyboard_focus())
                        session->request_keyboard_focus();
                } else {
                    session->release_keyboard_focus();
                }
            }
            return drawable && (visible || console_visible());
        });
        return true;
#endif
    }
};

DemoRmlUi::DemoRmlUi(CE::Runtime::Context& engine, const std::filesystem::path& asset_root, DemoConsole::Application* console)
: rmlui_state_(std::make_unique<RmlUiState>(engine, asset_root, console)) {}

DemoRmlUi::~DemoRmlUi() = default;

void DemoRmlUi::init_simulation() {
    rmlui_state_->create_widgets();
}

void DemoRmlUi::stop_console() noexcept {
#ifdef CHERYL_DEMO_CONSOLE
    if (rmlui_state_->console_routing)
        rmlui_state_->console_routing->retire();
    if (rmlui_state_->console_model)
        rmlui_state_->console_model->close();
    if (rmlui_state_->console)
        rmlui_state_->console->stop();
#endif
}

bool DemoRmlUi::process_input(const CE::Game::TickContext& tick, DemoTguiControls* tgui) {
    auto& rmlui = *rmlui_state_;
    rmlui.session->update_time(tick.delta_seconds);
    return rmlui.handle_input(tick, tgui);
}

bool DemoRmlUi::update(const CE::Game::TickContext& tick, const DemoUiStatus& status) {
    static_cast<void>(tick);
    auto& rmlui = *rmlui_state_;
#ifdef CHERYL_DEMO_CONSOLE
    if (rmlui.console_model) {
        rmlui.console_model->pump();
        rmlui.console_view->update();
    }
#endif
    rmlui.status_label->SetInnerRML(
        std::format("Updates: {}<br/>Camera: {:.0f}, {:.0f} — Clicks: {}", status.updates, status.pan_x, status.pan_y, status.clicks)
    );
    try {
        if (CE::UI::RmlUi::adopt_scene(rmlui.pending, rmlui.scene))
            rmlui.upload_error.clear();
        if (!rmlui.pending.valid())
            rmlui.pending = rmlui.uploader.submit(rmlui.platform, rmlui.session->record());
    } catch (const std::exception& error) {
        rmlui.upload_error = error.what();
    }
#ifdef CHERYL_DEMO_CONSOLE
    return rmlui.console_routing->consume_reset();
#else
    return std::exchange(rmlui.reset_requested, false);
#endif
}

void DemoRmlUi::write(CE::Rendering::FrameWriter& frame) const {
    if (rmlui_state_->original_visible() || rmlui_state_->console_visible())
        rmlui_state_->scene.write(frame);
}
std::string_view DemoRmlUi::error() const {
    return rmlui_state_->upload_error;
}
