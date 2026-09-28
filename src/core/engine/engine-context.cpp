#include <core/engine/engine-context.h>

#include <assets/abstracts/resource-provider.h>
#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <internals/exceptions.h>

#include <utility>

namespace CE::Engine {
    EngineContext::EngineContext(std::unique_ptr<iDisplaySystem> display,
                                 std::unique_ptr<RenderAPIs::iPresentationSurface> surface,
                                 std::unique_ptr<RenderAPIs::iRenderer> renderer,
                                 std::unique_ptr<Assets::ResourceProvider> resources,
                                 Input::iInputSystem& input) :
        display_(std::move(display)), surface_(std::move(surface)), renderer_(std::move(renderer)),
        resources_(std::move(resources)), input_(input) {
        if (!display_ || !surface_ || !renderer_ || !resources_)
            throw Exceptions::invalid_args(CE_HERE, "EngineContext requires display, surface, renderer, and resources");
    }

    EngineContext::~EngineContext() = default;

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
    Input::iInputSystem& EngineContext::input() const { return input_; }
}
