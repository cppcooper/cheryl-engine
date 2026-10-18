#pragma once

#include <cheryl/export.h>

#include "platform-dispatcher.h"
#include "worker-pool.h"
#include <cheryl/text/font-service.h>

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

    /** Execution capacity for Context-owned CPU work; settings alone start no threads. */
    struct ExecutionOptions {
        std::size_t worker_count = 1; ///< Positive capacity for a lazily created owned pool; ignored when shared_pool is supplied.
        std::shared_ptr<CE::Workers::Pool> shared_pool; ///< Optional retained physical pool; Context closes/drains only its own tracked groups, never the shared pool.
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
        mutable CE::Text::FontService fonts_;

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
        [[nodiscard]] CE::Text::FontService& fonts() const noexcept { return fonts_; }
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

/**
 * @fn CE::Runtime::Context::Context(std::unique_ptr<CE::Platform::DisplaySystem> display, std::unique_ptr<CE::Rendering::PresentationSurface> surface, std::unique_ptr<CE::Rendering::Renderer> renderer, std::unique_ptr<CE::Rendering::Resources::Provider> resources, CE::Input::System& input, ExecutionOptions execution)
 * @brief Assemble owned compatible adapters and borrowed input without starting a runtime.
 * @param display Display/window owner transferred to this context.
 * @param surface Compatible presentation owner transferred to this context.
 * @param renderer Compatible graphics renderer transferred to this context.
 * @param resources Resource provider in the same backend domain.
 * @param input Borrowed input that must remain alive through runtime cleanup and context destruction, detached while its window is still alive.
 * @param execution Positive owned capacity, or an application-supplied shared pool.
 * @throws CE::Exceptions::invalid_args For a missing required adapter/input or zero owned worker count.
 * Adapter compatibility and construction/current-context requirements belong to the selected backend. No automatic dispatch or native reinitialization is provided.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Runtime::Context::Context(std::unique_ptr<CE::Platform::DisplaySystem> display, std::unique_ptr<CE::Rendering::PresentationSurface> surface, std::unique_ptr<CE::Rendering::Renderer> renderer, std::unique_ptr<CE::Rendering::Resources::Provider> resources, std::unique_ptr<CE::Input::System> input, ExecutionOptions execution)
 * @brief Assemble owned compatible adapters and owned input without starting a runtime.
 * @param display Display/window owner transferred to this context.
 * @param surface Compatible presentation owner transferred to this context.
 * @param renderer Compatible graphics renderer transferred to this context.
 * @param resources Resource provider in the same backend domain.
 * @param input Non-null input owner transferred to this context; destroyed before the display.
 * @param execution Positive owned capacity, or an application-supplied shared pool.
 * @throws CE::Exceptions::invalid_args For a missing required adapter/input or zero owned worker count.
 * Adapter compatibility and construction/current-context requirements belong to the selected backend. No automatic dispatch or native reinitialization is provided.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Runtime::Context::~Context()
 * @brief Close and settle context-tracked CPU work before destroying adapters.
 * The caller must first stop/join the runtime and external producers. This blocks for accepted work; do not destroy a context on one of its workers or with work awaiting unpumped platform requests. Failure to settle safely is terminal. An injected pool and its unrelated groups are not shut down.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Runtime::Context::Context(const Context&)
 * @brief Copy construction is disabled; one context owns one adapter/session domain.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn Context& CE::Runtime::Context::operator=(const Context&)
 * @brief Copy assignment is disabled; adapter ownership is not transferable through assignment.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Text::FontService& CE::Runtime::Context::fonts() const noexcept
 * @brief Borrow this context's CPU font catalogue.
 * @return Mutable service retained until context destruction, even through a const Context.
 * Configure at the service's documented startup boundary; returned immutable Font handles have independent retained CPU lifetime.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Runtime::PlatformDispatcher& CE::Runtime::Context::platform_dispatcher()
 * @brief Borrow the context's platform request queue; obtaining it does not open a runtime session.
 * @return Dispatcher valid through context lifetime; retain submission() for independently owned producer endpoints.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Diagnostics::DomainId CE::Runtime::Context::diagnostic_id() const noexcept
 * @brief Read this context's process-local diagnostic identity; not a persistent or native-resource key.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Workers::Group CE::Runtime::Context::make_worker_group(CE::Workers::GroupOptions options)
 * @brief Create a workload in this context's tracked shutdown domain.
 * @param options Structural and CPU/locality policy for the new group.
 * @return Copyable group sharing either the lazily created owned pool or execution.shared_pool.
 * Synchronized across producers while the context lives. Closed context/pool, invalid options or unavailable required policy reject. Shutdown closes/drains every group created here, including copies held by callers; unrelated shared-pool groups remain independent.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Platform::DisplaySystem& CE::Runtime::Context::display() const
 * @brief Borrow the selected display adapter.
 * @return Context-associated reference, with no ownership transfer or thread marshalling.
 * Platform-owner calls remain on their required owner. Do not use beyond its owner lifetime.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Platform::Window& CE::Runtime::Context::window() const
 * @brief Borrow the selected window adapter.
 * @return Context-associated reference, with no ownership transfer or thread marshalling.
 * Requires the display owner; throws failed_operation if no window is active. Do not use beyond its owner lifetime.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Rendering::PresentationSurface& CE::Runtime::Context::surface() const
 * @brief Borrow the selected surface adapter.
 * @return Context-associated reference, with no ownership transfer or thread marshalling.
 * Presentation operations retain their graphics/platform affinity. Do not use beyond its owner lifetime.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Rendering::Renderer& CE::Runtime::Context::renderer() const
 * @brief Borrow the selected renderer adapter.
 * @return Context-associated reference, with no ownership transfer or thread marshalling.
 * Graphics operations retain their owner/current-context requirements. Do not use beyond its owner lifetime.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Rendering::Resources::Provider& CE::Runtime::Context::resources() const
 * @brief Borrow the selected resources adapter.
 * @return Context-associated reference, with no ownership transfer or thread marshalling.
 * Uploads retain backend domain and owner-thread requirements. Do not use beyond its owner lifetime.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn CE::Input::System& CE::Runtime::Context::input() const
 * @brief Borrow the selected input adapter.
 * @return Context-associated reference, with no ownership transfer or thread marshalling.
 * Native polling and mutable binding operations retain their input-owner requirements. Do not use beyond its owner lifetime.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */
