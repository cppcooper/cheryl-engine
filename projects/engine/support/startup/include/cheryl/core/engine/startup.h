#pragma once

#include <core/engine/engine-context.h>
#include <core/game-framework/game-runtime.h>

#include <CLI/CLI.hpp>

#include <functional>
#include <memory>
#include <string>

namespace CE::Engine {
    struct RuntimeConfiguration {
        GFramework::RunMode mode = GFramework::RunMode::Sequential;
        Input::PollingOptions polling;
        GFramework::SimulationTimingOptions timing;
    };

    struct StartupConfiguration {
        ExecutionOptions execution;
        RuntimeConfiguration runtime;
    };

    using ArgumentDefinitions = std::function<void(CLI::App&)>;

    struct StartupBackend {
        ArgumentDefinitions arguments{};
        std::function<void()> validate{};
        std::function<std::unique_ptr<EngineContext>(const ExecutionOptions&)> create{};
    };

    struct StartupResult {
        std::unique_ptr<EngineContext> engine{};
        RuntimeConfiguration runtime{};
        int exit_code = 0;

        [[nodiscard]] bool should_start() const noexcept { return engine != nullptr; }
        // The result and game must outlive the returned runtime.
        [[nodiscard]] GFramework::GameRuntime make_runtime(GFramework::AbstractGame& game) const&;
        GFramework::GameRuntime make_runtime(GFramework::AbstractGame& game) const&& = delete;
    };

    class Startup final {
        StartupConfiguration configuration_;
        StartupBackend backend_;
        bool initialized_ = false;
        CLI::App parser_;
        CLI::App* application_options_ = nullptr;

    public:
        explicit Startup(StartupBackend backend, std::string description = "Cheryl application", StartupConfiguration configuration = {});

        // Parser callbacks bind this object's configuration; its address is stable.
        Startup(const Startup&) = delete;
        Startup& operator=(const Startup&) = delete;
        Startup(Startup&&) = delete;
        Startup& operator=(Startup&&) = delete;

        // Definitions run immediately; bound application values must survive initialize().
        void add_application_options(const ArgumentDefinitions& definitions);
        // One synchronous attempt on the platform owner. Help/errors return without
        // a context; construction failures propagate after argument validation.
        [[nodiscard]] StartupResult initialize(int argc, const char* const* argv);

    private:
        void add_engine_options();
        void validate_configuration() const;
    };
}
