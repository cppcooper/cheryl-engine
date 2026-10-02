#pragma once

#include "input-types.h"

#include <atomic>
#include <memory>

namespace CE::Input {
    struct KeyboardFocus {
        FocusId target = 0;
        KeyboardRouting routing = KeyboardRouting::Exclusive;
        std::uint64_t epoch = 0;
    };

    namespace Detail {
        struct RoutingState {
            std::shared_ptr<const KeyboardFocus> empty = std::make_shared<KeyboardFocus>();
            std::atomic<std::shared_ptr<const KeyboardFocus>> current{empty};
            std::atomic<std::uint64_t> next_epoch{1};
        };
    }

    /** Scoped keyboard-focus ownership. Releasing an old owner cannot clear
     * a newer focus request, including one using the same target ID.
     */
    class FocusLease final {
        friend class InputRouting;

    public:
        FocusLease() = default;
        ~FocusLease();
        FocusLease(FocusLease&& other) noexcept;
        FocusLease& operator=(FocusLease&& other) noexcept;
        FocusLease(const FocusLease&) = delete;
        FocusLease& operator=(const FocusLease&) = delete;
        void reset() noexcept;
        [[nodiscard]] bool owns_focus() const;
        [[nodiscard]] FocusId target() const { return focus_ ? focus_->target : 0; }
        [[nodiscard]] std::uint64_t epoch() const { return focus_ ? focus_->epoch : 0; }

    private:
        FocusLease(std::shared_ptr<Detail::RoutingState> state, std::shared_ptr<const KeyboardFocus> focus);
        std::shared_ptr<Detail::RoutingState> state_;
        std::shared_ptr<const KeyboardFocus> focus_;
    };

    /** Thread-safe focus requests, independent of State mapping and capture.
     * The platform latches current() at the next poll. UI owners inspect routed
     * immutable records in simulation; no widget is called on the platform thread.
     */
    class InputRouting final {
    public:
        InputRouting() = default;
        InputRouting(const InputRouting&) = delete;
        InputRouting& operator=(const InputRouting&) = delete;
        [[nodiscard]] FocusLease focus(FocusId target, KeyboardRouting routing = KeyboardRouting::Exclusive);
        [[nodiscard]] std::shared_ptr<const KeyboardFocus> current() const { return state_->current.load(); }
        void clear() { state_->current.store(state_->empty); }

    private:
        std::shared_ptr<Detail::RoutingState> state_ = std::make_shared<Detail::RoutingState>();
    };
}
