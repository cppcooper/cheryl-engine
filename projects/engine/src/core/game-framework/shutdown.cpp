#include "shutdown-internal.h"
#include "../engine/view-state-internal.h"

#include <internals/exceptions.h>
#include <internals/failure-reporting.h>
#include <internals/compile-time-logging.hpp>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

namespace CE::Runtime::Detail {
    struct ShutdownPolicyState {
        mutable std::mutex mutex;
        std::condition_variable idle;
        bool active = true;
        std::size_t running = 0;
        const std::uint64_t generation = ViewSourceAccess::identity();
        std::weak_ptr<ShutdownState> owner;
        ShutdownPolicyCallback callback;
        ShutdownPolicyOptions options;
        CE::Events::EventBus::Delivery delivery;
        struct Entry {
            ShutdownPolicyState* policy;
            Entry* previous;
        };
        static thread_local Entry* entry;

        bool enabled() const noexcept {
            std::lock_guard lock(mutex);
            return active;
        }
        void invalidate() noexcept {
            std::lock_guard lock(mutex);
            active = false;
        }
        void wait() noexcept {
            for (auto* current = entry; current; current = current->previous)
                if (current->policy == this)
                    return;
            std::unique_lock lock(mutex);
            idle.wait(lock, [this] { return running == 0; });
        }
        void invoke(const ShutdownRequest& request, ShutdownDecision decision) {
            {
                std::lock_guard lock(mutex);
                if (!active)
                    return;
                ++running;
            }
            struct Guard {
                ShutdownPolicyState& policy;
                Entry current;
                explicit Guard(ShutdownPolicyState& value)
                : policy(value), current{&value, entry} {
                    entry = &current;
                }
                ~Guard() {
                    entry = current.previous;
                    std::lock_guard lock(policy.mutex);
                    --policy.running;
                    policy.idle.notify_all();
                }
            } guard(*this);
            callback(request, std::move(decision));
        }
    };
    thread_local ShutdownPolicyState::Entry* ShutdownPolicyState::entry = nullptr;

    struct ShutdownSlot {
        ShutdownRequest request;
        std::promise<ShutdownResult> completion;
        std::shared_future<ShutdownResult> result = completion.get_future().share();
        bool admitted = false;
        std::uint64_t expected_generation = 0;
        std::uint64_t generation = 0;
        std::optional<ShutdownResult> decision;
        std::shared_ptr<ShutdownPolicyState> policy;
        CE::Events::EventBus::Registration registration;
        CE::Events::EventBus::ErrorHandler errors;
    };

    namespace {
        struct PolicyDelivery {
            std::uint64_t request = 0;
            std::uint64_t generation = 0;
        };
        const CE::Events::EventChannel<PolicyDelivery> policy_event{"runtime-shutdown-policy"};

        const char* cancellation_name(ShutdownCancellation reason) noexcept {
            switch (reason) {
                case ShutdownCancellation::None: return "none";
                case ShutdownCancellation::HostRejected: return "host_rejected";
                case ShutdownCancellation::PolicyDetached: return "policy_detached";
                case ShutdownCancellation::PolicyFailed: return "policy_failed";
                case ShutdownCancellation::AdmissionFailed: return "admission_failed";
                case ShutdownCancellation::RuntimeClosed: return "runtime_closed";
            }
            return "unknown";
        }

        void finish(const std::shared_ptr<ShutdownSlot>& slot, ShutdownDisposition disposition, ShutdownCancellation reason) noexcept {
            if (!slot)
                return;
            slot->registration.disconnect();
            try {
                slot->completion.set_value({slot->request.id, disposition, reason});
            } catch (...) {
                CE::Diagnostics::report_failure("shutdown request result", std::current_exception());
            }
        }
    }

    struct ShutdownState {
        mutable std::mutex mutex;
        std::stop_source stop;
        std::shared_ptr<const std::function<void()>> wake;
        std::shared_ptr<ShutdownSlot> slot;
        std::shared_ptr<ShutdownPolicyState> policy;
        ShutdownStatus status = ShutdownStatus::Running;
        ShutdownCause cause = ShutdownCause::None;
        bool begun = false;
        bool closed = false;

        ShutdownState(std::stop_source source, std::function<void()> signal)
        : stop(std::move(source)), wake(std::make_shared<const std::function<void()>>(std::move(signal))) {}

        void ping() const noexcept {
            std::shared_ptr<const std::function<void()>> signal;
            {
                std::lock_guard lock(mutex);
                signal = wake;
            }
            try {
                if (signal && *signal)
                    (*signal)();
            } catch (...) {
                CE::Diagnostics::report_failure("shutdown owner wake", std::current_exception());
            }
        }
        ShutdownTicket request(ShutdownReason reason) {
            if (reason != ShutdownReason::HostRequest && reason != ShutdownReason::MainWindowClose)
                throw CE::Exceptions::invalid_args(CE_HERE, "Unknown shutdown request reason");
            std::shared_ptr<ShutdownSlot> selected;
            {
                std::lock_guard lock(mutex);
                if (closed || status >= ShutdownStatus::Committed || stop.stop_requested())
                    throw CE::Exceptions::failed_operation(CE_HERE, "Shutdown command endpoint is closed or committed");
                if (!slot) {
                    auto prepared = std::make_shared<ShutdownSlot>();
                    prepared->request = {ViewSourceAccess::identity(), reason, std::chrono::steady_clock::now()};
                    if (policy)
                        prepared->expected_generation = policy->generation;
                    slot = std::move(prepared);
                }
                selected = slot;
            }
            ping();
            return {selected->request, selected->result};
        }
        bool decide(std::uint64_t request, std::uint64_t generation, bool accept, ShutdownCancellation reason) {
            {
                std::lock_guard lock(mutex);
                if (closed || status != ShutdownStatus::Pending || stop.stop_requested() || !slot || !slot->admitted ||
                    slot->request.id != request || slot->generation != generation || slot->decision)
                    return false;
                slot->decision = ShutdownResult{request, accept ? ShutdownDisposition::Accepted : ShutdownDisposition::Cancelled, reason};
            }
            ping();
            return true;
        }
        void detach(std::uint64_t generation) noexcept {
            CE::Events::EventBus::Registration registration;
            std::shared_ptr<ShutdownPolicyState> retired;
            {
                std::lock_guard lock(mutex);
                if (!policy || policy->generation != generation)
                    return;
                retired = std::move(policy);
                if (slot && slot->admitted && slot->generation == generation && status == ShutdownStatus::Pending) {
                    slot->decision = ShutdownResult{slot->request.id, ShutdownDisposition::Cancelled, ShutdownCancellation::PolicyDetached};
                    registration = slot->registration;
                }
            }
            registration.disconnect();
            ping();
        }
        void force(ShutdownCause reason) noexcept {
            std::shared_ptr<ShutdownSlot> retired;
            std::shared_ptr<ShutdownPolicyState> authority;
            {
                std::lock_guard lock(mutex);
                authority = policy;
                if (!closed && status < ShutdownStatus::Committed) {
                    status = ShutdownStatus::Committed;
                    cause = reason;
                    retired = std::move(slot);
                }
            }
            if (authority)
                authority->invalidate();
            finish(retired, ShutdownDisposition::Superseded, ShutdownCancellation::None);
            static_cast<void>(stop.request_stop());
            ping();
        }
        void close() noexcept {
            std::shared_ptr<ShutdownSlot> retired;
            std::shared_ptr<ShutdownPolicyState> retired_policy;
            {
                std::lock_guard lock(mutex);
                closed = true;
                status = ShutdownStatus::Stopped;
                retired = std::move(slot);
                retired_policy = std::move(policy);
                wake.reset();
            }
            if (retired_policy)
                retired_policy->invalidate();
            finish(retired, ShutdownDisposition::Rejected, ShutdownCancellation::RuntimeClosed);
        }
    };

    struct ShutdownController::Bridge {
        std::shared_ptr<ShutdownState> state;
        std::thread::id owner;
        std::mutex mutex;
        const CE::Platform::WindowCloseRequested* operation = nullptr;
        bool entered = false;
        std::exception_ptr failure;

        void enter(const CE::Platform::WindowCloseRequested& event) noexcept {
            {
                std::lock_guard lock(mutex);
                if (owner != std::this_thread::get_id() || !operation || entered || event != *operation)
                    return;
                entered = true;
            }
            try {
                bool forced = false;
                {
                    std::lock_guard lock(state->mutex);
                    if (state->closed || state->status >= ShutdownStatus::Committed || state->stop.stop_requested())
                        return;
                    forced = state->policy && state->policy->options.native_close == NativeClosePolicy::Force;
                }
                if (forced)
                    state->force(ShutdownCause::WindowClose);
                else
                    static_cast<void>(state->request(ShutdownReason::MainWindowClose));
            } catch (...) {
                if (state->stop.stop_requested())
                    return;
                std::lock_guard lock(mutex);
                failure = std::current_exception();
            }
        }
    };

    ShutdownController::ShutdownController(std::stop_source stop, std::function<void()> wake)
    : state_(std::make_shared<ShutdownState>(std::move(stop), std::move(wake))), bridge_(std::make_shared<Bridge>()) {
        bridge_->state = state_;
    }
    ShutdownController::~ShutdownController() {
        close();
    }
    ShutdownEndpoint ShutdownController::endpoint() const {
        return ShutdownEndpoint(state_);
    }
    ShutdownPolicy
    ShutdownController::policy(ShutdownPolicyCallback callback, ShutdownPolicyOptions options, CE::Events::EventBus::Delivery delivery) {
        if (!callback)
            throw CE::Exceptions::invalid_args(CE_HERE, "Shutdown policy requires a callback");
        if (options.native_close != NativeClosePolicy::Negotiate && options.native_close != NativeClosePolicy::Force)
            throw CE::Exceptions::invalid_args(CE_HERE, "Unknown native close policy");
        auto prepared = std::make_shared<ShutdownPolicyState>();
        prepared->owner = state_;
        prepared->callback = std::move(callback);
        prepared->options = options;
        prepared->delivery = std::move(delivery);
        {
            std::lock_guard lock(state_->mutex);
            if (state_->begun || state_->closed || state_->policy || state_->status >= ShutdownStatus::Committed || state_->stop.stop_requested())
                throw CE::Exceptions::failed_operation(CE_HERE, "Install one shutdown policy before the runtime starts");
            state_->policy = prepared;
        }
        return ShutdownPolicy(std::move(prepared));
    }
    void ShutdownController::begin(CE::Events::EventBus& bus) {
        auto registration =
            bus.register_listener(CE::Platform::window_close_requested_event, [weak = std::weak_ptr(bridge_)](const auto& event) {
                if (const auto bridge = weak.lock())
                    bridge->enter(event);
            });
        {
            std::lock_guard lock(state_->mutex);
            state_->begun = true;
        }
        {
            std::lock_guard lock(bridge_->mutex);
            bridge_->owner = std::this_thread::get_id();
        }
        bridge_registration_ = registration;
    }

    void ShutdownController::process(CE::Events::EventBus& bus, RuntimeLifecycle& lifecycle, InitializedCapabilities initialized) {
        if (processing_)
            return;
        processing_ = true;
        struct Guard {
            bool& flag;
            ~Guard() { flag = false; }
        } guard{processing_};
        if (state_->stop.stop_requested()) {
            state_->force(ShutdownCause::StopRequested);
            return;
        }
        std::shared_ptr<ShutdownSlot> selected;
        std::shared_ptr<ShutdownPolicyState> policy;
        {
            std::lock_guard lock(state_->mutex);
            if (state_->closed || state_->status >= ShutdownStatus::Committed || !state_->slot)
                return;
            selected = state_->slot;
            policy = state_->policy;
        }
        if (!selected->admitted) {
            CE::Events::EventBus::Registration registration;
            CE::Events::EventBus::ErrorHandler errors;
            try {
                const auto generation = policy ? policy->generation : 0;
                if ((selected->expected_generation && selected->expected_generation != generation) || (policy && !policy->enabled()))
                    throw CE::Exceptions::failed_operation(CE_HERE, "Shutdown policy was withdrawn before admission");
                if (policy) {
                    errors = [weak = std::weak_ptr(state_), request = selected->request.id, generation](std::exception_ptr error) {
                        CE::Diagnostics::report_failure("shutdown policy delivery", error);
                        if (const auto state = weak.lock())
                            static_cast<void>(state->decide(request, generation, false, ShutdownCancellation::PolicyFailed));
                    };
                    const auto decision = ShutdownDecision(state_, selected->request.id, generation);
                    registration = bus.register_listener(
                        policy_event,
                        [policy, request = selected->request, decision, errors](const PolicyDelivery& event) {
                            if (event.request != request.id || event.generation != policy->generation)
                                return;
                            try {
                                policy->invoke(request, decision);
                            } catch (...) {
                                errors(std::current_exception());
                            }
                        },
                        policy->delivery, errors
                    );
                }
                std::optional<RuntimeLifecycleSnapshot> current;
                {
                    std::lock_guard lock(state_->mutex);
                    if (state_->slot == selected && state_->status < ShutdownStatus::Committed && !state_->stop.stop_requested() &&
                        state_->policy == policy && (!policy || policy->enabled())) {
                        current = lifecycle.pending(selected->request, initialized);
                        selected->generation = generation;
                        selected->policy = policy;
                        selected->registration = registration;
                        selected->errors = std::move(errors);
                        selected->admitted = true;
                        state_->status = ShutdownStatus::Pending;
                    }
                }
                if (!current) {
                    registration.disconnect();
                    return;
                }
                CE_LOG_DEBUG(
                    CE::Logging::Categories::engine,
                    "subsystem=events event=shutdown-requested operation=notification_attempt session={} sequence={} request={} reason={}",
                    current->session, current->sequence, selected->request.id,
                    selected->request.reason == ShutdownReason::MainWindowClose ? "window_close" : "host_request"
                );
                bus.notify(shutdown_requested_event, ShutdownRequested{*current, selected->request});
                if (policy)
                    bus.notify(policy_event, PolicyDelivery{selected->request.id, generation}, selected->errors);
            } catch (...) {
                registration.disconnect();
                std::shared_ptr<ShutdownSlot> retired;
                {
                    std::lock_guard lock(state_->mutex);
                    if (state_->slot == selected && !selected->admitted)
                        retired = std::move(state_->slot);
                }
                finish(retired, ShutdownDisposition::Rejected, ShutdownCancellation::AdmissionFailed);
                CE_LOG_DEBUG(CE::Logging::Categories::engine,
                    "subsystem=shutdown operation=request_admission outcome=rejected request={}", selected->request.id);
                CE::Diagnostics::report_failure("shutdown request admission", std::current_exception());
                return;
            }
        }

        std::optional<ShutdownResult> decision;
        std::optional<RuntimeLifecycleSnapshot> cancelled;
        {
            std::lock_guard lock(state_->mutex);
            if (state_->slot != selected || state_->status != ShutdownStatus::Pending || state_->stop.stop_requested())
                return;
            if (selected->decision)
                decision = selected->decision;
            else if (!selected->policy)
                decision = ShutdownResult{selected->request.id, ShutdownDisposition::Accepted, ShutdownCancellation::None};
            if (!decision)
                return;
            if (decision->disposition == ShutdownDisposition::Accepted) {
                state_->status = ShutdownStatus::Committed;
                state_->cause = ShutdownCause::AcceptedRequest;
            } else {
                cancelled = lifecycle.pending({}, initialized);
                state_->status = ShutdownStatus::Running;
            }
            state_->slot.reset(); // selected pins all captures until after unlock.
        }
        if (decision->disposition == ShutdownDisposition::Accepted) {
            if (selected->policy)
                selected->policy->invalidate();
            static_cast<void>(state_->stop.request_stop());
        }
        finish(selected, decision->disposition, decision->cancellation);
        if (cancelled) {
            CE_LOG_DEBUG(
                CE::Logging::Categories::engine,
                "subsystem=events event=shutdown-request-cancelled operation=notification_attempt session={} sequence={} request={} reason={}",
                cancelled->session, cancelled->sequence, selected->request.id, cancellation_name(decision->cancellation)
            );
            bus.notify(shutdown_request_cancelled_event, ShutdownRequestCancelled{*cancelled, selected->request, decision->cancellation});
        }
    }

    void ShutdownController::window_close(CE::Events::EventBus& bus, const CE::Platform::WindowCloseRequested& intent) {
        {
            std::lock_guard lock(bridge_->mutex);
            if (bridge_->operation)
                return;
            bridge_->operation = &intent;
            bridge_->entered = false;
            bridge_->failure = {};
        }
        bus.notify(CE::Platform::window_close_requested_event, intent);
        bridge_->enter(intent); // Entry gate supplies missed-delivery fallback.
        std::exception_ptr failure;
        {
            std::lock_guard lock(bridge_->mutex);
            bridge_->operation = nullptr;
            failure = bridge_->failure;
        }
        if (failure) {
            CE_LOG_DEBUG(CE::Logging::Categories::engine,
                "subsystem=shutdown operation=native_intent_admission outcome=rejected source={} intent={}", intent.source, intent.intent);
            CE::Diagnostics::report_failure("native close request admission", failure);
        }
    }
    void ShutdownController::force(ShutdownCause cause) noexcept {
        state_->force(cause);
    }
    void ShutdownController::quiescing() noexcept {
        std::lock_guard lock(state_->mutex);
        if (state_->status == ShutdownStatus::Committed)
            state_->status = ShutdownStatus::Quiescing;
    }
    void ShutdownController::close() noexcept {
        bridge_registration_.disconnect();
        state_->close();
    }
    bool ShutdownController::has_pending() const noexcept {
        std::lock_guard lock(state_->mutex);
        return state_->slot && (!state_->slot->admitted || state_->slot->decision.has_value());
    }
    ShutdownCause ShutdownController::cause() const noexcept {
        std::lock_guard lock(state_->mutex);
        return state_->cause;
    }
}

namespace CE::Runtime {
    ShutdownTicket ShutdownEndpoint::request(ShutdownReason reason) const {
        if (!state_)
            throw CE::Exceptions::failed_operation(CE_HERE, "Empty shutdown command endpoint");
        return state_->request(reason);
    }
    ShutdownSnapshot ShutdownEndpoint::snapshot() const {
        if (!state_)
            return {ShutdownStatus::Stopped, {}, false, true, ShutdownCause::None};
        std::lock_guard lock(state_->mutex);
        auto status = state_->status;
        if (state_->stop.stop_requested() && status < ShutdownStatus::Committed)
            status = ShutdownStatus::Committed;
        return {
            status, state_->slot ? std::optional{state_->slot->request} : std::nullopt, state_->slot && state_->slot->admitted,
            state_->closed, state_->cause
        };
    }
    ShutdownDecision::ShutdownDecision(std::weak_ptr<Detail::ShutdownState> state, std::uint64_t request, std::uint64_t generation) noexcept
    : state_(std::move(state)), request_(request), generation_(generation) {}
    bool ShutdownDecision::accept() const {
        const auto state = state_.lock();
        return state && state->decide(request_, generation_, true, ShutdownCancellation::None);
    }
    bool ShutdownDecision::cancel() const {
        const auto state = state_.lock();
        return state && state->decide(request_, generation_, false, ShutdownCancellation::HostRejected);
    }
    ShutdownPolicy::~ShutdownPolicy() {
        reset();
    }
    ShutdownPolicy::ShutdownPolicy(ShutdownPolicy&& other) noexcept
    : state_(std::move(other.state_)) {}
    ShutdownPolicy& ShutdownPolicy::operator=(ShutdownPolicy&& other) noexcept {
        if (this != &other) {
            reset();
            state_ = std::move(other.state_);
        }
        return *this;
    }
    void ShutdownPolicy::reset() noexcept {
        if (const auto policy = std::exchange(state_, {})) {
            policy->invalidate();
            if (const auto owner = policy->owner.lock())
                owner->detach(policy->generation);
            policy->wait();
        }
    }
    bool ShutdownPolicy::active() const noexcept {
        return state_ && state_->enabled();
    }
}
