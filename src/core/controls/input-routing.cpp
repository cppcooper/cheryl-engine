#include <core/controls/input-routing.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::Input {
    FocusLease::FocusLease(std::shared_ptr<Detail::RoutingState> state, std::shared_ptr<const KeyboardFocus> focus)
        : state_(std::move(state)), focus_(std::move(focus)) {}
    FocusLease::~FocusLease() { reset(); }
    FocusLease::FocusLease(FocusLease&& other) noexcept : state_(std::move(other.state_)), focus_(std::move(other.focus_)) {}
    FocusLease& FocusLease::operator=(FocusLease&& other) noexcept {
        if (this != &other) {
            reset();
            state_ = std::move(other.state_);
            focus_ = std::move(other.focus_);
        }
        return *this;
    }
    void FocusLease::reset() noexcept {
        if (state_ && focus_) {
            auto expected = focus_;
            // Pointer identity includes the request's epoch. A stale lease cannot
            // release the next owner, even if both refer to the same widget ID.
            (void)state_->current.compare_exchange_strong(expected, state_->empty);
        }
        focus_.reset();
        state_.reset();
    }
    bool FocusLease::owns_focus() const { return state_ && focus_ && state_->current.load() == focus_; }

    FocusLease InputRouting::focus(const FocusId target, const KeyboardRouting routing) {
        if (target == 0 || (routing != KeyboardRouting::Exclusive && routing != KeyboardRouting::PassThrough))
            throw Exceptions::invalid_args(CE_HERE, "Keyboard focus requires a nonzero owner and a valid routing policy");
        auto request = std::make_shared<KeyboardFocus>(KeyboardFocus{target, routing, state_->next_epoch.fetch_add(1)});
        state_->current.store(request);
        return FocusLease(state_, std::move(request));
    }
}
