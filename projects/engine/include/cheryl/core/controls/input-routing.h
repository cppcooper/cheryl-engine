#pragma once

#include <cheryl/export.h>

#include "input-types.h"

#include <atomic>
#include <memory>

namespace CE::Input::Routing {
    /** Immutable copied focus request, latched once before an input pump. */
    struct Focus {
        TargetId target = 0; ///< Focused application target;0 denotes no owner.
        KeyboardPolicy routing = KeyboardPolicy::Exclusive; ///< Focused keyboard gameplay-admission policy.
        std::uint64_t epoch = 0; ///< Request identity;0 denotes no request, separate from widget lifetime.
    };

    namespace Detail {
        struct State {
            std::shared_ptr<const CE::Input::Routing::Focus> empty = std::make_shared<CE::Input::Routing::Focus>();
            std::atomic<std::shared_ptr<const CE::Input::Routing::Focus>> current{empty};
            std::atomic<std::uint64_t> next_epoch{1};
        };
    }

    /** Scoped keyboard-focus ownership. Releasing an old owner cannot clear
     * a newer focus request, including one using the same target ID.
     * Move/release on any thread with exclusive access to this handle. Retains only
     * routing state, not the widget/adapter. Release clears focus without restoring older requests.
     */
    class Lease final {
        friend class Manager;

    public:
        Lease() = default;
        CE_API ~Lease();
        CE_API Lease(Lease&& other) noexcept;
        CE_API Lease& operator=(Lease&& other) noexcept;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        CE_API void reset() noexcept;
        // Snapshot only; ownership can change immediately after this observation.
        [[nodiscard]] CE_API bool owns_focus() const;
        [[nodiscard]] TargetId target() const { return focus_ ? focus_->target : 0; }
        [[nodiscard]] std::uint64_t epoch() const { return focus_ ? focus_->epoch : 0; }

    private:
        CE_API Lease(std::shared_ptr<Detail::State> state, std::shared_ptr<const Focus> focus);
        std::shared_ptr<Detail::State> state_;
        std::shared_ptr<const Focus> focus_;
    };

    /** Thread-safe focus requests, independent of State mapping and capture.
     * The platform latches current() at the next poll. UI owners inspect routed
     * immutable records in simulation; no widget is called on the platform thread.
     */
    class Manager final {
    public:
        Manager() = default;
        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;
        // Nonzero target/valid policy required; atomic publication wins among concurrent
        // requests. Epoch identifies a request, not a widget's lifetime or capture lease.
        [[nodiscard]] CE_API Lease focus(TargetId target, KeyboardPolicy routing = KeyboardPolicy::Exclusive);
        [[nodiscard]] std::shared_ptr<const Focus> current() const { return state_->current.load(); }
        // Clear current ownership without changing capture activation or published records.
        void clear() { state_->current.store(state_->empty); }

    private:
        std::shared_ptr<Detail::State> state_ = std::make_shared<Detail::State>();
    };
}

/** @fn CE::Input::Routing::Lease::Lease()
 * Create an empty lease with target/epoch0 and no ownership.
 */

/** @fn CE::Input::Routing::Lease::~Lease()
 * Release only this request; never clear a newer owner or restore an older request.
 */

/** @fn CE::Input::Routing::Lease::Lease(Lease&& other) noexcept
 * Transfer retained request state; source becomes empty. Serialize access to both handle objects.
 */

/** @fn CE::Input::Routing::Lease& CE::Input::Routing::Lease::operator=(Lease&& other) noexcept
 * Reset this request then transfer other. Old-owner release cannot affect a newer focus publication.
 */

/** @fn void CE::Input::Routing::Lease::reset() noexcept
 * Idempotently release this request if still current, then empty the handle. May run on any thread with exclusive handle access; collected records remain routed to their old target/epoch.
 */

/** @fn bool CE::Input::Routing::Lease::owns_focus() const
 * Observe whether this request is still current; another producer can replace ownership immediately after return.
 */

/** @fn CE::Input::Routing::TargetId CE::Input::Routing::Lease::target() const
 * Return this retained request's target,0 for empty. A preempted lease keeps its original target until reset.
 */

/** @fn std::uint64_t CE::Input::Routing::Lease::epoch() const
 * Return request epoch,0 for empty; preemption alone does not change it.
 */

/** @fn CE::Input::Routing::Manager::Manager()
 * Create synchronized empty focus state. Allocation may throw; no widget, capture channel or device mapping is owned.
 */

/** @fn CE::Input::Routing::Lease CE::Input::Routing::Manager::focus(TargetId target, KeyboardPolicy routing)
 * Publish a new focus request atomically among producers. Require nonzero target and known policy; invalid arguments/allocation failures throw. New epochs identify requests even when reusing a target; the platform latches current() for its next poll.
 * @see [Focus routing](cheryl-guide:runtime/input-state-model#focus-routing-and-editing-controls)
 */

/** @fn std::shared_ptr<const CE::Input::Routing::Focus> CE::Input::Routing::Manager::current() const
 * Atomically retain the current immutable request, or a nonnull empty Focus snapshot; does not latch an input poll itself.
 */

/** @fn void CE::Input::Routing::Manager::clear()
 * Clear ownership atomically without modifying capture activation, device mappings or records already published. Outstanding old leases cannot restore ownership.
 */
