#pragma once

#include <core/engine/engine-context.h>
#include <core/game-framework/game-runtime.h>

#include <CLI/CLI.hpp>

#include <functional>
#include <memory>
#include <string>

namespace CE::Engine {
    /** Initial simulation mode, input buffering and timing policy for a runtime. */
    struct RuntimeConfiguration {
        GFramework::RunMode mode = GFramework::RunMode::Sequential;
        Input::PollingOptions polling;
        GFramework::SimulationTimingOptions timing;
    };

    /** Initial execution/runtime defaults, overridden by registered command-line options. */
    struct StartupConfiguration {
        ExecutionOptions execution;
        RuntimeConfiguration runtime;
    };

    using ArgumentDefinitions = std::function<void(CLI::App&)>;

    /** Owned callbacks for one backend. arguments registers options during Startup
     * construction; validate runs after parsing; create constructs the context only
     * after validation. create is required. Captured borrowed values must survive
     * their callback's invocation.
     */
    struct StartupBackend {
        ArgumentDefinitions arguments{};
        std::function<void()> validate{};
        std::function<std::unique_ptr<EngineContext>(const ExecutionOptions&)> create{};
    };

    /** Owns the constructed context and selected runtime policy. Help or validation
     * errors leave engine empty and provide the process exit_code instead.
     */
    struct StartupResult {
        std::unique_ptr<EngineContext> engine{};
        RuntimeConfiguration runtime{};
        int exit_code = 0;

        /** @return Whether initialization produced a context ready for a runtime. */
        [[nodiscard]] bool should_start() const noexcept { return engine != nullptr; }
        /** Construct a runtime borrowing this result's context and the game.
         * The result and game must survive the runtime and its blocking session.
         * Rejects a result without a context; construction does not start the session.
         * @param game Simulation hooks retained by reference.
         * @return A runtime using the selected mode, polling and timing policy.
         */
        [[nodiscard]] GFramework::GameRuntime make_runtime(GFramework::AbstractGame& game) const&;
        GFramework::GameRuntime make_runtime(GFramework::AbstractGame& game) const&& = delete;
    };

    /** Registers command-line options and performs one synchronous initialization.
     * The object is not synchronized or movable. Initialize on the platform owner;
     * returned contexts retain their backend's lifetime and thread requirements.
     */
    class Startup final {
        StartupConfiguration configuration_;
        StartupBackend backend_;
        bool initialized_ = false;
        CLI::App parser_;
        CLI::App* application_options_ = nullptr;

    public:
        /** Register engine and backend options without constructing the context.
         * A missing backend factory is rejected; callback failures propagate.
         * @param backend Owned callbacks, with a required context factory.
         * @param description Application description used by command-line help.
         * @param configuration Initial defaults before command-line overrides.
         */
        explicit Startup(StartupBackend backend, std::string description = "Cheryl application", StartupConfiguration configuration = {});

        // Parser callbacks bind this object's configuration; its address is stable.
        Startup(const Startup&) = delete;
        Startup& operator=(const Startup&) = delete;
        Startup(Startup&&) = delete;
        Startup& operator=(Startup&&) = delete;

        /** Register application options before initialization.
         * Definitions run immediately; values bound to the parser must survive
         * initialize(). Empty definitions or registration after initialization fail.
         * @param definitions Callback receiving the application's option group.
         */
        void add_application_options(const ArgumentDefinitions& definitions);
        /** Parse, validate and construct a context once on the platform owner.
         * Command-line help/errors and invalid configuration return without a context.
         * Reuse and malformed argument arrays are rejected; other backend failures propagate.
         * @param argc Positive argument count, including the executable name.
         * @param argv Nonnull array of argc nonnull argument strings.
         * @return An owned context and runtime policy, or an exit code without a context.
         */
        [[nodiscard]] StartupResult initialize(int argc, const char* const* argv);

    private:
        void add_engine_options();
        void validate_configuration() const;
    };
}
