#pragma once

#include <memory>

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
     * The input adapter is borrowed (the current GLFW/Gainput adapter is a singleton).
     * Members are destroyed in reverse order: resources, renderer, surface, display.
     * GameRuntime coordinates their initialization and shutdown; this is not a game loop.
     */
    class EngineContext final {
    public:
        EngineContext(std::unique_ptr<iDisplaySystem> display,
                      std::unique_ptr<RenderAPIs::iPresentationSurface> surface,
                      std::unique_ptr<RenderAPIs::iRenderer> renderer,
                      std::unique_ptr<Assets::ResourceProvider> resources,
                      Input::iInputSystem& input);
        ~EngineContext();

        EngineContext(const EngineContext&) = delete;
        EngineContext& operator=(const EngineContext&) = delete;

        [[nodiscard]] iDisplaySystem& display() const;
        [[nodiscard]] iWindow& window() const;
        [[nodiscard]] RenderAPIs::iPresentationSurface& surface() const;
        [[nodiscard]] RenderAPIs::iRenderer& renderer() const;
        [[nodiscard]] Assets::ResourceProvider& resources() const;
        [[nodiscard]] Input::iInputSystem& input() const;

    private:
        std::unique_ptr<iDisplaySystem> display_;
        std::unique_ptr<RenderAPIs::iPresentationSurface> surface_;
        std::unique_ptr<RenderAPIs::iRenderer> renderer_;
        std::unique_ptr<Assets::ResourceProvider> resources_;
        Input::iInputSystem& input_;
    };
}
