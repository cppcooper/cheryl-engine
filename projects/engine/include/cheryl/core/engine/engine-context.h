#pragma once

#include "platform-dispatcher.h"
#include "worker-pool.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace CE::GFramework {
    class GameRuntime;
}

namespace CE {
    class iDisplaySystem;
    class iWindow;
}

namespace CE::Assets {
    struct ResourceProvider;
}

namespace CE::Input {
    class iInputSystem;
}

namespace CE::RenderAPIs {
    class iPresentationSurface;
    class iRenderer;
}

namespace CE::Engine {
    namespace ContextDetail {
        struct EngineContextAccess;
    }

    struct ExecutionOptions {
        std::size_t worker_count = 1;            // Owned root is created lazily.
        std::shared_ptr<WorkerPool> shared_pool; // Injected root is never closed by this context.
    };
    /** Owns a compatible set of platform, presentation, rendering, and resource adapters.
     * Input can be owned or explicitly borrowed. Owned input is destroyed before
     * resources, renderer, surface, and display, so callbacks detach from a live window.
     * GameRuntime coordinates their initialization and shutdown; this is not a game loop.
     * One context supports one runtime session; stopped adapters are not restarted.
     * Null adapters/owned input or a zero owned worker count throw on construction.
     * Borrowed input must survive runtime cleanup/context lifetime and detach from
     * the display's live window before its destruction. Adapter use retains its affinity.
     */
    class EngineContext final {
        std::unique_ptr<iDisplaySystem> display_;
        std::unique_ptr<RenderAPIs::iPresentationSurface> surface_;
        std::unique_ptr<RenderAPIs::iRenderer> renderer_;
        std::unique_ptr<Assets::ResourceProvider> resources_;
        std::unique_ptr<Input::iInputSystem> owned_input_;
        Input::iInputSystem* input_;
        std::atomic<bool> session_started_{false};
        PlatformDispatcher platform_dispatcher_;
        ExecutionOptions execution_;
        mutable std::mutex execution_mutex_;
        std::function<std::unique_ptr<WorkerPool>(std::size_t)> owned_worker_factory_;
        std::unique_ptr<WorkerPool> owned_workers_;
        std::vector<WorkerGroup> worker_groups_;
        bool worker_submissions_closed_ = false;
        const Diagnostics::DomainId domain_ = Diagnostics::next_domain_id();
        bool destroying_ = false;

    public:
        EngineContext(
            std::unique_ptr<iDisplaySystem> display,
            std::unique_ptr<RenderAPIs::iPresentationSurface> surface,
            std::unique_ptr<RenderAPIs::iRenderer> renderer,
            std::unique_ptr<Assets::ResourceProvider> resources,
            Input::iInputSystem& input,
            ExecutionOptions execution = ExecutionOptions{}
        );
        EngineContext(
            std::unique_ptr<iDisplaySystem> display,
            std::unique_ptr<RenderAPIs::iPresentationSurface> surface,
            std::unique_ptr<RenderAPIs::iRenderer> renderer,
            std::unique_ptr<Assets::ResourceProvider> resources,
            std::unique_ptr<Input::iInputSystem> input,
            ExecutionOptions execution = ExecutionOptions{}
        );
        ~EngineContext();

        EngineContext(const EngineContext&) = delete;
        EngineContext& operator=(const EngineContext&) = delete;

        // Borrowed adapters, not ownership transfer; getters do not marshal work.
        // window() requires the display owner and throws if there is no active window.
        [[nodiscard]] iDisplaySystem& display() const;
        [[nodiscard]] iWindow& window() const;
        [[nodiscard]] RenderAPIs::iPresentationSurface& surface() const;
        [[nodiscard]] RenderAPIs::iRenderer& renderer() const;
        [[nodiscard]] Assets::ResourceProvider& resources() const;
        [[nodiscard]] Input::iInputSystem& input() const;
        [[nodiscard]] PlatformDispatcher& platform_dispatcher() { return platform_dispatcher_; }
        [[nodiscard]] Diagnostics::DomainId diagnostic_id() const noexcept { return domain_; }
        // Groups created here are part of this context's shutdown domain, even
        // when their physical capacity comes from an application-supplied pool.
        // Synchronized across producers; closed context/pool or invalid options throw.
        [[nodiscard]] WorkerGroup make_worker_group(WorkerGroupOptions options = WorkerGroupOptions{});

    private:
        friend class GFramework::GameRuntime;
        friend struct ContextDetail::EngineContextAccess;
        void validate() const;
        void begin_session();
        void close_worker_submissions();
        [[nodiscard]] bool workers_idle() const;
        void finish_workers();
    };
}
