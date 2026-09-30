#include <core/engine/engine-context.h>

#include <assets/resources/resource-provider.h>
#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <internals/exceptions.h>

#include <utility>

namespace CE::Engine {
    EngineContext::EngineContext(
        std::unique_ptr<iDisplaySystem> display,
        std::unique_ptr<RenderAPIs::iPresentationSurface> surface,
        std::unique_ptr<RenderAPIs::iRenderer> renderer,
        std::unique_ptr<Assets::ResourceProvider> resources,
        Input::iInputSystem& input,
        ExecutionOptions execution
    )
    : display_(std::move(display)),
      surface_(std::move(surface)),
      renderer_(std::move(renderer)),
      resources_(std::move(resources)),
      input_(&input),
      execution_(std::move(execution)) {
        validate();
    }

    EngineContext::EngineContext(
        std::unique_ptr<iDisplaySystem> display,
        std::unique_ptr<RenderAPIs::iPresentationSurface> surface,
        std::unique_ptr<RenderAPIs::iRenderer> renderer,
        std::unique_ptr<Assets::ResourceProvider> resources,
        std::unique_ptr<Input::iInputSystem> input,
        ExecutionOptions execution
    )
    : display_(std::move(display)),
      surface_(std::move(surface)),
      renderer_(std::move(renderer)),
      resources_(std::move(resources)),
      owned_input_(std::move(input)),
      input_(owned_input_.get()),
      execution_(std::move(execution)) {
        validate();
    }

    void EngineContext::validate() const {
        if (!display_ || !surface_ || !renderer_ || !resources_ || !input_)
            throw Exceptions::invalid_args(CE_HERE, "EngineContext requires display, surface, renderer, resources, and input");
        if (!execution_.shared_pool && execution_.worker_count == 0)
            throw Exceptions::invalid_args(CE_HERE, "Owned execution requires a positive worker count");
    }

    void EngineContext::begin_session() {
        if (session_started_.exchange(true, std::memory_order_acq_rel))
            throw Exceptions::failed_operation(CE_HERE, "EngineContext supports only one runtime session");
    }

    EngineContext::~EngineContext() {
        close_worker_submissions();
        finish_workers();
    }

    WorkerGroup EngineContext::make_worker_group(WorkerGroupOptions options) {
        std::lock_guard lock(execution_mutex_);
        if (worker_submissions_closed_)
            throw Exceptions::failed_operation(CE_HERE, "EngineContext worker submissions are closed");
        WorkerPool* pool = execution_.shared_pool.get();
        if (!pool) {
            if (!owned_workers_)
                owned_workers_ = std::make_unique<WorkerPool>(execution_.worker_count);
            pool = owned_workers_.get();
        }
        auto group = pool->make_group(std::move(options));
        worker_groups_.push_back(group);
        return group;
    }

    void EngineContext::close_worker_submissions() {
        std::lock_guard lock(execution_mutex_);
        worker_submissions_closed_ = true;
        for (const auto& group : worker_groups_)
            group.close();
    }

    bool EngineContext::workers_idle() const {
        std::lock_guard lock(execution_mutex_);
        for (const auto& group : worker_groups_) {
            const auto status = group.status();
            if (status.pending != 0 || status.running != 0)
                return false;
        }
        return true;
    }

    void EngineContext::finish_workers() {
        std::vector<WorkerGroup> groups;
        WorkerPool* owned;
        {
            std::lock_guard lock(execution_mutex_);
            groups = worker_groups_;
            owned = owned_workers_.get();
        }
        // Jobs may reenter the context while finishing. Never hold its mutex
        // while waiting, and never shut down unrelated injected-pool groups.
        for (const auto& group : groups)
            group.drain();
        if (owned)
            owned->shutdown();
    }

    iDisplaySystem& EngineContext::display() const { return *display_; }

    iWindow& EngineContext::window() const {
        auto* active = display_->active_window();
        if (!active)
            throw Exceptions::failed_operation(CE_HERE, "EngineContext has no active window");
        return *active;
    }

    RenderAPIs::iPresentationSurface& EngineContext::surface() const { return *surface_; }
    RenderAPIs::iRenderer& EngineContext::renderer() const { return *renderer_; }
    Assets::ResourceProvider& EngineContext::resources() const { return *resources_; }
    Input::iInputSystem& EngineContext::input() const { return *input_; }
}
