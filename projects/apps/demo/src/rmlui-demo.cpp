#include "rmlui-demo.h"

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
    CE::Input::System& input;
    CE::Runtime::PlatformDispatcher::Submission platform;
    CE::UI::RmlUi::SceneUploader uploader;
    const std::filesystem::path root;
    const std::filesystem::path font;
    std::unique_ptr<CE::UI::RmlUi::Session> session;
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

    RmlUiState(CE::Runtime::Context& engine, const std::filesystem::path& root, std::filesystem::path font)
    : input(engine.input()),
      platform(engine.platform_dispatcher().submission()),
      uploader(engine.resources()),
      root(root),
      font(std::move(font)) {}

    ~RmlUiState() {
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

    void create_widgets(const CE::Game::TickContext& tick) {
        session = std::make_unique<CE::UI::RmlUi::Session>(input, 3);
        if (!session->load_font(font, "demo"))
            throw CE::Exceptions::failed_operation(CE_HERE, "Cannot load the demo RmlUi font");
        session->set_view(tick.logical_size, tick.framebuffer_size);
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
        reset_callback = std::make_unique<Callback>([this] { reset_requested = true; });
        image_callback = std::make_unique<Callback>([this] {
            alternate_image = !alternate_image;
            picture->SetAttribute(
                "src", std::string{alternate_image ? "../textures/ui/quadrants-alt.png" : "../textures/ui/quadrants.png"}
            );
        });
        element(*document, "reset").AddEventListener("click", reset_callback.get());
        element(*document, "change-image").AddEventListener("click", image_callback.get());
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    }

    bool inside_panel(const CE::Input::Events::Pointer& position) const {
        auto* hit = session->context().GetElementAtPoint({static_cast<float>(position.x), static_cast<float>(position.y)});
        for (; hit; hit = hit->GetParentNode())
            if (hit == panel)
                return true;
        return false;
    }

    void handle_input(const CE::Game::TickContext& tick) {
        using namespace CE::Input;
        session->handle_input(tick.input.records(), visible, [&](const CE::Input::Events::Record& record) {
            const auto* button = std::get_if<CE::Input::Events::Button>(&record.data);
            if (button && record.device_kind == DeviceKind::Keyboard && button->phase == CE::Input::Events::ButtonPhase::Press) {
                if (button->key == KeyboardKey::F4 && visible) {
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
                        session->release_keyboard_focus();
                    }
                } else if (button->key == KeyboardKey::Escape && session->owns_keyboard_focus())
                    session->release_keyboard_focus();
            }
            if (visible && tick.logical_size.width > 0 && tick.logical_size.height > 0 && button &&
                record.device_kind == DeviceKind::Mouse && button->mouse_button == MouseButton::Left &&
                button->phase == CE::Input::Events::ButtonPhase::Press && button->position) {
                if (inside_panel(*button->position)) {
                    if (!session->owns_keyboard_focus())
                        session->request_keyboard_focus();
                } else
                    session->release_keyboard_focus();
            }
            return visible;
        });
    }
};

DemoRmlUi::DemoRmlUi(CE::Runtime::Context& engine, const std::filesystem::path& asset_root, std::filesystem::path font)
: rmlui_state_(std::make_unique<RmlUiState>(engine, asset_root, std::move(font))) {}

DemoRmlUi::~DemoRmlUi() = default;

bool DemoRmlUi::update(const CE::Game::TickContext& tick, const DemoUiStatus& status) {
    auto& rmlui = *rmlui_state_;
    if (!rmlui.session)
        rmlui.create_widgets(tick);
    else
        rmlui.session->set_view(tick.logical_size, tick.framebuffer_size);
    rmlui.session->update_time(tick.delta_seconds);
    rmlui.handle_input(tick);
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
    return std::exchange(rmlui.reset_requested, false);
}

void DemoRmlUi::write(CE::Rendering::FrameWriter& frame) const {
    if (rmlui_state_->visible)
        rmlui_state_->scene.write(frame);
}
std::string_view DemoRmlUi::error() const {
    return rmlui_state_->upload_error;
}
