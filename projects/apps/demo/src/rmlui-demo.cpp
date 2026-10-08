#include "rmlui-demo.h"

#include <backends/opengl/resource-provider.h>
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
    CE::UI::RmlUi::Materials materials(CE::Assets::ResourceProvider& provider, const std::filesystem::path& root) {
        using namespace CE::Assets;
        auto* native = dynamic_cast<OpenGLResourceProvider*>(&provider);
        if (!native)
            throw CE::Exceptions::invalid_args(CE_HERE, "The demo UI requires an OpenGL material provider");
        PipelineDefinition definition;
        definition.vertex_layout = VertexLayout2D::Position3UV2Color4;
        definition.state = {BlendMode::PremultipliedAlpha, DepthMode::Disabled, false, CullMode::None};
        definition.parameters = {{"projection", ParameterType::Mat4, true, ParameterSemantic::Projection}};
        // These application shaders multiply vertex/image RGBA for both toolkits.
        // RmlUi's pipeline preserves its premultiplied output through blending.
        definition.program_sources = {root / "shaders/tgui.vert", root / "shaders/tgui-solid.frag"};
        const GLSLPipelineBindings solid_bindings{{{"projection", "projectionMatrix"}}};
        CE::UI::RmlUi::Materials result;
        result.solid = native->build_material({native->build_pipeline(definition, solid_bindings), {}});
        definition.program_sources[1] = root / "shaders/tgui-textured.frag";
        definition.parameters.push_back({"image", ParameterType::Sampler2D});
        const GLSLPipelineBindings textured_bindings{{{"projection", "projectionMatrix"}, {"image", "mytexture"}}};
        result.textured = native->build_material({native->build_pipeline(std::move(definition), textured_bindings), {}});
        return result;
    }

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

struct DemoRmlUi::State {
    CE::Input::iInputSystem& input;
    CE::Engine::PlatformDispatcher::Submission platform;
    CE::UI::RmlUi::SceneUploader uploader;
    CE::UI::RmlUi::Materials materials;
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

    State(CE::Engine::EngineContext& engine, const std::filesystem::path& root, std::filesystem::path font)
    : input(engine.input()),
      platform(engine.platform_dispatcher().submission()),
      uploader(engine.resources()),
      materials(::materials(engine.resources(), root)),
      root(root),
      font(std::move(font)) {}

    ~State() {
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

    void create_widgets(const CE::GFramework::TickContext& tick) {
        session = std::make_unique<CE::UI::RmlUi::Session>(input, 3);
        if (!session->load_font(font, "demo"))
            throw CE::Exceptions::failed_operation(CE_HERE, "Cannot load the demo RmlUi font");
        session->set_view(tick.logical_size, tick.framebuffer_size);
        const auto source = (root / "ui/demo.rml").u8string();
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
            picture->SetAttribute("src", std::string{alternate_image ? "quadrants-alt.png" : "quadrants.png"});
        });
        element(*document, "reset").AddEventListener("click", reset_callback.get());
        element(*document, "change-image").AddEventListener("click", image_callback.get());
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    }

    bool inside_panel(const CE::Input::PointerEvent& position) const {
        auto* hit = session->context().GetElementAtPoint({static_cast<float>(position.x), static_cast<float>(position.y)});
        for (; hit; hit = hit->GetParentNode())
            if (hit == panel)
                return true;
        return false;
    }

    void handle_input(const CE::GFramework::TickContext& tick) {
        using namespace CE::Input;
        session->handle_input(tick.input.records(), visible, [&](const InputRecord& record) {
            const auto* button = std::get_if<ButtonEvent>(&record.data);
            if (button && record.device_kind == DeviceKind::Keyboard && button->phase == ButtonPhase::Press) {
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
                button->phase == ButtonPhase::Press && button->position) {
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

DemoRmlUi::DemoRmlUi(CE::Engine::EngineContext& engine, const std::filesystem::path& asset_root, std::filesystem::path font)
: state_(std::make_unique<State>(engine, asset_root, std::move(font))) {}

DemoRmlUi::~DemoRmlUi() = default;

bool DemoRmlUi::update(const CE::GFramework::TickContext& tick, const DemoUiStatus& status) {
    auto& state = *state_;
    if (!state.session)
        state.create_widgets(tick);
    else
        state.session->set_view(tick.logical_size, tick.framebuffer_size);
    state.session->update_time(tick.delta_seconds);
    state.handle_input(tick);
    state.status_label->SetInnerRML(
        std::format("Updates: {}<br/>Camera: {:.0f}, {:.0f} — Clicks: {}", status.updates, status.pan_x, status.pan_y, status.clicks)
    );
    try {
        if (CE::UI::RmlUi::adopt_scene(state.pending, state.scene))
            state.upload_error.clear();
        if (!state.pending.valid())
            state.pending = state.uploader.submit(state.platform, state.session->record(), state.materials);
    } catch (const std::exception& error) {
        state.upload_error = error.what();
    }
    return std::exchange(state.reset_requested, false);
}

void DemoRmlUi::write(CE::RenderAPIs::RenderFrameWriter& frame) const {
    if (state_->visible)
        state_->scene.write(frame);
}
std::string_view DemoRmlUi::error() const {
    return state_->upload_error;
}
