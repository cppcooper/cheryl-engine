#pragma once

#include <core/controls/polling-backlog.h>
#include <core/engine/simulation-dispatcher.h>
#include <core/diagnostics.h>

#include "simulation-scheduler.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace CE::Engine {
    class EngineContext;
}

namespace CE::GFramework {
    namespace RuntimeDetail {
        struct GameRuntimeAccess;
    }

    struct AbstractGame;

    enum class RunMode { Sequential, Concurrent };

    struct RuntimeStats {
        Diagnostics::DomainId domain = 0;
        std::uint64_t updates = 0;
        std::uint64_t polls = 0;
        std::uint64_t published = 0;
        std::uint64_t rendered = 0;
        std::uint64_t superseded = 0;
        std::uint64_t skipped_publication = 0;
        std::uint64_t dropped_batches = 0;
        std::uint64_t dropped_nanoseconds = 0;
        std::uint64_t peak_polls = 0;
        std::uint64_t resizes = 0;
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
        Engine::EngineContext& engine_;
        AbstractGame& game_;
        RunMode mode_;
        Input::PollingOptions polling_;
        SimulationTimingOptions timing_;
        RuntimeStats diagnostics_{Diagnostics::next_domain_id()};
        const char* phase_ = "reserve_session"; // Platform owner; retained first failing phase.
        std::atomic<bool> run_finished_{false};
        bool renderer_ready_ = false; // Platform-owned; partial initialization is not maintenance-ready.
        std::atomic<bool> run_started_{false};
        std::atomic<bool> stop_requested_{false};
        struct Scheduler {
            std::mutex mutex;
            std::condition_variable wake;
        };
        std::shared_ptr<Scheduler> scheduler_ = std::make_shared<Scheduler>();
        Engine::SimulationDispatcher simulation_dispatcher_;
        std::function<std::thread(std::function<void()>)> simulation_thread_factory_;

    public:
        GameRuntime(
            Engine::EngineContext& engine,
            AbstractGame& game,
            RunMode mode = RunMode::Sequential,
            Input::PollingOptions polling = Input::PollingOptions{},
            SimulationTimingOptions timing = SimulationTimingOptions{}
        );

        void run();
        void stop();
        // Caller synchronizes with run start/return. Query before run or after
        // its return/throw; live sampling rejects. Counters publish after join.
        [[nodiscard]] RuntimeStats diagnostics() const;
        [[nodiscard]] Engine::SimulationDispatcher& simulation_dispatcher() { return simulation_dispatcher_; }

    private:
        friend struct RuntimeDetail::GameRuntimeAccess;
        void run_sequential();
        void run_concurrent();
        void finish_unstarted_session();
        void pump_shutdown_requests(std::exception_ptr& failure);
        void finish_worker_shutdown(std::exception_ptr& failure);
        void preserve_failure(std::exception_ptr& first, const char* phase, std::exception_ptr next);
        void report_session(bool failed) const noexcept;
    };
}
