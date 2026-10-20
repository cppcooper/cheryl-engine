#pragma once

#include <cheryl/export.h>

#include <core/controls/polling-backlog.h>
#include <core/engine/simulation-dispatcher.h>
#include <core/engine/view-state.h>
#include <core/engine/runtime-services.h>
#include <core/diagnostics.h>

#include "simulation-scheduler.h"
#include "runtime-lifecycle.h"
#include "shutdown.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

namespace CE::Runtime {
    class Context;
}

namespace CE::Game {
    struct AbstractGame;
}

namespace CE::Runtime {
    namespace Detail {
        struct GameRuntimeAccess;
        class RuntimePressureState;
        struct RuntimePressureChanged;
        struct RuntimePressureWatch;
        class ShutdownController;
    }

    /** Placement of simulation relative to the calling platform owner. */
    enum class RunMode { Sequential, ///< Run update, frame preparation and native rendering on the caller.
        Concurrent ///< Run update/frame preparation on one simulation worker; native work remains on the caller.
    };

    /** Coordinates platform input, game simulation, rendering, presentation, and teardown.
     * Each scheduled update receives accumulated State activity and the selected
     * simulation delta, with observation time available separately. Both modes
     * use the same variable/fixed timing and bounded recovery policy.
     * Rendering consumes published render state, never the game's
     * live mutable simulation state. These boundaries
     * apply in either mode. The calling thread owns platform polling and graphics operations;
     * concurrent mode gives update and frame preparation to one simulation worker.
     * Sequential mode uses one recycled frame. Concurrent mode uses three slots
     * and renders the newest complete frame available at each handoff.
     */
    class GameRuntime final {
        static constexpr std::chrono::milliseconds resource_maintenance_interval{10};
        ::CE::Runtime::Context& engine_;
        ::CE::Game::AbstractGame& game_;
        ::CE::Runtime::RunMode mode_;
        Input::PollingOptions polling_;
        ::CE::Runtime::Simulation::TimingOptions timing_;
        ::CE::Runtime::RuntimeStats diagnostics_{::CE::Diagnostics::next_domain_id()};
        const char* phase_ = "reserve_session"; // Platform owner; retained first failing phase.
        std::atomic<bool> run_finished_{false};
        std::uint64_t observed_focus_epoch_ = 0; // Platform owner.
        ::CE::Runtime::Simulation::Clock::time_point next_timing_report_;
        ::CE::Runtime::Simulation::Clock::time_point next_platform_report_;
        ::CE::Runtime::Simulation::Clock::time_point pressure_started_;
        std::uint64_t reported_drops_ = 0;
        bool lag_warning_ = false;      // Simulation owner.
        bool pressure_warning_ = false; // Platform owner.
        bool renderer_ready_ = false; // Platform-owned; partial initialization is not maintenance-ready.
        std::atomic<bool> run_started_{false};
        std::stop_source stop_source_;
        struct Scheduler {
            std::mutex mutex;
            std::condition_variable wake;
        };
        std::shared_ptr<Scheduler> scheduler_ = std::make_shared<Scheduler>();
        ::CE::Runtime::SimulationDispatcher simulation_dispatcher_;
        std::function<std::thread(std::function<void()>)> simulation_thread_factory_;
        std::shared_ptr<::CE::Runtime::Detail::RuntimePressureState> pressure_state_;
        bool publishing_pressure_ = false; // Platform-owned; nested diagnoses wait for the next safe pass.
        RuntimeLifecycle lifecycle_;
        InitializedCapabilities initialized_;
        std::unique_ptr<Detail::ShutdownController> shutdown_;
        ServiceAttachment maintenance_binding_;

    public:
        CE_API GameRuntime(
            ::CE::Runtime::Context& engine,
            ::CE::Game::AbstractGame& game,
            ::CE::Runtime::RunMode mode = ::CE::Runtime::RunMode::Sequential,
            Input::PollingOptions polling = Input::PollingOptions{},
            ::CE::Runtime::Simulation::TimingOptions timing = ::CE::Runtime::Simulation::TimingOptions{}
        );
        CE_API ~GameRuntime();

        // One blocking session on the platform owner. Borrowed engine/game must remain
        // alive through return/throw; cleanup preserves the first failure. Do not destroy
        // the runtime while run() or another operation is using it.
        CE_API void run();
        // Thread-safe request; run() performs normal teardown after simulation stops.
        CE_API void stop();
        // Caller synchronizes with run start/return. Query before run or after
        // its return/throw; live sampling rejects. Counters publish after join.
        [[nodiscard]] CE_API ::CE::Runtime::RuntimeStats diagnostics() const;
        [[nodiscard]] RuntimeLifecycle lifecycle() const noexcept { return lifecycle_; }
        [[nodiscard]] CE_API ShutdownEndpoint shutdown() const;
        /** Install one host authority before run. Negotiate requires a consumable
         * native close; Force explicitly keeps native close unconditional.
         */
        [[nodiscard]] CE_API ShutdownPolicy set_shutdown_policy(
            ShutdownPolicyCallback, ShutdownPolicyOptions = {}, CE::Events::EventBus::Delivery = {});
        [[nodiscard]] ::CE::Runtime::SimulationDispatcher& simulation_dispatcher() { return simulation_dispatcher_; }

    private:
        CE_API void bind_renderer_maintenance();
        CE_API void publish_step(const Simulation::Step&, double dropped_seconds);
        CE_API void process_shutdown();
        CE_API void publish_ready();
        CE_API void publish_shutdown_starting(std::exception_ptr failure) noexcept;
        CE_API void publish_quiescing() noexcept;
        CE_API void finish_lifecycle(std::exception_ptr failure) noexcept;
        CE_API WindowState select_window();
        CE_API void select_simulation_view(const WindowState& selected);
        [[nodiscard]] CE_API bool window_selection_current(const WindowState& selected) const;
        friend struct ::CE::Runtime::Detail::GameRuntimeAccess;
        CE_API void run_sequential();
        CE_API void run_concurrent();
        CE_API void finish_unstarted_session();
        CE_API void pump_shutdown_requests(std::exception_ptr& failure);
        CE_API void finish_worker_shutdown(std::exception_ptr& failure);
        CE_API void preserve_failure(std::exception_ptr& first, const char* phase, std::exception_ptr next);
        CE_API void report_session(bool failed) const noexcept;
        CE_API void observe_input(const std::shared_ptr<const Input::PollSnapshot>& snapshot) noexcept;
        CE_API void observe_timing(const ::CE::Runtime::Simulation::Batch& batch) noexcept;
        CE_API void observe_platform(bool pressure) noexcept;
        CE_API void publish_runtime_pressure() noexcept;
        [[nodiscard]] CE_API ::CE::Runtime::Detail::RuntimePressureWatch
        watch_runtime_pressure(std::function<void(const ::CE::Runtime::Detail::RuntimePressureChanged&)> callback);
    };
}

/**
 * @fn CE::Runtime::GameRuntime::~GameRuntime()
 * @brief Close internal observation state without invoking listeners.
 * The caller must first stop and join an active run; destruction performs no runtime teardown.
 */

/**
 * @fn CE::Runtime::GameRuntime::GameRuntime(Context& engine, CE::Game::AbstractGame& game, RunMode mode, Input::PollingOptions polling, Simulation::TimingOptions timing)
 * @brief Retain a context/game and validate execution, polling and timing policy.
 * @param engine Borrowed compatible context, alive through runtime use and cleanup.
 * @param game Borrowed game hooks, alive through runtime use and cleanup.
 * @param mode Sequential caller-owned or concurrent simulation-worker execution.
 * @param polling Input observation/backlog policy, independent of simulation delta.
 * @param timing Variable/fixed and bounded recovery policy.
 * @throws CE::Exceptions::invalid_args For invalid mode, polling or timing settings.
 * Construction starts no session; run() reserves the context's single session.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Runtime::GameRuntime::run()
 * @brief Execute one blocking session on the platform/graphics owner.
 * Initializes adapters/game, pumps input/dispatch/rendering, then quiesces, settles work and tears down in dependency order.
 * Context/game must remain alive through return or throw. Reuse or another runtime session on the same context rejects. Hook/native failures trigger cleanup and preserve the first exception. Do not destroy the runtime while this or another operation uses it.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Runtime::GameRuntime::stop()
 * @brief Request normal shutdown from any producer thread while this runtime lives.
 * Does not join, interrupt a hook or synchronously release resources. run() observes the shared request and completes normal teardown.
 * Bypasses ordinary shutdown negotiation and supersedes an unresolved request. Use shutdown().request()
 * when the host policy must be able to defer/cancel; repeated unconditional stop is safe.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/** @fn CE::Runtime::RuntimeLifecycle CE::Runtime::GameRuntime::lifecycle() const noexcept
 * Retain synchronized current/terminal observations without borrowing Runtime. snapshot() is allowed
 * during the live session, unlike diagnostics(); terminal state owns final counters and bounded failure
 * summaries. Register on Context::events() before run for milestone occurrences; there is no bus replay.
 * @see [Lifecycle observation](cheryl-guide:runtime/runtime-frame-boundary#lifecycle-observation)
 */
/** @fn CE::Runtime::ShutdownEndpoint CE::Runtime::GameRuntime::shutdown() const
 * Retain ordinary command/snapshot access without a raw Runtime pointer. request() stages safe platform
 * work, coalesces unresolved duplicates and returns a shared result ticket. Runtime closure rejects new
 * requests. Never block the platform/simulation owner needed by the policy on a ticket's completion.
 */
/** @fn CE::Runtime::ShutdownPolicy CE::Runtime::GameRuntime::set_shutdown_policy(ShutdownPolicyCallback callback, ShutdownPolicyOptions options, CE::Events::EventBus::Delivery delivery)
 * Install the single host decision authority before run and retain its move-only handle. Immediate
 * callbacks run on platform; optional queued delivery selects another owner and must not run inline.
 * The callback can retain its private ShutdownDecision while a prompt/save completes. Policy/delivery
 * failure or withdrawal cancels an unresolved ordinary request; forced stop/failure supersedes it.
 * Default Negotiate requires consumable native close, while Force keeps native close unconditional.
 * @see [Shutdown protocol](cheryl-guide:runtime/runtime-frame-boundary#negotiable-shutdown)
 */

/**
 * @fn CE::Runtime::RuntimeStats CE::Runtime::GameRuntime::diagnostics() const
 * @brief Copy counters before a session or after run() returns/throws.
 * @return Owned observations published after simulation join.
 * @throws CE::Exceptions::failed_operation During an active session.
 * Caller synchronizes with run start/return; the rejection check does not authorize racing a new run or live object destruction.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Runtime::SimulationDispatcher& CE::Runtime::GameRuntime::simulation_dispatcher()
 * @brief Borrow the simulation request queue without opening it.
 * @return Runtime-owned dispatcher; retain its submission() endpoint for independent producer lifetime.
 * Callbacks run at actual update boundaries. Request acceptance requires the runtime's active queue and is separate from execution.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */
