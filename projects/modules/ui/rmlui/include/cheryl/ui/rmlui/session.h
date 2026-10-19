#pragma once

#include <cheryl/export.h>

#include "rendering.h"

#include <core/controls/input-interface.h>
#include <core/display/framebuffer-size.h>
#include <core/display/viewport.h>
#include <core/engine/view-state.h>

#include <RmlUi/Core/Context.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <span>

namespace CE::Runtime {
    class Context;
}
namespace CE::Text {
    class Font;
    class FontService;
}

namespace CE::UI::RmlUi {
    /** Session-owned CPU toolkit limits, validated during construction. */
    struct SessionOptions {
        // Core's stock font atlas can allocate up to 1024 per dimension.
        unsigned int maximum_texture_size = 4096; ///< Maximum texture dimension in pixels; positive/representable; at least 1024 for the native font atlas.
        bool automatic_view = false; ///< Context construction follows SimulationView; standalone construction requires explicit follow_view().
    };

    /** Copied support snapshot; committed text does not imply IME composition or full text-editor shaping. */
    struct Capabilities {
        bool keyboard_focus = false; ///< Input supports routed keyboard-focus leases.
        bool committed_text = false; ///< Input supports committed Unicode Text records.
        bool clipboard = false; ///< Clipboard integration is unavailable in the current adapter.
        bool cursor = false; ///< Native OS cursor control is unavailable in the current adapter.
        bool ime = false; ///< IME composition is unavailable.
        bool rectangular_clipping = true; ///< Axis-aligned rectangular clip recording is available.
        bool effects = false; ///< Masks, offscreen layers, filters and custom shaders are unavailable.
    };

    // Create on the simulation/UI owner before authoring native documents.
    // One session owns Core's process globals; it does not replace a host's
    // initialized SDK or installed interfaces. input outlives the session.
    /** Owns one toolkit process-global session on its creating simulation/UI thread. Input outlives the session; external native toolkit references must be released before teardown. Calls after close or from another owner throw.
     * @see [Session integration](cheryl-guide:modules/rmlui) */
    class Session final {
        struct State;
        std::shared_ptr<State> state_;
        CE::Runtime::ViewAttachment view_attachment_;

    public:
        CE_UI_RMLUI_API Session(CE::Input::System& input, CE::Input::Routing::TargetId target, SessionOptions options = {});
        CE_UI_RMLUI_API Session(
            CE::Input::System& input,
            const CE::Text::FontService& fonts,
            CE::Input::Routing::TargetId target,
            SessionOptions options = {}
        );
        CE_UI_RMLUI_API Session(CE::Runtime::Context& engine, CE::Input::Routing::TargetId target, SessionOptions options = {});
        CE_UI_RMLUI_API ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        [[nodiscard]] CE_UI_RMLUI_API Rml::Context& context();
        [[nodiscard]] CE_UI_RMLUI_API Capabilities capabilities() const;
        CE_UI_RMLUI_API bool load_font(const std::filesystem::path& file, const Rml::String& family, bool fallback = false);
        CE_UI_RMLUI_API bool load_font(std::span<const unsigned char> bytes, const Rml::String& family, bool fallback = false);
        // Registers retained Engine bytes and returns the native RCSS family name.
        [[nodiscard]] CE_UI_RMLUI_API Rml::String font_family(const CE::Text::Font& selected);
        CE_UI_RMLUI_API void set_font(Rml::Element& element, const CE::Text::Font& selected);
        CE_UI_RMLUI_API void set_view(CE::Platform::Dimensions<int> logical, CE::Platform::FramebufferSize framebuffer);
        /** Attach and seed on the simulation owner. A valid set_view() detaches automatic sizing. */
        CE_UI_RMLUI_API void follow_view(const CE::Runtime::SimulationViewSource& source);
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
        static CE_UI_RMLUI_API void apply_view(State&, CE::Platform::Dimensions<int>, CE::Platform::FramebufferSize);
        [[nodiscard]] CE_UI_RMLUI_API State& owner() const;
    };
}

/** @fn CE::UI::RmlUi::Session::Session(CE::Input::System& input, CE::Input::Routing::TargetId target, SessionOptions options)
 * Construct a standalone toolkit session on the UI owner before creating widgets/documents. target must be nonzero; capture support and toolkit global ownership are required. Borrow input for the session lifetime; option/capture/global-state/allocation failures throw. Configure standalone fonts explicitly as needed.
 * @param target Nonzero application-chosen keyboard/text routing identity for this entire session. Components share it; numeric magnitude gives no priority. Choose distinct IDs for independent recipients sharing one input system.
 * @see [Session routing and widget focus](cheryl-guide:gui/index#session-routing-and-widget-focus)
 */

/** @fn CE::UI::RmlUi::Session::Session(CE::Input::System& input, const CE::Text::FontService& fonts, CE::Input::Routing::TargetId target, SessionOptions options)
 * Construct with a retained catalogue/default snapshot: ordinary native text inherits the configured default. Existing Font handles/bytes are retained without reopening paths; later service reconfiguration does not rebind this session. Input remains borrowed, but the font service need not outlive the imported snapshot. Case-insensitive RCSS aliases must identify one case-sensitive Engine reference; case collisions reject construction.
 * @param target Nonzero application-chosen keyboard/text routing identity for this entire session. Components share it; numeric magnitude gives no priority. Choose distinct IDs for independent recipients sharing one input system.
 * @see [Session routing and widget focus](cheryl-guide:gui/index#session-routing-and-widget-focus)
 */

/** @fn CE::UI::RmlUi::Session::Session(CE::Runtime::Context& engine, CE::Input::Routing::TargetId target, SessionOptions options)
 * Construct using engine.input() and engine.fonts(). Acquire this on the permitted owner after Engine initialization; preserve the input lifetime and release the session before runtime input teardown.
 * @param target Nonzero application-chosen keyboard/text routing identity for this entire session. Components share it; numeric magnitude gives no priority. Choose distinct IDs for independent recipients sharing one input system.
 * @see [Session routing and widget focus](cheryl-guide:gui/index#session-routing-and-widget-focus)
 */

/** @fn CE::UI::RmlUi::Session::~Session()
 * Release toolkit globals and retained CPU state after callers/native references quiesce. Uploaded Cheryl scenes and CPU recordings remain independently retained. Use close_after_quiescence() for final platform teardown after simulation joins.
 */

/** @fn CE::UI::RmlUi::Capabilities CE::UI::RmlUi::Session::capabilities() const
 * Return a copied input/toolkit support snapshot; query on the live creating UI owner.
 */

/** @fn void CE::UI::RmlUi::Session::set_view(CE::Platform::Dimensions<int> logical, CE::Platform::FramebufferSize framebuffer)
 * Copy logical window and framebuffer-pixel dimensions; neither reads a native window. Negative sizes reject. Zero logical extent disables pointer input; zero logical/framebuffer extent suppresses drawing. Native layout uses logical units.
 * Initialize before first input/recording, then refresh when either size changes. The view persists between calls; repeating unchanged values is valid.
 */

/** @fn void CE::UI::RmlUi::Session::update_time(double delta_seconds)
 * Advance toolkit simulation time by a finite nonnegative representable number of seconds. Invalid durations reject; native layout/update failures may propagate. Call record() to update authored layout without advancing elapsed time.
 */

/** @fn void CE::UI::RmlUi::Session::request_keyboard_focus(CE::Input::Routing::KeyboardPolicy routing)
 * Acquire committed-text capture and a new focus lease/epoch for this target. Requires keyboard-focus and Text capabilities. New focus affects future polls; already collected records retain their earlier target/epoch. Toolkit widget/element focus is authored separately.
 */

/** @fn void CE::UI::RmlUi::Session::release_keyboard_focus()
 * Release this session's focus/Text leases and native widget focus. Releasing an old lease cannot erase another owner's newer focus. Drain already collected poll-latched records before release.
 */

/** @fn bool CE::UI::RmlUi::Session::owns_keyboard_focus() const
 * Return whether this session's current focus lease owns routing; this is distinct from a particular widget being focused.
 */

/** @fn void CE::UI::RmlUi::Session::handle_input(std::span<const CE::Input::Events::Record> records, bool pointer_selected)
 * Deliver one complete ordered tick batch on the UI owner without consuming/modifying shared records. Keyboard/Text delivery requires matching target and entry focus epoch; observe modifier releases even when not selected. pointer_selected explicitly admits mouse records, whose recorded positions are authoritative. Missing/invalid positions or text reject; unavailable clipboard shortcuts do not delete selected text.
 * @see [Batch routing](cheryl-guide:development/ui-adapters#routing-and-unavailable-services)
 */

/** @fn void CE::UI::RmlUi::Session::handle_input(std::span<const CE::Input::Events::Record> records, bool pointer_selected, const std::function<bool(const CE::Input::Events::Record&)>& before_record)
 * Before each record, synchronously run application controls and use the returned bool for that record's pointer selection. The callback is borrowed for this call. Keyboard/Text retains the batch-entry epoch even if callbacks change focus; acknowledge preemption after draining the batch. Callback/event failures propagate after any earlier widget effects; no rollback is promised.
 */

/** @fn CE::UI::RmlUi::RecordedScene CE::UI::RmlUi::Session::record()
 * Record one complete owned CPU scene on the UI owner. Update native layout without advancing simulation time, then render when drawable. A zero view returns an empty recording; failure discards the incomplete recording and propagates. No graphics upload occurs.
 */

/** @fn void CE::UI::RmlUi::Session::close_after_quiescence()
 * Release toolkit ownership after UI/simulation calls have stopped. Final teardown may run on platform after join; release external toolkit widget/font/element/listener references first. No calls may race teardown; later session calls reject. Retained CPU recordings/uploaded Cheryl scenes remain independent.
 */

/** @fn Rml::Context& CE::UI::RmlUi::Session::context()
 * Borrow the native RmlUi context on its live UI owner. Author documents/styles/listeners natively and release external native references before session teardown.
 */

/** @fn bool CE::UI::RmlUi::Session::load_font(const std::filesystem::path& file, const Rml::String& family, bool fallback)
 * Load an explicit face from file into the native family alias. Nonempty family is required; collisions with managed Engine/private fallback aliases reject. Core retains loaded font storage; unavailable/native load returns false. fallback explicitly permits global glyph fallback; specialty fonts should ordinarily leave it false.
 */

/** @fn bool CE::UI::RmlUi::Session::load_font(std::span<const unsigned char> bytes, const Rml::String& family, bool fallback)
 * Copy nonempty font bytes through Core shutdown, without borrowing application storage. Alias/availability rules follow file loading. Invalid arguments/allocation failures throw; native face rejection returns false.
 */

/** @fn Rml::String CE::UI::RmlUi::Session::font_family(const CE::Text::Font& selected)
 * Register retained primary variants and return the Engine reference as its RCSS alias. Preserves actual face indices and nearest missing normal/italic styles without synthesizing glyphs. Empty selections, case collisions or a different snapshot under an already reserved alias reject. Common fallbacks use separate aliases; specialty fonts do not become global fallbacks.
 * @see [RmlUi font naming and styles](cheryl-guide:modules/rmlui#fonts)
 */

/** @fn void CE::UI::RmlUi::Session::set_font(Rml::Element& element, const CE::Text::Font& selected)
 * Set the native font-family property to font_family(selected). The element must belong to this session, including its root; foreign elements reject. Other RCSS properties such as weight/italic/size remain native style choices; property/registration failures throw.
 */
