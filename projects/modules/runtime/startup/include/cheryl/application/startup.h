#pragma once

#include <cheryl/export.h>

#include <core/engine/engine-context.h>
#include <core/game-framework/game-runtime.h>
#include <core/logging/process-lifetime.h>
#include <enums/window-mode.h>

#include <CLI/CLI.hpp>

#include <functional>
#include <memory>
#include <string>

namespace CE::Application {
    /** Initial simulation mode, input buffering and timing policy for a runtime. */
    struct RuntimeOptions {
        CE::Runtime::RunMode mode = CE::Runtime::RunMode::Sequential;
        CE::Input::PollingOptions polling;
        CE::Runtime::Simulation::TimingOptions timing;
    };

    /** Initial execution/runtime defaults, overridden by registered command-line options. */
    struct StartupOptions {
        CE::Runtime::ExecutionOptions execution;
        RuntimeOptions runtime;
    };

    enum class BackendSelection { Default, GLFWOpenGL };

    struct ApplicationOptions {
        int width = 1280;
        int height = 720;
        CE::Platform::WindowMode mode = CE::Platform::WindowMode::NORMAL;
        std::string title;
        int swap_interval = 1;
        CE::Runtime::ExecutionOptions execution;
        RuntimeOptions runtime;
        BackendSelection backend = BackendSelection::Default;
    };

    using OptionDefinitions = std::function<void(CLI::App&)>;

    /** Owned callbacks for one backend. arguments registers options during Startup
     * construction; validate runs after parsing; create constructs the context only
     * after validation. create is required. Captured borrowed values must survive
     * their callback's invocation.
     */
    struct StartupBackend {
        OptionDefinitions arguments{};
        std::function<void()> validate{};
        std::function<std::unique_ptr<CE::Runtime::Context>(const CE::Runtime::ExecutionOptions&)> create{};
    };

    /** Owns the constructed context and selected runtime policy. Help or validation
     * errors leave context empty and provide the process exit_code instead.
     */
    struct StartupResult {
        std::unique_ptr<CE::Runtime::Context> context{};
        RuntimeOptions runtime{};
        int exit_code = 0;

        /** @return Whether initialization produced a context ready for a runtime. */
        [[nodiscard]] bool should_start() const noexcept { return context != nullptr; }
        /** Construct a runtime borrowing this result's context and the game.
         * The result and game must survive the runtime and its blocking session.
         * Rejects a result without a context; construction does not start the session.
         * @param game Simulation hooks retained by reference.
         * @return A runtime using the selected mode, polling and timing policy.
         */
        [[nodiscard]] CE_STARTUP_API CE::Runtime::GameRuntime make_runtime(CE::Game::AbstractGame& game) const&;
        CE::Runtime::GameRuntime make_runtime(CE::Game::AbstractGame& game) const&& = delete;
    };

    /** Registers command-line options and performs one synchronous initialization.
     * The object is not synchronized or movable. Initialize on the platform owner;
     * returned contexts retain their backend's lifetime and thread requirements.
     * Owns a nested process logging guard. Keep Startup alive through the returned
     * context/game, or retain an outer Logging::ProcessLifetime until they finish.
     */
    class Startup final {
        ::CE::Logging::ProcessLifetime logging_lifetime_;
        StartupOptions configuration_;
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
        CE_STARTUP_API explicit Startup(
            StartupBackend backend,
            std::string description = "Cheryl application",
            StartupOptions configuration = {}
        );

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
        CE_STARTUP_API void add_application_options(const OptionDefinitions& definitions);
        /** Parse, validate and construct a context once on the platform owner.
         * Command-line help/errors and invalid configuration return without a context.
         * Reuse and malformed argument arrays are rejected; other backend failures propagate.
         * @param argc Positive argument count, including the executable name.
         * @param argv Nonnull array of argc nonnull argument strings.
         * @return An owned context and runtime policy, or an exit code without a context.
         */
        [[nodiscard]] CE_STARTUP_API StartupResult initialize(int argc, const char* const* argv);

    private:
        CE_STARTUP_API void add_engine_options();
        CE_STARTUP_API void validate_configuration() const;
    };
}

namespace CE::Application {
    // Standard startup integrates the backend selected when this module was built.
    // Applications can still provide a lower-level StartupBackend explicitly.
    [[nodiscard]] CE_STARTUP_API Startup
    make_startup(std::string description = "Cheryl application", ApplicationOptions configuration = {});
    // Keep borrowed input alive through the returned context's teardown.
    [[nodiscard]] CE_STARTUP_API Startup
    make_startup(CE::Input::System& input, std::string description = "Cheryl application", ApplicationOptions configuration = {});
}
