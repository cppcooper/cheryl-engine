#pragma once

#include <cheryl/export.h>
#include <core/subsystems/event-channel.h>

#include <cstdint>
#include <memory>

namespace CE::Rendering {
    namespace Detail {
        struct GraphicsDomainState;
    }

    /** Native resource usability; an uninitialized domain has sequence zero. */
    enum class GraphicsDomainStatus {
        Unavailable, ///< Uninitialized or permanently invalidated; inspect sequence/reason.
        Available    ///< Initialized native domain; normal backend affinity still applies.
    };
    /** Cause of the latest committed transition, independent of facade ownership. */
    enum class GraphicsDomainReason {
        NotInitialized, ///< Native initialization has not committed.
        Initialized,    ///< Mandatory initialization committed successfully.
        Shutdown,       ///< Native resources were deleted and the domain deactivated.
        Abandoned       ///< Native cleanup could not run; context destruction owns remaining resources.
    };

    /** Value-only observation; availability does not authorize foreign-thread/context use. */
    struct GraphicsDomainSnapshot {
        std::uint64_t id = 0;       ///< Permanent process-local identity; zero denotes an unsupported/empty token.
        std::uint64_t sequence = 0; ///< Zero before initialization, one when available, two after permanent invalidation.
        GraphicsDomainStatus status = GraphicsDomainStatus::Unavailable;    ///< Usability of native resources.
        GraphicsDomainReason reason = GraphicsDomainReason::NotInitialized; ///< Committed lifecycle cause.
        bool operator==(const GraphicsDomainSnapshot&) const = default;
    };

    /** Copyable observation token retaining no renderer, context, window or native objects.
     * Copies may cross threads and remain readable after facade destruction. A
     * replacement renderer has a new identity; an invalidated token never reopens.
     */
    class GraphicsDomain final {
        friend class GraphicsDomainOwner;
        std::shared_ptr<Detail::GraphicsDomainState> state_;
        explicit GraphicsDomain(std::shared_ptr<Detail::GraphicsDomainState> state) noexcept;

    public:
        GraphicsDomain() = default;
        [[nodiscard]] CE_API GraphicsDomainSnapshot snapshot() const noexcept;
    };

    /** Backend lifecycle authority for a token, independent of notification routing.
     * Construct before native effects; allocation or identity exhaustion throws.
     * Serialize mutations on the native owner. Destruction invalidates a still-live
     * domain as Abandoned without callbacks or native calls.
     */
    class GraphicsDomainOwner final {
        GraphicsDomain domain_;

    public:
        CE_API GraphicsDomainOwner();
        CE_API ~GraphicsDomainOwner();
        GraphicsDomainOwner(const GraphicsDomainOwner&) = delete;
        GraphicsDomainOwner& operator=(const GraphicsDomainOwner&) = delete;
        [[nodiscard]] GraphicsDomain token() const noexcept { return domain_; }
        CE_API void make_available() noexcept;
        CE_API void shutdown() noexcept;
        CE_API void abandon() noexcept;
    };

    /** Owned transition selected after the complete renderer operation.
     * current is immutable historical state; domain.snapshot() can already be newer.
     * Register and read the token seed on the graphics publication owner. Use id
     * and sequence to reconcile deferred/late observations.
     */
    struct GraphicsDomainStateChanged {
        GraphicsDomainSnapshot current; ///< Committed transition, independent of later changes.
        GraphicsDomain domain;          ///< Retained live observation, without native ownership.
    };

    /** Rare native-domain lifecycle transitions, published on the graphics owner. */
    inline const CE::Events::EventChannel<GraphicsDomainStateChanged> graphics_domain_state_changed_event{"graphics-domain-state-changed"};
}

/** @fn CE::Rendering::GraphicsDomain::GraphicsDomain()
 * Create an empty, unsupported token. snapshot() returns identity/sequence zero.
 */

/** @fn CE::Rendering::GraphicsDomainSnapshot CE::Rendering::GraphicsDomain::snapshot() const noexcept
 * Copy coherent identity, sequence and usability from any thread without native
 * queries. Register and read the initial seed on the publication owner when an
 * ordered subscription is required. Retained copies never keep native state alive.
 */

/** @fn CE::Rendering::GraphicsDomain CE::Rendering::GraphicsDomainOwner::token() const noexcept
 * Retain observation state independently of this backend authority and its context.
 */

/** @fn void CE::Rendering::GraphicsDomainOwner::make_available() noexcept
 * Commit initialization once, after mandatory native setup succeeds. Later calls
 * have no effect; a terminal domain cannot reopen. Performs no notification.
 */

/** @fn void CE::Rendering::GraphicsDomainOwner::shutdown() noexcept
 * Permanently invalidate an available domain after actual native deletion, without
 * callbacks. An uninitialized or already terminal domain is unchanged.
 */

/** @fn void CE::Rendering::GraphicsDomainOwner::abandon() noexcept
 * Permanently invalidate an available domain without native calls or callbacks.
 * Use when native cleanup cannot run; preserve an already committed Shutdown reason.
 */
