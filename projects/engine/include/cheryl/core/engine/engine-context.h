#pragma once

#include <cheryl/export.h>

#include "platform-dispatcher.h"
#include "worker-pool.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace CE::Runtime {
    class GameRuntime;
}

namespace CE::Platform {
    class DisplaySystem;
    class Window;
}

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Input {
    class System;
}

namespace CE::Rendering {
    class PresentationSurface;
    class Renderer;
}

namespace CE::Runtime {
    namespace Detail {
        struct ContextAccess;
    }

    struct ExecutionOptions {
        std::size_t worker_count = 1;                   // Owned root is created lazily.
        std::shared_ptr<CE::Workers::Pool> shared_pool; // Injected root is never closed by this context.
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
    class Context final {
        std::unique_ptr<CE::Platform::DisplaySystem> display_;
        std::unique_ptr<CE::Rendering::PresentationSurface> surface_;
        std::unique_ptr<CE::Rendering::Renderer> renderer_;
        std::unique_ptr<CE::Rendering::Resources::Provider> resources_;
        std::unique_ptr<CE::Input::System> owned_input_;
        CE::Input::System* input_;
        std::atomic<bool> session_started_{false};
        PlatformDispatcher platform_dispatcher_;
        ExecutionOptions execution_;
        mutable std::mutex execution_mutex_;
        std::function<std::unique_ptr<CE::Workers::Pool>(std::size_t)> owned_worker_factory_;
        std::unique_ptr<CE::Workers::Pool> owned_workers_;
        std::vector<CE::Workers::Group> worker_groups_;
        bool worker_submissions_closed_ = false;
        const CE::Diagnostics::DomainId domain_ = CE::Diagnostics::next_domain_id();
        bool destroying_ = false;

    public:
        CE_API Context(
            std::unique_ptr<CE::Platform::DisplaySystem> display,
            std::unique_ptr<CE::Rendering::PresentationSurface> surface,
            std::unique_ptr<CE::Rendering::Renderer> renderer,
            std::unique_ptr<CE::Rendering::Resources::Provider> resources,
            CE::Input::System& input,
            ExecutionOptions execution = ExecutionOptions{}
        );
        CE_API Context(
            std::unique_ptr<CE::Platform::DisplaySystem> display,
            std::unique_ptr<CE::Rendering::PresentationSurface> surface,
            std::unique_ptr<CE::Rendering::Renderer> renderer,
            std::unique_ptr<CE::Rendering::Resources::Provider> resources,
            std::unique_ptr<CE::Input::System> input,
            ExecutionOptions execution = ExecutionOptions{}
        );
        CE_API ~Context();

        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;

        // Borrowed adapters, not ownership transfer; getters do not marshal work.
        // window() requires the display owner and throws if there is no active window.
        [[nodiscard]] CE_API CE::Platform::DisplaySystem& display() const;
        [[nodiscard]] CE_API CE::Platform::Window& window() const;
        [[nodiscard]] CE_API CE::Rendering::PresentationSurface& surface() const;
        [[nodiscard]] CE_API CE::Rendering::Renderer& renderer() const;
        [[nodiscard]] CE_API CE::Rendering::Resources::Provider& resources() const;
        [[nodiscard]] CE_API CE::Input::System& input() const;
        [[nodiscard]] PlatformDispatcher& platform_dispatcher() { return platform_dispatcher_; }
        [[nodiscard]] CE::Diagnostics::DomainId diagnostic_id() const noexcept { return domain_; }
        // Groups created here are part of this context's shutdown domain, even
        // when their physical capacity comes from an application-supplied pool.
        // Synchronized across producers; closed context/pool or invalid options throw.
        [[nodiscard]] CE_API CE::Workers::Group make_worker_group(CE::Workers::GroupOptions options = CE::Workers::GroupOptions{});

    private:
        friend class GameRuntime;
        friend struct Detail::ContextAccess;
        CE_API void validate() const;
        CE_API void begin_session();
        CE_API void close_worker_submissions();
        [[nodiscard]] CE_API bool workers_idle() const;
        CE_API void finish_workers();
    };
}
