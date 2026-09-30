#pragma once

#include <functional>
#include <future>
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
     * Callback failures reach their own futures. Shutdown cancels unexecuted requests
     * on the platform while resources still exist; their futures report broken_promise.
     */
    class PlatformTaskQueue final {
    public:
        template <typename Work>
        [[nodiscard]] auto submit(Work&& work) -> std::future<std::invoke_result_t<std::decay_t<Work>&, EngineContext&>> {
            using Result = std::invoke_result_t<std::decay_t<Work>&, EngineContext&>;
            std::promise<Result> completion;
            auto result = completion.get_future();
            // Keep the callable in the queue, separate from the result's shared state.
            // A cancelled future can outlive the queue without retaining CPU captures.
            enqueue(Task([work = std::forward<Work>(work), completion = std::move(completion)](EngineContext& engine) mutable {
                try {
                    if constexpr (std::is_void_v<Result>) {
                        std::invoke(work, engine);
                        completion.set_value();
                    }
                    else {
                        completion.set_value(std::invoke(work, engine));
                    }
                }
                catch (...) {
                    completion.set_exception(std::current_exception());
                }
            }));
            return result;
        }
        [[nodiscard]] bool has_pending() const;

    private:
        friend class GFramework::GameRuntime;
        using Task = std::packaged_task<void(EngineContext&)>;
        void enqueue(Task request);
        void open(std::function<void()> wake);
        void drain(EngineContext& engine);
        void close();
        void require_owner() const;

        mutable std::mutex mutex_;
        std::vector<Task> pending_;
        std::function<void()> wake_;
        std::thread::id owner_;
        bool accepting_ = false;
    };
}
