#pragma once

#include <core/diagnostics.h>
#include <atomic>

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
    class EngineContext;

    /** Transfers owned requests to the runtime's platform/graphics thread.
     * Submit values or move-owned CPU data; a callback must not access live simulation objects.
     * Check future readiness during update instead of blocking that update on the platform.
     * Shutdown destroys pending captures on the owner before resource cleanup; futures
     * report broken_promise. Submission handles remain safe after dispatcher destruction.
     */
    class PlatformDispatcher final {
        friend class GFramework::GameRuntime;
        using Task = std::packaged_task<void(EngineContext&)>;
        struct State {
            mutable std::mutex mutex;
            std::vector<Task> pending;
            std::function<void()> wake;
            std::thread::id owner;
            bool accepting = false;
            bool opened = false;
            bool draining = false;
            Diagnostics::DispatchStats diagnostics{Diagnostics::next_domain_id()};
            std::atomic<std::uint64_t> failures{0};
        };
        std::shared_ptr<State> state_ = std::make_shared<State>();

    public:
        /** Copyable submission endpoint, with no borrowed dispatcher or engine pointer.
         * Queueing always defers execution, including submissions from the owner thread.
         * Rejection throws failed_operation; accepted callback failures reach their future.
         */
        class Submission final {
            friend class PlatformDispatcher;
            std::shared_ptr<State> state_;

            explicit Submission(std::shared_ptr<State> state)
            : state_(std::move(state)) {}

        public:
            template <typename Work>
            [[nodiscard]] auto submit(Work&& work) const -> std::future<std::invoke_result_t<std::decay_t<Work>&, EngineContext&>> {
                using Result = std::invoke_result_t<std::decay_t<Work>&, EngineContext&>;
                std::promise<Result> completion;
                auto result = completion.get_future();
                // Keep callable ownership separate from the future's shared state.
                // Cancellation releases captures even when the future is retained.
                PlatformDispatcher::enqueue(
                    state_, Task([work = std::forward<Work>(work), completion = std::move(completion), tracked = std::weak_ptr<State>(state_)](EngineContext& engine) mutable {
                        try {
                            if constexpr (std::is_void_v<Result>) {
                                std::invoke(work, engine);
                                completion.set_value();
                            } else {
                                completion.set_value(std::invoke(work, engine));
                            }
                        } catch (...) {
                            if (const auto state = tracked.lock())
                                state->failures.fetch_add(1, std::memory_order_relaxed);
                            completion.set_exception(std::current_exception());
                        }
                    })
                );
                return result;
            }
        };

        PlatformDispatcher() = default;
        ~PlatformDispatcher();
        PlatformDispatcher(const PlatformDispatcher&) = delete;
        PlatformDispatcher& operator=(const PlatformDispatcher&) = delete;

        [[nodiscard]] Submission submission() const { return Submission(state_); }
        template <typename Work>
        [[nodiscard]] auto submit(Work&& work) -> std::future<std::invoke_result_t<std::decay_t<Work>&, EngineContext&>> {
            return submission().submit(std::forward<Work>(work));
        }
        [[nodiscard]] bool has_pending() const;
        // Queue snapshot only. Callback exceptions remain owned by their futures.
        [[nodiscard]] Diagnostics::DispatchStats diagnostics() const;

    private:
        static void enqueue(const std::shared_ptr<State>& state, Task request);
        static void require_owner(const State& state);
        void open(std::function<void()> wake);
        void drain(EngineContext& engine);
        void close();
        void invalidate();
    };
}
