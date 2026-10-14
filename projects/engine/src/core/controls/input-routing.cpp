#include <core/controls/input-routing.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::Input::Routing {
    Lease::Lease(std::shared_ptr<Detail::State> state, std::shared_ptr<const Focus> focus)
    : state_(std::move(state)), focus_(std::move(focus)) {}

    Lease::~Lease() {
        reset();
    }

    Lease::Lease(Lease&& other) noexcept
    : state_(std::move(other.state_)), focus_(std::move(other.focus_)) {}

    Lease& Lease::operator=(Lease&& other) noexcept {
        if (this != &other) {
            reset();
            state_ = std::move(other.state_);
            focus_ = std::move(other.focus_);
        }
        return *this;
    }

    void Lease::reset() noexcept {
        if (state_ && focus_) {
            auto expected = focus_;
            // Pointer identity includes the request's epoch. A stale lease cannot
            // release the next owner, even if both refer to the same widget ID.
            (void)state_->current.compare_exchange_strong(expected, state_->empty);
        }
        focus_.reset();
        state_.reset();
    }

    bool Lease::owns_focus() const {
        return state_ && focus_ && state_->current.load() == focus_;
    }

    Lease Manager::focus(const TargetId target, const KeyboardPolicy routing) {
        if (target == 0 || (routing != KeyboardPolicy::Exclusive && routing != KeyboardPolicy::PassThrough))
            throw Exceptions::invalid_args(CE_HERE, "Keyboard focus requires a nonzero owner and a valid routing policy");
        auto request = std::make_shared<Focus>(Focus{target, routing, state_->next_epoch.fetch_add(1)});
        state_->current.store(request);
        return Lease(state_, std::move(request));
    }
}
