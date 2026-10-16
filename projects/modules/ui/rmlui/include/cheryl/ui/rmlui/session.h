#pragma once

#include <cheryl/export.h>

#include "rendering.h"

#include <core/controls/input-interface.h>
#include <core/display/framebuffer-size.h>
#include <core/display/viewport.h>

#include <RmlUi/Core/Context.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <span>

namespace CE::UI::RmlUi {
    struct SessionOptions {
        // Core's stock FreeType atlas can allocate up to 1024 per dimension.
        unsigned int maximum_texture_size = 4096;
    };

    struct Capabilities {
        bool keyboard_focus = false;
        bool committed_text = false;
        bool clipboard = false;
        bool cursor = false;
        bool ime = false;
        bool rectangular_clipping = true;
        bool effects = false;
    };

    // Create on the simulation/UI owner before authoring native documents.
    // One session owns Core's process globals; it does not replace a host's
    // initialized SDK or installed interfaces. input outlives the session.
    class Session final {
        struct State;
        std::unique_ptr<State> state_;

    public:
        CE_UI_RMLUI_API Session(CE::Input::System& input, CE::Input::Routing::TargetId target, SessionOptions options = {});
        CE_UI_RMLUI_API ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        [[nodiscard]] CE_UI_RMLUI_API Rml::Context& context();
        [[nodiscard]] CE_UI_RMLUI_API Capabilities capabilities() const;
        // File loading lets FreeType own its buffer. Memory loading copies bytes
        // through Core shutdown; neither path borrows the application's storage.
        CE_UI_RMLUI_API bool load_font(const std::filesystem::path& file, const Rml::String& family, bool fallback = false);
        CE_UI_RMLUI_API bool load_font(std::span<const unsigned char> bytes, const Rml::String& family, bool fallback = false);
        CE_UI_RMLUI_API void set_view(CE::Platform::Dimensions<int> logical, CE::Platform::FramebufferSize framebuffer);
        // Advances simulation time and updates native layout/animation. record()
        // also updates layout after authoring or input, without advancing time.
        CE_UI_RMLUI_API void update_time(double delta_seconds);
        CE_UI_RMLUI_API void request_keyboard_focus(CE::Input::Routing::KeyboardPolicy routing = CE::Input::Routing::KeyboardPolicy::Exclusive);
        CE_UI_RMLUI_API void release_keyboard_focus();
        [[nodiscard]] CE_UI_RMLUI_API bool owns_keyboard_focus() const;
        CE_UI_RMLUI_API void handle_input(std::span<const CE::Input::Events::Record> records, bool pointer_selected);
        // Run application controls before each record and return its pointer
        // selection. Keyboard/text retain the entry epoch throughout the batch;
        // preempted focus is released only after all records have been handled.
        CE_UI_RMLUI_API void handle_input(
            std::span<const CE::Input::Events::Record> records,
            bool pointer_selected,
            const std::function<bool(const CE::Input::Events::Record&)>& before_record
        );
        [[nodiscard]] CE_UI_RMLUI_API RecordedScene record();

        // After simulation has joined, release external element/listener/resource
        // references, then transfer final destruction to the platform owner.
        // Uploaded scenes and CPU recordings may remain retained independently.
        CE_UI_RMLUI_API void close_after_quiescence();

    private:
        [[nodiscard]] CE_UI_RMLUI_API State& owner() const;
    };
}
