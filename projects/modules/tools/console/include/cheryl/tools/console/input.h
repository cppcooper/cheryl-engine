#pragma once

#include <cheryl/export.h>
#include <cheryl/core/controls/input-record.h>

#include <functional>
#include <memory>
#include <optional>
#include <span>

namespace CE::UI::RmlUi {
    class Session;
}

namespace CE::Tools::Console::Presentation {
    class View;

    /** Configuration for the selected Console input owner, not a shortcut registry. */
    struct InputOptions {
        std::optional<CE::Input::KeyboardKey> toggle_key = CE::Input::KeyboardKey::GraveAccent; ///< Physical Press toggles regardless of Shift; nullopt disables. Unknown is invalid.
    };

    /** Install one selected input owner per UI Session, not one per View.
     * Borrows Session and View on their common UI owner; both outlive Input.
     * Does not open the View or acquire an additional global capture. Call retire()
     * on the UI owner before View/Session retirement; destruction then makes no
     * toolkit calls and may follow a simulation-thread join. No listeners are added.
     * @see [Console input](cheryl-guide:modules/console#input-ownership)
     */
    class CE_TYPE_API Input final {
        struct State;
        std::unique_ptr<State> state_;

    public:
        CE_CONSOLE_API Input(CE::UI::RmlUi::Session& session, View& view, InputOptions options = {});
        CE_CONSOLE_API ~Input();
        Input(const Input&) = delete;
        Input& operator=(const Input&) = delete;
        CE_CONSOLE_API bool handle_input(
            std::span<const CE::Input::Events::Record> records,
            bool drawable = true,
            const std::function<void(std::span<const CE::Input::Events::Record>)>& drain = {});
        [[nodiscard]] CE_CONSOLE_API bool transitioned() const noexcept;
        CE_CONSOLE_API std::optional<bool> route_toggle(const CE::Input::Events::Record& record);
        CE_CONSOLE_API void cancel_pending_open() noexcept;
        CE_CONSOLE_API void retire();
    };
}

/** @fn CE::Tools::Console::Presentation::Input::Input(CE::UI::RmlUi::Session& session, View& view, InputOptions options)
 * Select a borrowed View and its Session. The caller guarantees the View belongs
 * to this Session and no other Console Input drains it. Unknown/out-of-range keys
 * throw invalid_argument. Allocation can throw; no focus is acquired.
 */

/** @fn bool CE::Tools::Console::Presentation::Input::handle_input(std::span<const CE::Input::Events::Record> records, bool drawable, const std::function<void(std::span<const CE::Input::Events::Record>)>& drain)
 * Call once per complete ordered poll, before consuming any gameplay effects.
 * Returns whole-batch gameplay admission, not a pause policy. On false, discard
 * already queued gameplay/menu effects, including actions earlier in this batch.
 * Held buttons remain quarantined until release; fresh gestures recover later.
 * Default drain delivers the filtered batch to RmlUi, selecting Console pointers.
 * Optional drain runs synchronously once on the UI owner, retaining no references;
 * it must drain all participating Sessions exactly once using only the supplied
 * records. Call route_toggle() in its per-record callback, and cancel_pending_open()
 * when selecting a different focus owner. It may read transitioned() to cancel
 * queued UI effects before drain. Both Sessions honor ui_consumed.
 * Advanced drain retains responsibility for its existing physical-button gesture
 * quarantine, including stale releases and positive fresh-gesture recovery;
 * default drain supplies that quarantine for the ordinary single-Console host.
 * New Console focus is acquired only after drain returns. Non-drawable batches
 * close the Console. Reentrancy throws logic_error; callback/toolkit/allocation
 * failures propagate, with pending opening cancelled. No input span is retained.
 */

/** @fn std::optional<bool> CE::Tools::Console::Presentation::Input::route_toggle(const CE::Input::Events::Record& record)
 * Advanced drain only: call once per record, in supplied order, before toolkit
 * delivery. A selected Press retires old RmlUi focus and hides the View, returning
 * true for pending open or false for close. Other records return nullopt. Neither
 * repeat nor release toggles. Calling outside handle_input throws logic_error.
 */

/** @fn void CE::Tools::Console::Presentation::Input::cancel_pending_open() noexcept
 * Advanced drain only: cancel deferred Console focus when the host selects a
 * different owner (for example Escape or another toolkit). Later toggles in the
 * same drain can select Console again. Does not release another owner's focus.
 */

/** @fn void CE::Tools::Console::Presentation::Input::retire()
 * On the live UI owner, cancel pending opening and hide/release the selected
 * Console. Idempotent; after successful retirement repeated calls need no toolkit
 * access, including after joining that owner. Further handle_input calls throw.
 * Retire before destroying the borrowed View/Session. Destructor only frees
 * retained bookkeeping; it does not access a possibly retired toolkit.
 */

/** @fn bool CE::Tools::Console::Presentation::Input::transitioned() const noexcept
 * Report an ownership transition in the current/most recently handled batch.
 * The signal is set before a transition batch's drain callback; inspect the
 * final handle_input result before gameplay even when a native Close occurs.
 */
