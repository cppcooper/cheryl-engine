#pragma once

#include "platform-dispatcher.h"

#include <atomic>
#include <memory>

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
    /** Owns a compatible set of platform, presentation, rendering, and resource adapters.
     * Input can be owned or explicitly borrowed. Owned input is destroyed before
     * resources, renderer, surface, and display, so callbacks detach from a live window.
     * GameRuntime coordinates their initialization and shutdown; this is not a game loop.
     * One context supports one runtime session; stopped adapters are not restarted.
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

    public:
        EngineContext(std::unique_ptr<iDisplaySystem> display,
                      std::unique_ptr<RenderAPIs::iPresentationSurface> surface,
                      std::unique_ptr<RenderAPIs::iRenderer> renderer,
                      std::unique_ptr<Assets::ResourceProvider> resources,
                      Input::iInputSystem& input);
        EngineContext(std::unique_ptr<iDisplaySystem> display,
                      std::unique_ptr<RenderAPIs::iPresentationSurface> surface,
                      std::unique_ptr<RenderAPIs::iRenderer> renderer,
                      std::unique_ptr<Assets::ResourceProvider> resources,
                      std::unique_ptr<Input::iInputSystem> input);
        ~EngineContext();

        EngineContext(const EngineContext&) = delete;
        EngineContext& operator=(const EngineContext&) = delete;

        [[nodiscard]] iDisplaySystem& display() const;
        [[nodiscard]] iWindow& window() const;
        [[nodiscard]] RenderAPIs::iPresentationSurface& surface() const;
        [[nodiscard]] RenderAPIs::iRenderer& renderer() const;
        [[nodiscard]] Assets::ResourceProvider& resources() const;
        [[nodiscard]] Input::iInputSystem& input() const;
        [[nodiscard]] PlatformDispatcher& platform_dispatcher() { return platform_dispatcher_; }

    private:
        friend class GFramework::GameRuntime;
        void validate() const;
        void begin_session();

    };
}
