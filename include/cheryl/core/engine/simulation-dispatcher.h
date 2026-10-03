#pragma once

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace CE::GFramework {
    class GameRuntime;
}

namespace CE::Engine {
    /** Transfers owned requests to the runtime's simulation thread.
     * Only the simulation owner may access its mutable game state. Queue owned requests;
     * borrowed captures must outlive completion or owner-thread cancellation.
     * Never wait on a simulation future from the simulation owner. Delivery precedes
     * input transfer at each actual update boundary in either runtime mode.
     * Shutdown destroys pending captures on the owner before resource cleanup; futures
     * report broken_promise. Submission handles remain safe after dispatcher destruction.
     */
    class SimulationDispatcher final {
        friend class GFramework::GameRuntime;
        using Task = std::packaged_task<void()>;
        struct State {
            mutable std::mutex mutex;
            std::vector<Task> pending;
            std::function<void()> wake;
            std::thread::id owner;
            bool accepting = false;
            bool opened = false;
            bool draining = false;
        };
        std::shared_ptr<State> state_ = std::make_shared<State>();

    public:
        /** Copyable submission endpoint, with no borrowed dispatcher or game pointer.
         * Queueing always defers execution, including submissions from the owner thread.
         * Rejection throws failed_operation; accepted callback failures reach their future.
         */
        class Submission final {
            friend class SimulationDispatcher;
            std::shared_ptr<State> state_;

            explicit Submission(std::shared_ptr<State> state)
            : state_(std::move(state)) {}

        public:
            template <typename Work>
            [[nodiscard]] auto submit(Work&& work) const -> std::future<std::invoke_result_t<std::decay_t<Work>&>> {
                using Result = std::invoke_result_t<std::decay_t<Work>&>;
                std::promise<Result> completion;
                auto result = completion.get_future();
                // Keep callable ownership separate from the future's shared state.
                // Cancellation releases captures even when the future is retained.
                SimulationDispatcher::enqueue(state_, Task([work = std::forward<Work>(work), completion = std::move(completion)]() mutable {
                    try {
                        if constexpr (std::is_void_v<Result>) {
                            std::invoke(work);
                            completion.set_value();
                        } else {
                            completion.set_value(std::invoke(work));
                        }
                    } catch (...) {
                        completion.set_exception(std::current_exception());
                    }
                }));
                return result;
            }
        };

        SimulationDispatcher() = default;
        ~SimulationDispatcher();
        SimulationDispatcher(const SimulationDispatcher&) = delete;
        SimulationDispatcher& operator=(const SimulationDispatcher&) = delete;

        [[nodiscard]] Submission submission() const { return Submission(state_); }
        template <typename Work> [[nodiscard]] auto submit(Work&& work) -> std::future<std::invoke_result_t<std::decay_t<Work>&>> {
            return submission().submit(std::forward<Work>(work));
        }
        [[nodiscard]] bool has_pending() const;

    private:
        static void enqueue(const std::shared_ptr<State>& state, Task request);
        static void require_owner(const State& state);
        void open(std::function<void()> wake);
        void bind_owner();
        void drain();
        void close();
        void invalidate();
    };
}
