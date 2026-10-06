#include <ui/rmlui/session.h>
#include <ui/rmlui/input.h>

#include <internals/compile-time-logging.hpp>
#include <internals/exceptions.h>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/SystemInterface.h>

#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace CE::UI::RmlUi {
    namespace {
        std::mutex library_mutex;
        bool library_owned = false;

        class System final : public Rml::SystemInterface {
        public:
            double elapsed = 0;

            double GetElapsedTime() override { return elapsed; }
            bool LogMessage(const Rml::Log::Type type, const Rml::String& message) override {
                if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT)
                    CE_LOG_ERROR(CE::renderlog, "subsystem=rmlui message={}", message);
                else if (type == Rml::Log::LT_WARNING)
                    CE_LOG_WARN(CE::renderlog, "subsystem=rmlui message={}", message);
                else
                    CE_LOG_DEBUG(CE::renderlog, "subsystem=rmlui message={}", message);
                return true;
            }
            void SetMouseCursor(const Rml::String&) override {}
            void SetClipboardText(const Rml::String&) override {
                throw Exceptions::failed_operation(CE_HERE, "RmlUi OS clipboard service is unavailable");
            }
            void GetClipboardText(Rml::String&) override {
                throw Exceptions::failed_operation(CE_HERE, "RmlUi OS clipboard service is unavailable");
            }
            void ActivateKeyboard(Rml::Vector2f, float) override {}
            void DeactivateKeyboard() override {}
        };

        class LibraryLease final {
        public:
            LibraryLease(System& system, RenderTarget& renderer) {
                const std::lock_guard lock(library_mutex);
                if (library_owned || Rml::GetSystemInterface() || Rml::GetRenderInterface() || Rml::GetFileInterface() ||
                    Rml::GetFontEngineInterface() || Rml::GetTextInputHandler())
                    throw Exceptions::failed_operation(CE_HERE, "RmlUi already has an active session or host-installed interfaces");
                Rml::SetSystemInterface(&system);
                Rml::SetRenderInterface(&renderer);
                try {
                    if (!Rml::Initialise())
                        throw Exceptions::failed_operation(
                            CE_HERE, "RmlUi initialization failed; verify the required stock FreeType configuration"
                        );
                } catch (...) {
                    // Core 6.3 cannot Shutdown a partially initialized SDK. Its
                    // dependency contract prevents missing default interfaces;
                    // never leave our failed constructor's storage installed.
                    Rml::SetSystemInterface(nullptr);
                    Rml::SetRenderInterface(nullptr);
                    throw;
                }
                library_owned = true;
            }
            ~LibraryLease() {
                const std::lock_guard lock(library_mutex);
                Rml::Shutdown();
                library_owned = false;
            }
            LibraryLease(const LibraryLease&) = delete;
            LibraryLease& operator=(const LibraryLease&) = delete;
        };

        int coordinate(const double value) {
            const auto rounded = std::floor(value);
            if (!std::isfinite(rounded) || rounded < std::numeric_limits<int>::min() || rounded > std::numeric_limits<int>::max())
                throw Exceptions::invalid_args(CE_HERE, "RmlUi input needs a finite representable logical coordinate");
            return static_cast<int>(rounded);
        }

        void pointer(Rml::Context& context, const Input::PointerEvent& position, const int flags) {
            context.ProcessMouseMove(coordinate(position.x), coordinate(position.y), flags);
        }

        int mouse_button(const Input::MouseButton button) {
            switch (button) {
                case Input::MouseButton::Left:
                    return 0;
                case Input::MouseButton::Right:
                    return 1;
                case Input::MouseButton::Middle:
                    return 2;
                default:
                    return -1;
            }
        }

        bool clipboard_shortcut(const Input::ButtonEvent& event) {
            const bool command = Input::has_modifier(event.modifiers, Input::Modifiers::Control) ||
                                 Input::has_modifier(event.modifiers, Input::Modifiers::Super);
            const bool shift = Input::has_modifier(event.modifiers, Input::Modifiers::Shift);
            using Key = Input::KeyboardKey;
            return (command && (event.key == Key::C || event.key == Key::X || event.key == Key::V || event.key == Key::Insert)) ||
                   (shift && (event.key == Key::Insert || event.key == Key::Delete));
        }
    }

    struct Session::State {
        Input::iInputSystem& input;
        const Input::FocusId target;
        const std::thread::id thread = std::this_thread::get_id();
        const Capabilities capabilities;
        Input::CaptureLease events;
        Input::CaptureLease text;
        Input::FocusLease focus;
        std::uint64_t focus_epoch = 0;
        Input::Modifiers modifier_state = Input::Modifiers::None;
        bool drawable = false;
        bool pointer_view = false;
        bool pointer_active = false;
        // Reverse destruction shuts down contexts/fonts before renderer/system
        // storage and memory-loaded font bytes disappear.
        std::vector<std::vector<unsigned char>> font_bytes;
        System system;
        RenderTarget renderer;
        LibraryLease library;
        Rml::Context* context = nullptr;

        State(Input::iInputSystem& source, const Input::FocusId id, const SessionOptions options)
        : input(source),
          target(id),
          capabilities{source.supports_focus(), source.supports(Input::InputMode::Text)},
          events(source.capture(Input::InputMode::Events)),
          renderer(options.maximum_texture_size),
          library(system, renderer) {
            context = Rml::CreateContext("cheryl-ui", {1, 1});
            if (!context)
                throw Exceptions::failed_operation(CE_HERE, "Cannot create the RmlUi context");
            context->SetDensityIndependentPixelRatio(1);
            context->EnableMouseCursor(false);
        }
    };

    Session::Session(Input::iInputSystem& input, const Input::FocusId target, const SessionOptions options) {
        if (target == 0)
            throw Exceptions::invalid_args(CE_HERE, "RmlUi requires a nonzero focus target");
        if (options.maximum_texture_size < 1024)
            throw Exceptions::invalid_args(CE_HERE, "RmlUi's stock font atlas needs a texture bound of at least 1024");
        state_ = std::make_unique<State>(input, target, options);
    }

    Session::~Session() = default;

    Session::State& Session::owner() const {
        if (!state_ || state_->thread != std::this_thread::get_id())
            throw Exceptions::failed_operation(CE_HERE, "RmlUi calls require the live simulation/UI owner");
        return *state_;
    }

    Rml::Context& Session::context() {
        return *owner().context;
    }
    Capabilities Session::capabilities() const {
        return owner().capabilities;
    }

    bool Session::load_font(const std::filesystem::path& file, const Rml::String& family, const bool fallback) {
        (void)owner();
        if (family.empty())
            throw Exceptions::invalid_args(CE_HERE, "RmlUi font registration needs a family name");
        const auto utf8 = file.u8string();
        return Rml::LoadFontFace(
            Rml::String(utf8.begin(), utf8.end()), family, Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto, fallback
        );
    }

    bool Session::load_font(const std::span<const unsigned char> bytes, const Rml::String& family, const bool fallback) {
        auto& state = owner();
        if (bytes.empty() || family.empty())
            throw Exceptions::invalid_args(CE_HERE, "RmlUi memory font registration needs bytes and a family name");
        state.font_bytes.emplace_back(bytes.begin(), bytes.end());
        const auto& copy = state.font_bytes.back();
        return Rml::LoadFontFace({copy.data(), copy.size()}, family, Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto, fallback);
    }

    void Session::set_view(const ViewPort<int> logical, const FramebufferSize framebuffer) {
        auto& state = owner();
        if (logical.width < 0 || logical.height < 0 || framebuffer.width < 0 || framebuffer.height < 0)
            throw Exceptions::invalid_args(CE_HERE, "RmlUi window dimensions must be nonnegative");
        state.pointer_view = logical.width > 0 && logical.height > 0;
        state.drawable = state.pointer_view && framebuffer.width > 0 && framebuffer.height > 0;
        state.renderer.set_view({logical.width, logical.height});
        state.context->SetDimensions({logical.width, logical.height});
    }

    void Session::update_time(const double delta_seconds) {
        auto& state = owner();
        if (!std::isfinite(delta_seconds) || delta_seconds < 0 || !std::isfinite(state.system.elapsed + delta_seconds))
            throw Exceptions::invalid_args(CE_HERE, "RmlUi timing needs a finite nonnegative simulation duration");
        state.system.elapsed += delta_seconds;
        if (!state.context->Update())
            throw Exceptions::failed_operation(CE_HERE, "RmlUi context update failed");
    }

    void Session::request_keyboard_focus(const Input::KeyboardRouting routing) {
        auto& state = owner();
        if (!state.capabilities.keyboard_focus || !state.capabilities.committed_text)
            throw Exceptions::failed_operation(CE_HERE, "RmlUi keyboard focus requires routed committed text support");
        auto text = state.input.capture(Input::InputMode::Text);
        auto focus = state.input.routing().focus(state.target, routing);
        state.text = std::move(text);
        state.focus = std::move(focus);
        state.focus_epoch = state.focus.epoch();
    }

    void Session::release_keyboard_focus() {
        auto& state = owner();
        state.focus.reset();
        state.text.reset();
        if (auto* element = state.context->GetFocusElement())
            element->Blur();
    }

    bool Session::owns_keyboard_focus() const {
        return owner().focus.owns_focus();
    }

    void Session::handle_input(const std::span<const Input::InputRecord> records, const bool pointer_selected) {
        auto& state = owner();
        for (const auto& record : records) {
            const auto* button = std::get_if<Input::ButtonEvent>(&record.data);
            if (button && (record.device_kind == Input::DeviceKind::Keyboard || record.device_kind == Input::DeviceKind::Mouse))
                state.modifier_state = button->modifiers;
            const bool keyboard = record.is_text() || record.device_kind == Input::DeviceKind::Keyboard;
            if (keyboard) {
                if (record.target != state.target || state.focus_epoch == 0 || record.focus_epoch != state.focus_epoch)
                    continue;
                if (const auto* text = std::get_if<Input::TextEvent>(&record.data)) {
                    if (text->codepoint > 0x10FFFF || (text->codepoint >= 0xD800 && text->codepoint <= 0xDFFF))
                        throw Exceptions::invalid_args(CE_HERE, "RmlUi text input requires a Unicode scalar");
                    state.context->ProcessTextInput(static_cast<Rml::Character>(text->codepoint));
                } else if (button) {
                    const auto key = keyboard_key(button->key);
                    if (key == Rml::Input::KI_UNKNOWN || clipboard_shortcut(*button))
                        continue;
                    if (button->phase == Input::ButtonPhase::Release)
                        state.context->ProcessKeyUp(key, modifiers(button->modifiers));
                    else
                        state.context->ProcessKeyDown(key, modifiers(button->modifiers));
                }
            } else if (record.device_kind == Input::DeviceKind::Mouse && pointer_selected && state.pointer_view) {
                const int flags = modifiers(state.modifier_state);
                if (const auto* move = std::get_if<Input::PointerEvent>(&record.data)) {
                    pointer(*state.context, *move, flags);
                    state.pointer_active = true;
                } else if (button) {
                    const int index = mouse_button(button->mouse_button);
                    if (index < 0 || button->phase == Input::ButtonPhase::Repeat)
                        continue;
                    if (!button->position)
                        throw Exceptions::invalid_args(CE_HERE, "RmlUi pointer buttons need an observation-time position");
                    pointer(*state.context, *button->position, flags);
                    state.pointer_active = true;
                    if (button->phase == Input::ButtonPhase::Release)
                        state.context->ProcessMouseButtonUp(index, flags);
                    else
                        state.context->ProcessMouseButtonDown(index, flags);
                } else if (const auto* scroll = std::get_if<Input::ScrollEvent>(&record.data)) {
                    if (!scroll->position || !std::isfinite(scroll->x) || !std::isfinite(scroll->y) ||
                        !std::isfinite(static_cast<float>(scroll->x)) || !std::isfinite(static_cast<float>(scroll->y)))
                        throw Exceptions::invalid_args(CE_HERE, "RmlUi scrolling needs a position and finite representable deltas");
                    pointer(*state.context, *scroll->position, flags);
                    state.pointer_active = true;
                    state.context->ProcessMouseWheel({-static_cast<float>(scroll->x), -static_cast<float>(scroll->y)}, flags);
                }
            }
        }
        if ((!pointer_selected || !state.pointer_view) && state.pointer_active) {
            state.context->ProcessMouseLeave();
            state.pointer_active = false;
        }
        // Drain poll-latched records before acknowledging preemption. Releasing
        // this stale lease cannot erase another adapter's newer focus request.
        if (state.focus.epoch() != 0 && !state.focus.owns_focus())
            release_keyboard_focus();
    }

    RecordedScene Session::record() {
        auto& state = owner();
        try {
            if (!state.context->Update())
                throw Exceptions::failed_operation(CE_HERE, "RmlUi context update failed");
            state.renderer.begin_recording();
            if (state.drawable && !state.context->Render())
                throw Exceptions::failed_operation(CE_HERE, "RmlUi context rendering failed");
            return state.renderer.finish_recording();
        } catch (...) {
            state.renderer.discard_recording();
            throw;
        }
    }

    void Session::close_after_quiescence() {
        state_.reset();
    }
}
