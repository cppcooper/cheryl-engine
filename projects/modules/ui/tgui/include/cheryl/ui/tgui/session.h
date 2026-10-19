#pragma once

#include <cheryl/export.h>

#include "layout.h"
#include "rendering.h"

#include <core/controls/input-interface.h>
#include <core/display/framebuffer-size.h>
#include <core/display/viewport.h>
#include <core/engine/view-state.h>
#include <core/engine/runtime-services.h>
#include <cheryl/text/font.h>

#include <TGUI/Backend/Window/BackendGui.hpp>

#include <functional>
#include <memory>
#include <span>

namespace CE::Runtime {
    class Context;
}
namespace CE::Text {
    class FontService;
}

namespace CE::UI::TGUI {
    /** Session-owned CPU toolkit limits, validated during construction. */
    struct SessionOptions {
        unsigned int maximum_texture_size = 4096; ///< Maximum texture dimension in pixels, at least 128 and int-representable.
        float font_scale = 1; ///< Positive finite toolkit font scale, fixed for this session.
        bool automatic_view = false; ///< Context construction follows SimulationView; standalone construction requires explicit follow_view().
        bool automatic_time = false; ///< Context construction advances on future simulation steps; manual update_time() then rejects.
    };

    /** Copied support snapshot; committed text does not imply IME composition or full text-editor shaping. */
    struct Capabilities {
        bool keyboard_focus = false; ///< Input supports routed keyboard-focus leases.
        bool committed_text = false; ///< Input supports committed Unicode Text records.
        bool clipboard = false; ///< Clipboard integration is unavailable in the current adapter.
        bool cursor = false; ///< Native OS cursor control is unavailable in the current adapter.
        bool ime = false; ///< IME composition is unavailable.
    };

    // Create on the simulation/UI owner, before creating any toolkit objects.
    // TGUI's process-global backend permits one session at a time. input must
    // outlive the session; widget/font/texture references must not outlive it.
    /** Owns one toolkit process-global session on its creating simulation/UI thread. Input outlives the session; external native toolkit references must be released before teardown. Calls after close or from another owner throw.
     * @see [Session integration](cheryl-guide:modules/tgui) */
    class Session final {
        struct State;
        std::shared_ptr<State> state_;
        CE::Runtime::ViewAttachment view_attachment_;
        CE::Runtime::ServiceAttachment time_attachment_;

    public:
        CE_UI_TGUI_API Session(CE::Input::System& input, CE::Input::Routing::TargetId target, SessionOptions options = {});
        CE_UI_TGUI_API Session(
            CE::Input::System& input,
            const CE::Text::FontService& fonts,
            CE::Input::Routing::TargetId target,
            SessionOptions options = {}
        );
        CE_UI_TGUI_API Session(CE::Runtime::Context& engine, CE::Input::Routing::TargetId target, SessionOptions options = {});
        CE_UI_TGUI_API ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        [[nodiscard]] CE_UI_TGUI_API tgui::BackendGui& gui();
        [[nodiscard]] CE_UI_TGUI_API Capabilities capabilities() const;
        // Returned native fonts and widgets must be released before this session.
        [[nodiscard]] CE_UI_TGUI_API tgui::Font font(const CE::Text::Font& selected, CE::Text::FontStyle style = {});
        CE_UI_TGUI_API void set_font(tgui::Widget::Ptr widget, const CE::Text::Font& selected, CE::Text::FontStyle style = {});
        // Both dimensions are copied by the runtime; no native window is read.
        CE_UI_TGUI_API void set_view(CE::Platform::Dimensions<int> logical, CE::Platform::FramebufferSize framebuffer);
        /** Attach and seed on the simulation owner. A valid set_view() detaches automatic sizing. */
        CE_UI_TGUI_API void follow_view(const CE::Runtime::SimulationViewSource& source);
        // Add the widget to this GUI first. Typed rules bind its current parent's
        // content dimensions; reapply after reparenting. Released scalable axes
        // freeze at their current size. Other native sizing remains unchanged.
        CE_UI_TGUI_API void set_layout(tgui::Widget::Ptr widget, const WidgetLayout& layout);
        CE_UI_TGUI_API void update_time(double seconds);
        /** Attach on the simulation owner; starts next step without replaying a past delta. */
        CE_UI_TGUI_API void follow_time(const CE::Runtime::RuntimeServices&);
        CE_UI_TGUI_API void detach_time();
        CE_UI_TGUI_API void request_keyboard_focus(CE::Input::Routing::KeyboardPolicy routing = CE::Input::Routing::KeyboardPolicy::Exclusive);
        CE_UI_TGUI_API void release_keyboard_focus();
        [[nodiscard]] CE_UI_TGUI_API bool owns_keyboard_focus() const;
        // Observe the entire ordered stream for modifiers, including releases.
        // Deliver keyboard/text only for this session's requested focus epoch.
        // The application explicitly selects pointer delivery for this call.
        CE_UI_TGUI_API void handle_input(std::span<const CE::Input::Events::Record> records, bool pointer_selected);
        // Run application controls before each record and return its pointer
        // selection. Keyboard/text retain the entry epoch throughout the batch;
        // preempted focus is released only after all records have been handled.
        CE_UI_TGUI_API void handle_input(
            std::span<const CE::Input::Events::Record> records,
            bool pointer_selected,
            const std::function<bool(const CE::Input::Events::Record&)>& before_record
        );
        [[nodiscard]] CE_UI_TGUI_API RecordedScene record();

        // After the runtime joins simulation, final teardown may run on platform.
        // Release external toolkit references first; no calls may race teardown.
        // Uploaded Cheryl scenes/frames are independent and may remain retained.
        CE_UI_TGUI_API void close_after_quiescence();

    private:
        static CE_UI_TGUI_API void apply_view(State&, CE::Platform::Dimensions<int>, CE::Platform::FramebufferSize);
        static CE_UI_TGUI_API void apply_time(State&, double);
        [[nodiscard]] CE_UI_TGUI_API State& owner() const;
    };
}

/** @fn CE::UI::TGUI::Session::Session(CE::Input::System& input, CE::Input::Routing::TargetId target, SessionOptions options)
 * Construct a standalone toolkit session on the UI owner before creating widgets/documents. target must be nonzero; capture support and toolkit global ownership are required. Borrow input for the session lifetime; option/capture/global-state/allocation failures throw. Configure standalone fonts explicitly as needed.
 * @param target Nonzero application-chosen keyboard/text routing identity for this entire session. Components share it; numeric magnitude gives no priority. Choose distinct IDs for independent recipients sharing one input system.
 * @see [Session routing and widget focus](cheryl-guide:gui/index#session-routing-and-widget-focus)
 */

/** @fn CE::UI::TGUI::Session::Session(CE::Input::System& input, const CE::Text::FontService& fonts, CE::Input::Routing::TargetId target, SessionOptions options)
 * Construct with a retained catalogue/default snapshot: ordinary native text inherits the configured default. Existing Font handles/bytes are retained without reopening paths; later service reconfiguration does not rebind this session. Input remains borrowed, but the font service need not outlive the imported snapshot. Theme font aliases use configured Engine references.
 * @param target Nonzero application-chosen keyboard/text routing identity for this entire session. Components share it; numeric magnitude gives no priority. Choose distinct IDs for independent recipients sharing one input system.
 * @see [Session routing and widget focus](cheryl-guide:gui/index#session-routing-and-widget-focus)
 */

/** @fn CE::UI::TGUI::Session::Session(CE::Runtime::Context& engine, CE::Input::Routing::TargetId target, SessionOptions options)
 * Construct using engine.input() and engine.fonts(). Acquire this on the permitted owner after Engine initialization; preserve the input lifetime and release the session before runtime input teardown.
 * @param target Nonzero application-chosen keyboard/text routing identity for this entire session. Components share it; numeric magnitude gives no priority. Choose distinct IDs for independent recipients sharing one input system.
 * @see [Session routing and widget focus](cheryl-guide:gui/index#session-routing-and-widget-focus)
 */

/** @fn CE::UI::TGUI::Session::~Session()
 * Release toolkit globals and retained CPU state after callers/native references quiesce. Uploaded Cheryl scenes and CPU recordings remain independently retained. Use close_after_quiescence() for final platform teardown after simulation joins.
 */

/** @fn CE::UI::TGUI::Capabilities CE::UI::TGUI::Session::capabilities() const
 * Return a copied input/toolkit support snapshot; query on the live creating UI owner.
 */

/** @fn void CE::UI::TGUI::Session::set_view(CE::Platform::Dimensions<int> logical, CE::Platform::FramebufferSize framebuffer)
 * Copy logical window and framebuffer-pixel dimensions; neither reads a native window. Negative sizes reject. Zero logical extent disables pointer input; zero logical/framebuffer extent suppresses drawing. Native layout uses logical units.
 * Initialize before first input/recording, then refresh when either size changes. The view persists between calls; repeating unchanged values is valid.
 */

/** @fn void CE::UI::TGUI::Session::update_time(double seconds)
 * Advance toolkit simulation time by a finite nonnegative representable number of seconds. Invalid durations reject; native layout/update failures may propagate. Uses the session's UI owner and does not poll platform events.
 */

/** @fn void CE::UI::TGUI::Session::request_keyboard_focus(CE::Input::Routing::KeyboardPolicy routing)
 * Acquire committed-text capture and a new focus lease/epoch for this target. Requires keyboard-focus and Text capabilities. New focus affects future polls; already collected records retain their earlier target/epoch. Toolkit widget/element focus is authored separately.
 */

/** @fn void CE::UI::TGUI::Session::release_keyboard_focus()
 * Release this session's focus/Text leases and native widget focus. Releasing an old lease cannot erase another owner's newer focus. Drain already collected poll-latched records before release.
 */

/** @fn bool CE::UI::TGUI::Session::owns_keyboard_focus() const
 * Return whether this session's current focus lease owns routing; this is distinct from a particular widget being focused.
 */

/** @fn void CE::UI::TGUI::Session::handle_input(std::span<const CE::Input::Events::Record> records, bool pointer_selected)
 * Deliver one complete ordered tick batch on the UI owner without consuming/modifying shared records. Keyboard/Text delivery requires matching target and entry focus epoch; observe modifier releases even when not selected. pointer_selected explicitly admits mouse records, whose recorded positions are authoritative. Missing/invalid positions or text reject; unavailable clipboard shortcuts do not delete selected text.
 * @see [Batch routing](cheryl-guide:development/ui-adapters#routing-and-unavailable-services)
 */

/** @fn void CE::UI::TGUI::Session::handle_input(std::span<const CE::Input::Events::Record> records, bool pointer_selected, const std::function<bool(const CE::Input::Events::Record&)>& before_record)
 * Before each record, synchronously run application controls and use the returned bool for that record's pointer selection. The callback is borrowed for this call. Keyboard/Text retains the batch-entry epoch even if callbacks change focus; acknowledge preemption after draining the batch. Callback/event failures propagate after any earlier widget effects; no rollback is promised.
 */

/** @fn CE::UI::TGUI::RecordedScene CE::UI::TGUI::Session::record()
 * Record one complete owned CPU scene on the UI owner. Invoke toolkit drawing when drawable. A zero view returns an empty recording; failure discards the incomplete recording and propagates. No graphics upload occurs.
 */

/** @fn void CE::UI::TGUI::Session::close_after_quiescence()
 * Release toolkit ownership after UI/simulation calls have stopped. Final teardown may run on platform after join; release external toolkit widget/font/element/listener references first. No calls may race teardown; later session calls reject. Retained CPU recordings/uploaded Cheryl scenes remain independent.
 */

/** @fn tgui::BackendGui& CE::UI::TGUI::Session::gui()
 * Borrow the native GUI on its live UI owner. Do not retain widgets/font/texture/backend references past session teardown; use native widget APIs within this lifetime.
 */

/** @fn tgui::Font CE::UI::TGUI::Session::font(const CE::Text::Font& selected, CE::Text::FontStyle style)
 * Resolve a retained Engine font to a native font using closest weight/italic variants and actual collection face indices. Empty Font rejects; native loading/allocation failures propagate. Release returned native font/widget references before the session.
 */

/** @fn void CE::UI::TGUI::Session::set_font(tgui::Widget::Ptr widget, const CE::Text::Font& selected, CE::Text::FontStyle style)
 * Set a native renderer font from the retained Engine selection/style. Null widgets or empty fonts reject. The widget/native font must not outlive this session; the operation can affect widgets sharing that renderer.
 */

/** @fn void CE::UI::TGUI::Session::set_layout(tgui::Widget::Ptr widget, const WidgetLayout& layout)
 * Bind typed placement to the current parent's content size. Add the widget to this GUI with native Manual layout first; null/foreign/unparented widgets or invalid/nonfinite rules reject. Reapply after reparenting. Releasing a formerly scalable axis freezes its current size; other native sizing is preserved.
 * @see [Typed layout](cheryl-guide:modules/tgui#typed-layout)
 */
