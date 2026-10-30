#include <core/rendering/graphics-domain.h>

#include <core/diagnostics.h>
#include <internals/exceptions.h>

#include <atomic>
#include <utility>

namespace CE::Rendering {
    struct Detail::GraphicsDomainState {
        const std::uint64_t id = CE::Diagnostics::next_domain_id();
        // A single atomic keeps state, sequence and terminal reason coherent.
        std::atomic<GraphicsDomainReason> reason{GraphicsDomainReason::NotInitialized};
    };

    GraphicsDomain::GraphicsDomain(std::shared_ptr<Detail::GraphicsDomainState> state) noexcept
    : state_(std::move(state)) {}

    GraphicsDomainSnapshot GraphicsDomain::snapshot() const noexcept {
        if (!state_)
            return {};
        const auto reason = state_->reason.load(std::memory_order_acquire);
        const auto sequence = reason == GraphicsDomainReason::NotInitialized ? 0u : reason == GraphicsDomainReason::Initialized ? 1u : 2u;
        return {
            state_->id, sequence,
            reason == GraphicsDomainReason::Initialized ? GraphicsDomainStatus::Available : GraphicsDomainStatus::Unavailable, reason
        };
    }

    GraphicsDomainOwner::GraphicsDomainOwner()
    : domain_(std::make_shared<Detail::GraphicsDomainState>()) {
        if (!domain_.snapshot().id)
            throw CE::Exceptions::failed_operation(CE_HERE, "Graphics domain identities exhausted");
    }

    GraphicsDomainOwner::~GraphicsDomainOwner() {
        abandon();
    }

    void GraphicsDomainOwner::make_available() noexcept {
        auto expected = GraphicsDomainReason::NotInitialized;
        domain_.state_->reason.compare_exchange_strong(expected, GraphicsDomainReason::Initialized, std::memory_order_acq_rel);
    }

    void GraphicsDomainOwner::shutdown() noexcept {
        auto expected = GraphicsDomainReason::Initialized;
        domain_.state_->reason.compare_exchange_strong(expected, GraphicsDomainReason::Shutdown, std::memory_order_acq_rel);
    }

    void GraphicsDomainOwner::abandon() noexcept {
        auto expected = GraphicsDomainReason::Initialized;
        domain_.state_->reason.compare_exchange_strong(expected, GraphicsDomainReason::Abandoned, std::memory_order_acq_rel);
    }
}
