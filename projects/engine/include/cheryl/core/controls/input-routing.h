#pragma once

#include <cheryl/export.h>

#include "input-types.h"

#include <atomic>
#include <memory>

namespace CE::Input::Routing {
    struct Focus {
        TargetId target = 0;
        KeyboardPolicy routing = KeyboardPolicy::Exclusive;
        std::uint64_t epoch = 0;
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
