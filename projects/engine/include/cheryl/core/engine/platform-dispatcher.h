#pragma once

#include <cheryl/export.h>

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

namespace CE::Runtime {
    class GameRuntime;
}

namespace CE::Runtime {
    class Context;

    /** Transfers owned requests to the runtime's platform/graphics thread.
     * Submit values or move-owned CPU data; a callback must not access live simulation objects.
     * Check future readiness during update instead of blocking that update on the platform.
     * Shutdown destroys pending captures on the owner before resource cleanup; futures
     * report broken_promise. Submission handles remain safe after dispatcher destruction.
     */
    class PlatformDispatcher final {
        friend class GameRuntime;
        using Task = std::packaged_task<void(Context&)>;
        struct State {
            mutable std::mutex mutex;
            std::vector<Task> pending;
            std::function<void()> wake;
            std::thread::id owner;
            bool accepting = false;
            bool opened = false;
            bool draining = false;
            CE::Diagnostics::DispatchStats diagnostics{CE::Diagnostics::next_domain_id()};
            std::atomic<std::uint64_t> failures{0};
        };
        std::shared_ptr<State> state_ = std::make_shared<State>();

    public:
        /** Copyable submission endpoint, with no borrowed dispatcher or engine pointer.
         * Queueing always defers execution, including submissions from the owner thread.
         * Rejection throws failed_operation; accepted callback failures reach their future.
         * Submission is thread-safe while the endpoint remains alive. Own callable
         * captures through execution/cancellation; reference captures/results remain
         * the caller's lifetime responsibility. Do not wait from the execution owner.
         */
        class Submission final {
            friend class PlatformDispatcher;
            std::shared_ptr<State> state_;

            explicit Submission(std::shared_ptr<State> state)
            : state_(std::move(state)) {}

        public:
            template <typename Work>
            [[nodiscard]] auto submit(Work&& work) const -> std::future<std::invoke_result_t<std::decay_t<Work>&, Context&>> {
                using Result = std::invoke_result_t<std::decay_t<Work>&, Context&>;
                std::promise<Result> completion;
                auto result = completion.get_future();
                // Keep callable ownership separate from the future's shared state.
                // Cancellation releases captures even when the future is retained.
                PlatformDispatcher::enqueue(
                    state_, Task([work = std::forward<Work>(work), completion = std::move(completion),
                                  tracked = std::weak_ptr<State>(state_)](Context& engine) mutable {
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
        CE_API ~PlatformDispatcher();
        PlatformDispatcher(const PlatformDispatcher&) = delete;
        PlatformDispatcher& operator=(const PlatformDispatcher&) = delete;

        [[nodiscard]] Submission submission() const { return Submission(state_); }
        template <typename Work>
        [[nodiscard]] auto submit(Work&& work) -> std::future<std::invoke_result_t<std::decay_t<Work>&, Context&>> {
            return submission().submit(std::forward<Work>(work));
        }
        // Synchronized queued-work snapshot; false does not establish completion/closure.
        [[nodiscard]] CE_API bool has_pending() const;
        // Queue snapshot only. Callback exceptions remain owned by their futures.
        [[nodiscard]] CE_API CE::Diagnostics::DispatchStats diagnostics() const;

    private:
        CE_API static void enqueue(const std::shared_ptr<State>& state, Task request);
        CE_API static void require_owner(const State& state);
        CE_API void open(std::function<void()> wake);
        CE_API void drain(Context& engine);
        CE_API void close();
        CE_API void invalidate();
    };
}

/**
 * @fn CE::Runtime::PlatformDispatcher::PlatformDispatcher()
 * @brief Create a closed request queue. GameRuntime opens and binds its single execution session.
 * @see [Detailed guide](cheryl-guide:runtime/thread-dispatch)
 */

/**
 * @fn CE::Runtime::PlatformDispatcher::~PlatformDispatcher()
 * @brief Invalidate submission state and cancel unexecuted requests.
 * Runtime normally performs owner-thread cancellation before teardown; do not destroy while operations use this object. Retained Submission values remain safe and reject later work.
 * @see [Detailed guide](cheryl-guide:runtime/thread-dispatch)
 */

/**
 * @fn CE::Runtime::PlatformDispatcher::PlatformDispatcher(const PlatformDispatcher&)
 * @brief Copy construction is disabled; retain a copyable Submission endpoint instead.
 * @see [Detailed guide](cheryl-guide:runtime/thread-dispatch)
 */

/**
 * @fn PlatformDispatcher& CE::Runtime::PlatformDispatcher::operator=(const PlatformDispatcher&)
 * @brief Copy assignment is disabled; queue/session ownership cannot be replaced.
 * @see [Detailed guide](cheryl-guide:runtime/thread-dispatch)
 */

/**
 * @fn CE::Runtime::PlatformDispatcher::Submission CE::Runtime::PlatformDispatcher::submission() const
 * @brief Retain a copyable producer endpoint independent of dispatcher lifetime.
 * @return Endpoint owning shared queue state, without a borrowed game/context pointer.
 * Obtaining it does not open the queue; concurrent submission requires a live endpoint.
 * @see [Detailed guide](cheryl-guide:runtime/thread-dispatch)
 */

/**
 * @fn bool CE::Runtime::PlatformDispatcher::has_pending() const
 * @brief Observe whether requests remain in the shared queue.
 * @return Synchronized queue snapshot; false does not establish closure or completion of an already detached batch.
 * @see [Detailed guide](cheryl-guide:runtime/thread-dispatch)
 */

/**
 * @fn CE::Diagnostics::DispatchStats CE::Runtime::PlatformDispatcher::diagnostics() const
 * @brief Copy synchronized queue/counter observations.
 * @return Best-effort stats, including detached-batch running counts. Callback exceptions remain owned by their futures; this is not a completion barrier.
 * @see [Detailed guide](cheryl-guide:runtime/thread-dispatch)
 */

/**
 * @fn template<typename Work> auto CE::Runtime::PlatformDispatcher::submit(Work&& work) -> std::future<std::invoke_result_t<std::decay_t<Work>&, Context&>>
 * @brief Queue an owned callable for deferred platform-owner execution.
 * @tparam Work Callable invocable with Runtime::Context&; move-only captures and void/value/reference results are supported.
 * @param work Callable forwarded into owned queue storage; borrowed captures/results remain the caller's lifetime responsibility.
 * @return Future holding the invocation result or its exception; cancellation before entry produces broken_promise.
 * @throws CE::Exceptions::failed_operation If no active accepting runtime queue exists. Allocation/callable construction failures can propagate before acceptance.
 * Always queues, even from the owner thread. Do not wait on this future from the execution owner. Capture immutable values/owned CPU data; do not access live simulation objects from this callback.
 * @see [Detailed guide](cheryl-guide:runtime/thread-dispatch)
 */

/**
 * @fn template<typename Work> auto CE::Runtime::PlatformDispatcher::Submission::submit(Work&& work) const -> std::future<std::invoke_result_t<std::decay_t<Work>&, Context&>>
 * @brief Queue an owned callable for deferred platform-owner execution.
 * @tparam Work Callable invocable with Runtime::Context&; move-only captures and void/value/reference results are supported.
 * @param work Callable forwarded into owned queue storage; borrowed captures/results remain the caller's lifetime responsibility.
 * @return Future holding the invocation result or its exception; cancellation before entry produces broken_promise.
 * @throws CE::Exceptions::failed_operation If no active accepting runtime queue exists. Allocation/callable construction failures can propagate before acceptance.
 * Always queues, even from the owner thread. Do not wait on this future from the execution owner. Capture immutable values/owned CPU data; do not access live simulation objects from this callback.
 * @see [Detailed guide](cheryl-guide:runtime/thread-dispatch)
 */
