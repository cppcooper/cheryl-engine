#include <ui/rmlui/session.h>
#include <ui/rmlui/input.h>

#include "font.h"

#include <internals/compile-time-logging.hpp>
#include <internals/exceptions.h>
#include <cheryl/core/engine/engine-context.h>
#include <cheryl/text/font-service.h>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/StringUtilities.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CE::UI::RmlUi {
    namespace {
        std::mutex library_mutex;
        bool library_owned = false;

        void load_faces(
            const std::vector<const CE::Text::FontFace*>& faces, const Rml::String& family, const bool fallback
        ) {
            const bool normal = std::any_of(faces.begin(), faces.end(), [](const auto* face) { return !face->style.italic; });
            const bool italic = std::any_of(faces.begin(), faces.end(), [](const auto* face) { return face->style.italic; });
            const auto load = [&](const CE::Text::FontFace& face, const bool slanted) {
                if (face.face_index > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
                    !Rml::LoadFontFace(
                        {face.bytes.data(), face.bytes.size()}, family,
                        slanted ? Rml::Style::FontStyle::Italic : Rml::Style::FontStyle::Normal,
                        static_cast<Rml::Style::FontWeight>(face.style.weight), fallback, static_cast<int>(face.face_index)
                    ))
                    throw Exceptions::failed_operation(CE_HERE, "Cannot register the Engine font with RmlUi");
            };
            for (const auto* face : faces) {
                load(*face, face->style.italic);
                // Core requires an exact style match before it can choose weight.
                // Complete an absent style with the available family's faces.
                if (!normal)
                    load(*face, false);
                if (!italic)
                    load(*face, true);
            }
        }

        class System final : public Rml::SystemInterface {
        public:
            double elapsed = 0;

            double GetElapsedTime() override { return elapsed; }
            bool LogMessage(const Rml::Log::Type type, const Rml::String& message) override {
                if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT)
                    CE_LOG_ERROR(CE::Logging::Categories::rendering, "subsystem=rmlui message={}", message);
                else if (type == Rml::Log::LT_WARNING)
                    CE_LOG_WARN(CE::Logging::Categories::rendering, "subsystem=rmlui message={}", message);
                else
                    CE_LOG_DEBUG(CE::Logging::Categories::rendering, "subsystem=rmlui message={}", message);
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
            std::unique_ptr<Rml::FontEngineInterface> fonts_ = Detail::make_font_engine();

        public:
            LibraryLease(System& system, RenderTarget& renderer) {
                const std::lock_guard lock(library_mutex);
                if (library_owned || Rml::GetSystemInterface() || Rml::GetRenderInterface() || Rml::GetFileInterface() ||
                    Rml::GetFontEngineInterface() || Rml::GetTextInputHandler())
                    throw Exceptions::failed_operation(CE_HERE, "RmlUi already has an active session or host-installed interfaces");
                Rml::SetSystemInterface(&system);
                Rml::SetRenderInterface(&renderer);
                Rml::SetFontEngineInterface(fonts_.get());
                try {
                    if (!Rml::Initialise())
                        throw Exceptions::failed_operation(CE_HERE, "RmlUi initialization failed with the Engine font service");
                } catch (...) {
                    // Core 6.3 cannot Shutdown a partially initialized SDK. Its
                    // dependency contract prevents missing default interfaces;
                    // never leave our failed constructor's storage installed.
                    Rml::SetSystemInterface(nullptr);
                    Rml::SetRenderInterface(nullptr);
                    Rml::SetFontEngineInterface(nullptr);
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

        void pointer(Rml::Context& context, const CE::Input::Events::Pointer& position, const int flags) {
            context.ProcessMouseMove(coordinate(position.x), coordinate(position.y), flags);
        }

        int mouse_button(const CE::Input::MouseButton button) {
            switch (button) {
                case CE::Input::MouseButton::Left:
                    return 0;
                case CE::Input::MouseButton::Right:
                    return 1;
                case CE::Input::MouseButton::Middle:
                    return 2;
                default:
                    return -1;
            }
        }

        bool clipboard_shortcut(const CE::Input::Events::Button& event) {
            const bool command = CE::Input::has_modifier(event.modifiers, CE::Input::Modifiers::Control) ||
                                 CE::Input::has_modifier(event.modifiers, CE::Input::Modifiers::Super);
            const bool shift = CE::Input::has_modifier(event.modifiers, CE::Input::Modifiers::Shift);
            using Key = CE::Input::KeyboardKey;
            return (command && (event.key == Key::C || event.key == Key::X || event.key == Key::V || event.key == Key::Insert)) ||
                   (shift && (event.key == Key::Insert || event.key == Key::Delete));
        }
    }

    struct Session::State {
        enum class FontOwner { Engine, Explicit, Fallback };
        struct FontRegistration {
            FontOwner owner;
            CE::Text::Font source;
            bool complete = false;
        };

        CE::Input::System& input;
        const CE::Input::Routing::TargetId target;
        const std::thread::id thread = std::this_thread::get_id();
        const Capabilities capabilities;
        CE::Input::Capture::Lease events;
        CE::Input::Capture::Lease text;
        CE::Input::Routing::Lease focus;
        std::uint64_t focus_epoch = 0;
        CE::Input::Modifiers modifier_state = CE::Input::Modifiers::None;
        bool drawable = false;
        bool pointer_view = false;
        bool pointer_active = false;
        // Reverse destruction shuts down contexts/fonts before renderer/system
        // storage and memory-loaded font bytes disappear.
        std::vector<std::vector<unsigned char>> font_bytes;
        std::unordered_map<Rml::String, FontRegistration> engine_fonts;
        std::size_t fallback_family = 0;
        bool fallbacks_loaded = false;
        System system;
        RenderTarget renderer;
        LibraryLease library;
        Rml::Context* context = nullptr;

        State(CE::Input::System& source, const CE::Input::Routing::TargetId id, const SessionOptions options)
        : input(source),
          target(id),
          capabilities{source.supports_focus(), source.supports(CE::Input::Capture::Channel::Text)},
          events(source.capture(CE::Input::Capture::Channel::Events)),
          renderer(options.maximum_texture_size),
          library(system, renderer) {
            context = Rml::CreateContext("cheryl-ui", {1, 1});
            if (!context)
                throw Exceptions::failed_operation(CE_HERE, "Cannot create the RmlUi context");
            context->SetDensityIndependentPixelRatio(1);
            context->EnableMouseCursor(false);
        }

        FontRegistration& reserve_font(const CE::Text::Font& selected) {
            if (!selected)
                throw Exceptions::invalid_args(CE_HERE, "RmlUi needs a resolved Engine font");
            const auto& reference = selected.reference();
            const auto lower = Rml::StringUtilities::ToLower(reference);
            const auto [registration, inserted] = engine_fonts.try_emplace(lower, FontRegistration{FontOwner::Engine, selected});
            if (!inserted) {
                if (registration->second.owner != FontOwner::Engine || registration->second.source.reference() != reference ||
                    registration->second.source.faces().data() != selected.faces().data())
                    throw Exceptions::invalid_args(CE_HERE, "RmlUi font-family names must identify one Engine font without case collisions");
            }
            return registration->second;
        }

        Rml::String font_family(const CE::Text::Font& selected) {
            auto& registration = reserve_font(selected);
            const auto& reference = selected.reference();
            if (registration.complete)
                return reference;
            // Retain the immutable memory before publishing views to Core.
            std::vector<const CE::Text::FontFace*> faces;
            for (const auto& face : selected.faces()) {
                if (!face.fallback)
                    faces.push_back(&face);
            }
            load_faces(faces, reference, false);
            registration.complete = true;
            return reference;
        }

        void load_fallbacks(const CE::Text::Font& selected, const bool primary_only = false) {
            if (!selected)
                throw Exceptions::invalid_args(CE_HERE, "RmlUi needs a resolved Engine fallback font");
            Rml::String family;
            std::size_t family_index = 0;
            std::vector<const CE::Text::FontFace*> faces;
            for (const auto& face : selected.faces()) {
                if (primary_only ? face.fallback : (!face.fallback && !face.builtin))
                    continue;
                if (family.empty() || face.family_index != family_index) {
                    if (!faces.empty())
                        load_faces(faces, family, true);
                    faces.clear();
                    family_index = face.family_index;
                    do {
                        family = "__cheryl_fallback_" + std::to_string(fallback_family++);
                    } while (engine_fonts.contains(family));
                    engine_fonts.emplace(family, FontRegistration{FontOwner::Fallback, selected});
                }
                faces.push_back(&face);
            }
            if (!faces.empty())
                load_faces(faces, family, true);
            fallbacks_loaded = true;
        }

        void reserve_explicit(const Rml::String& family) {
            const auto lower = Rml::StringUtilities::ToLower(family);
            const auto [registration, inserted] = engine_fonts.try_emplace(lower, FontRegistration{FontOwner::Explicit, {}});
            if (!inserted && registration->second.owner != FontOwner::Explicit)
                throw Exceptions::invalid_args(CE_HERE, "RmlUi explicit font loading conflicts with an Engine font family");
        }
    };

    Session::Session(CE::Input::System& input, const CE::Input::Routing::TargetId target, const SessionOptions options) {
        if (target == 0)
            throw Exceptions::invalid_args(CE_HERE, "RmlUi requires a nonzero focus target");
        if (options.maximum_texture_size < 1024)
            throw Exceptions::invalid_args(CE_HERE, "RmlUi's stock font atlas needs a texture bound of at least 1024");
        state_ = std::make_unique<State>(input, target, options);
    }

    Session::Session(
        CE::Input::System& input,
        const CE::Text::FontService& fonts,
        const CE::Input::Routing::TargetId target,
        const SessionOptions options
    ) : Session(input, target, options) {
        auto& state = owner();
        const auto catalogue = fonts.all();
        for (const auto& font : catalogue)
            static_cast<void>(state.reserve_font(font));
        const auto default_font = fonts.default_font();
        for (const auto& fallback : fonts.fallbacks())
            state.load_fallbacks(fallback, true);
        for (const auto& font : catalogue)
            static_cast<void>(state.font_family(font));
        set_font(*state.context->GetRootElement(), default_font);
    }

    Session::Session(CE::Runtime::Context& engine, const CE::Input::Routing::TargetId target, const SessionOptions options)
    : Session(engine.input(), engine.fonts(), target, options) {}

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
        auto& state = owner();
        if (family.empty())
            throw Exceptions::invalid_args(CE_HERE, "RmlUi font registration needs a family name");
        state.reserve_explicit(family);
        const auto utf8 = file.u8string();
        return Rml::LoadFontFace(
            Rml::String(utf8.begin(), utf8.end()), family, Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto, fallback
        );
    }

    bool Session::load_font(const std::span<const unsigned char> bytes, const Rml::String& family, const bool fallback) {
        auto& state = owner();
        if (bytes.empty() || family.empty())
            throw Exceptions::invalid_args(CE_HERE, "RmlUi memory font registration needs bytes and a family name");
        state.reserve_explicit(family);
        state.font_bytes.emplace_back(bytes.begin(), bytes.end());
        const auto& copy = state.font_bytes.back();
        return Rml::LoadFontFace({copy.data(), copy.size()}, family, Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto, fallback);
    }

    Rml::String Session::font_family(const CE::Text::Font& selected) {
        auto& state = owner();
        static_cast<void>(state.reserve_font(selected));
        if (!state.fallbacks_loaded)
            state.load_fallbacks(selected);
        return state.font_family(selected);
    }

    void Session::set_font(Rml::Element& element, const CE::Text::Font& selected) {
        auto& state = owner();
        if (&element != state.context->GetRootElement() && element.GetContext() != state.context)
            throw Exceptions::invalid_args(CE_HERE, "RmlUi font assignment needs an element in this session");
        if (!element.SetProperty("font-family", font_family(selected)))
            throw Exceptions::failed_operation(CE_HERE, "Cannot assign the Engine font to the RmlUi element");
    }

    void Session::set_view(const CE::Platform::Dimensions<int> logical, const CE::Platform::FramebufferSize framebuffer) {
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

    void Session::request_keyboard_focus(const CE::Input::Routing::KeyboardPolicy routing) {
        auto& state = owner();
        if (!state.capabilities.keyboard_focus || !state.capabilities.committed_text)
            throw Exceptions::failed_operation(CE_HERE, "RmlUi keyboard focus requires routed committed text support");
        auto text = state.input.capture(CE::Input::Capture::Channel::Text);
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

    void Session::handle_input(const std::span<const CE::Input::Events::Record> records, const bool pointer_selected) {
        handle_input(records, pointer_selected, {});
    }

    void Session::handle_input(
        const std::span<const CE::Input::Events::Record> records,
        bool pointer_selected,
        const std::function<bool(const CE::Input::Events::Record&)>& before_record
    ) {
        auto& state = owner();
        const auto focus_epoch = state.focus_epoch;
        const auto leave_unselected_pointer = [&] {
            if ((!pointer_selected || !state.pointer_view) && state.pointer_active) {
                state.context->ProcessMouseLeave();
                state.pointer_active = false;
            }
        };
        for (const auto& record : records) {
            if (before_record)
                pointer_selected = before_record(record);
            leave_unselected_pointer();
            const auto* button = std::get_if<CE::Input::Events::Button>(&record.data);
            if (button && (record.device_kind == CE::Input::DeviceKind::Keyboard || record.device_kind == CE::Input::DeviceKind::Mouse))
                state.modifier_state = button->modifiers;
            const bool keyboard = record.is_text() || record.device_kind == CE::Input::DeviceKind::Keyboard;
            if (keyboard) {
                if (record.target != state.target || focus_epoch == 0 || record.focus_epoch != focus_epoch)
                    continue;
                if (const auto* text = std::get_if<CE::Input::Events::Text>(&record.data)) {
                    if (text->codepoint > 0x10FFFF || (text->codepoint >= 0xD800 && text->codepoint <= 0xDFFF))
                        throw Exceptions::invalid_args(CE_HERE, "RmlUi text input requires a Unicode scalar");
                    state.context->ProcessTextInput(static_cast<Rml::Character>(text->codepoint));
                } else if (button) {
                    const auto key = keyboard_key(button->key);
                    if (key == Rml::Input::KI_UNKNOWN || clipboard_shortcut(*button))
                        continue;
                    if (button->phase == CE::Input::Events::ButtonPhase::Release)
                        state.context->ProcessKeyUp(key, modifiers(button->modifiers));
                    else
                        state.context->ProcessKeyDown(key, modifiers(button->modifiers));
                }
            } else if (record.device_kind == CE::Input::DeviceKind::Mouse && pointer_selected && state.pointer_view) {
                const int flags = modifiers(state.modifier_state);
                if (const auto* move = std::get_if<CE::Input::Events::Pointer>(&record.data)) {
                    pointer(*state.context, *move, flags);
                    state.pointer_active = true;
                } else if (button) {
                    const int index = mouse_button(button->mouse_button);
                    if (index < 0 || button->phase == CE::Input::Events::ButtonPhase::Repeat)
                        continue;
                    if (!button->position)
                        throw Exceptions::invalid_args(CE_HERE, "RmlUi pointer buttons need an observation-time position");
                    pointer(*state.context, *button->position, flags);
                    state.pointer_active = true;
                    if (button->phase == CE::Input::Events::ButtonPhase::Release)
                        state.context->ProcessMouseButtonUp(index, flags);
                    else
                        state.context->ProcessMouseButtonDown(index, flags);
                } else if (const auto* scroll = std::get_if<CE::Input::Events::Scroll>(&record.data)) {
                    if (!scroll->position || !std::isfinite(scroll->x) || !std::isfinite(scroll->y) ||
                        !std::isfinite(static_cast<float>(scroll->x)) || !std::isfinite(static_cast<float>(scroll->y)))
                        throw Exceptions::invalid_args(CE_HERE, "RmlUi scrolling needs a position and finite representable deltas");
                    pointer(*state.context, *scroll->position, flags);
                    state.pointer_active = true;
                    state.context->ProcessMouseWheel({-static_cast<float>(scroll->x), -static_cast<float>(scroll->y)}, flags);
                }
            }
        }
        leave_unselected_pointer();
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
