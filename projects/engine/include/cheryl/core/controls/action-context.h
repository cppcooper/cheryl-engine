#pragma once

#include <cheryl/export.h>
#include "input-bindings.h"

#include <memory>
#include <vector>

namespace CE::Input::Actions {
    namespace Detail {
        struct ContextToken;
        struct ContextsState;
    }

    /** One AND chord mapped to a context-local semantic button; alternative entries combine with OR. */
    struct ContextButton {
        Chord chord; ///< Nonempty chord whose controls must be held together.
        Id action{0}; ///< Local semantic ID; zero is valid and IDs can be reused in other contexts/direct bindings.
    };
    /** One physical axis gated by an optional modifier chord and mapped into the context-local namespace. */
    struct ContextAxis {
        Chord modifiers; ///< AND gate; an empty chord is unconditional.
        CE::Input::Control control; ///< Physical axis source, borrowed by identity only.
        Id action{0}; ///< Local semantic axis ID; cannot conflict with a button in this definition.
        AxisOptions options; ///< Finite scale/dead-zone and absolute/relative kind.
    };
    /** Owned context definition, copied/configured before acceptance; no device is borrowed.
     * IDs are local and reusable across contexts/direct mappings. Empty collections
     * create an empty namespace; button/axis conflicts and invalid mappings reject.
     * @see [Action contexts](cheryl-guide:runtime/input-state-model#action-contexts)
     */
    struct ContextDefinition {
        std::vector<ContextButton> buttons; ///< Owned button/chord entries.
        std::vector<ContextAxis> axes; ///< Owned additive axis entries.
    };

    /** One requested action namespace. Move/reset/destruction require exclusive
     * access to this handle, on any thread. Removal takes effect at the next poll;
     * previously published ticks/views retain their own state. A valid lease is
     * an accepted request, not proof that a poll has installed its mappings.
     */
    class Context final {
        std::shared_ptr<Detail::ContextToken> token_;
        CE_API explicit Context(std::shared_ptr<Detail::ContextToken> token);
        friend class ContextSubmission;

    public:
        Context() = default;
        CE_API ~Context();
        CE_API Context(Context&& other) noexcept;
        CE_API Context& operator=(Context&& other) noexcept;
        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        CE_API void reset() noexcept;
        [[nodiscard]] CE_API explicit operator bool() const noexcept;
        // Copy the identity when retaining an action view independently of the lease.
        [[nodiscard]] CE_API ContextId id() const noexcept;
    };

    /** Retained request endpoint acquired on the binding/poll owner. attach() may
     * run on any producer. Invalid definitions reject before acceptance; owner
     * application/publication can still fail. Input destruction closes the endpoint
     * and invalidates live leases; later attach calls reject safely.
     */
    class ContextSubmission final {
        std::shared_ptr<Detail::ContextsState> state_;
        CE_API explicit ContextSubmission(std::shared_ptr<Detail::ContextsState> state);
        friend class Bindings;

    public:
        ContextSubmission() = default;
        [[nodiscard]] CE_API Context attach(ContextDefinition definition) const;
    };
}

/** @fn CE::Input::Actions::Context::Context()
 * Create an empty lease with false bool and identity 0.
 */

/** @fn CE::Input::Actions::Context::~Context()
 * Mark this context for removal at the next poll; never rewrite already retained snapshots/ticks.
 */

/** @fn CE::Input::Actions::Context::Context(Context&& other) noexcept
 * Transfer the accepted request; the source becomes empty. Serialize access to both handles.
 */

/** @fn CE::Input::Actions::Context& CE::Input::Actions::Context::operator=(Context&& other) noexcept
 * Release the destination request for next-poll removal, then transfer other; no published history is changed.
 */

/** @fn void CE::Input::Actions::Context::reset() noexcept
 * Idempotently request removal and empty this handle. May run on any producer with exclusive access; detached held buttons publish release and axes zero at a later poll.
 */

/** @fn CE::Input::Actions::Context::operator bool() const noexcept
 * Return whether a live accepted/noninvalidated lease exists, not whether a poll has installed its mappings. Input destruction/clear invalidates leases.
 */

/** @fn CE::Input::Actions::ContextId CE::Input::Actions::Context::id() const noexcept
 * Copy the namespace identity,0 for empty. A retained nonzero identity can query old Tick state after reset/destruction; it is not a saved-game identifier.
 */

/** @fn CE::Input::Actions::ContextSubmission::ContextSubmission()
 * Create an empty endpoint; attach() rejects until an endpoint is acquired from live Bindings.
 */

/** @fn CE::Input::Actions::Context CE::Input::Actions::ContextSubmission::attach(ContextDefinition definition) const
 * Accept an owned definition on any producer, returning a move-only lease. Empty/closed endpoints or invalid mappings reject before acceptance; allocation failures may propagate. Application occurs at the next poll boundary, seeding held buttons/absolute axes/device gates but not previous relative deltas. Poll evaluation/publication may fail and retry while retaining the preceding complete snapshot. Destruction of input closes the endpoint; clear/deinitialize invalidates old leases without preventing new attachment.
 * @see [Request and polling boundaries](cheryl-guide:runtime/input-state-model#request-and-polling-boundaries)
 */
