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
    /** Initial owner placement, simulation/input policy and independent render pacing. */
    struct RuntimeOptions {
        CE::Runtime::RunMode mode = CE::Runtime::RunMode::Sequential; ///< Sequential/concurrent owner placement, independent of simulation pacing.
        CE::Input::PollingOptions polling; ///< Input observation/backlog settings validated before context creation.
        CE::Runtime::Simulation::TimingOptions timing; ///< Variable/fixed simulation delta and bounded recovery settings.
        CE::Runtime::FramePacingOptions frame_pacing; ///< Independent render cap; disabled by default, retaining a 60 FPS target.
    };

    /** Initial execution/runtime defaults, overridden by registered command-line options. */
    struct StartupOptions {
        CE::Runtime::ExecutionOptions execution; ///< Context CPU capacity or retained shared pool; a zero owned capacity is invalid.
        RuntimeOptions runtime; ///< Initial runtime defaults before registered command-line overrides.
    };

    /** Requested standard backend; this does not load/select arbitrary runtime plugins. */
    enum class BackendSelection { Default, ///< Resolve the backend selected when Startup was built.
        GLFWOpenGL ///< Require the built GLFW/OpenGL integration.
    };

    /** Owned defaults for the standard backend factory and subsequent runtime; command-line options can override them. */
    struct ApplicationOptions {
        int width = 1280; ///< Positive initial logical window width.
        int height = 720; ///< Positive initial logical window height.
        CE::Platform::WindowMode mode = CE::Platform::WindowMode::NORMAL; ///< Initial native window mode, not the simulation RunMode.
        std::string title; ///< Owned initial window title.
        int swap_interval = 1; ///< Nonnegative presentation swap interval for the standard backend.
        CE::Runtime::ExecutionOptions execution; ///< Context execution settings; shared_pool retains application-supplied capacity.
        RuntimeOptions runtime; ///< Simulation execution, pacing and input buffering defaults.
        BackendSelection backend = BackendSelection::Default; ///< Default compiled backend or explicit supported GLFWOpenGL choice.
    };

    /** Synchronous registration callback borrowing CLI::App; bound values must remain alive through initialize(). */
    using OptionDefinitions = std::function<void(CLI::App&)>;

    /** Owned callbacks for one backend. arguments registers options during Startup
     * construction; validate runs after parsing; create constructs the context only
     * after validation. create is required. Captured borrowed values must survive
     * their callback's invocation.
     */
    struct StartupBackend {
        OptionDefinitions arguments{}; ///< Optional option registrar called during Startup construction; captures/bound values must survive their uses.
        std::function<void()> validate{}; ///< Optional post-parse validation before create; invalid_args/CLI parse validation becomes an error result.
        std::function<std::unique_ptr<CE::Runtime::Context>(const CE::Runtime::ExecutionOptions&)> create{}; ///< Required context factory invoked after validation with selected execution options; returns non-null owned Context or propagates failure.
    };

    /** Owns the constructed context and selected runtime policy. Help or validation
     * errors leave context empty and provide the process exit_code instead.
     */
    struct StartupResult {
        std::unique_ptr<CE::Runtime::Context> context{}; ///< Owned constructed context, or empty for help/validation results. Must outlive every borrowing runtime.
        RuntimeOptions runtime{}; ///< Selected runtime policy after parsing, copied into make_runtime().
        int exit_code = 0; ///< Process result for a context-free help/error result; zero for ordinary success/help.

        [[nodiscard]] bool should_start() const noexcept { return context != nullptr; }
        [[nodiscard]] CE_STARTUP_API CE::Runtime::GameRuntime make_runtime(CE::Game::AbstractGame& game) const&; ///< Construct an unstarted runtime borrowing context and game; see StartupResult lifetime and construction contract.
        CE::Runtime::GameRuntime make_runtime(CE::Game::AbstractGame& game) const&& = delete; ///< Disabled for temporary results, whose context would not outlive the runtime.
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

        CE_STARTUP_API void add_application_options(const OptionDefinitions& definitions);
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

/**
 * @fn bool CE::Application::StartupResult::should_start() const noexcept
 * @brief Check whether initialization produced a context.
 * @return context != nullptr; false means use exit_code without constructing/running a game runtime.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @struct CE::Application::StartupResult
 * @details
 * @par Runtime construction
 * make_runtime(game) is available on a const lvalue result. It returns an unstarted
 * runtime configured with this result's mode, polling, timing and frame-pacing settings. The
 * result/context and borrowed game must outlive runtime execution and cleanup.
 * Construction does not begin the context's single runtime session.
 *
 * Calling make_runtime(game) with an empty context throws
 * CE::Exceptions::failed_operation; invalid selected
 * runtime policies and allocation failures can also propagate. Construction from
 * a temporary result is deleted to prevent borrowing its soon-destroyed context.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @fn CE::Application::Startup::Startup(StartupBackend backend, std::string description, StartupOptions configuration)
 * @brief Register common/backend options without constructing native context state.
 * @param backend Owned callbacks with required create factory.
 * @param description Owned description used in CLI help.
 * @param configuration Initial execution/runtime settings, overridden by registered arguments.
 * @throws CE::Exceptions::invalid_args For a missing factory.
 * Registration/allocation callback failures propagate. Startup owns a nested ProcessLifetime; keep it or an outer guard alive until all returned context/game work finishes. Callback-bound values must remain alive through initialize().
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @fn void CE::Application::Startup::add_application_options(const OptionDefinitions& definitions)
 * @brief Invoke an application registrar immediately before initialization.
 * @param definitions Nonempty callback borrowing the application CLI option group.
 * @throws CE::Exceptions::invalid_args For an empty callback.
 * @throws CE::Exceptions::failed_operation After initialization has begun.
 * Keep parser-bound values/captures alive through initialize(). Registrar/CLI failures propagate; this operation is not synchronized.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @fn CE::Application::StartupResult CE::Application::Startup::initialize(int argc, const char* const* argv)
 * @brief Parse, validate and construct a context once on the platform owner.
 * @param argc Positive count including the executable name.
 * @param argv Non-null array of argc non-null argument strings, borrowed for parsing.
 * @return Owned context/runtime policy, or empty context plus help/validation exit_code.
 * @throws CE::Exceptions::failed_operation For reuse or a factory returning nullptr.
 * @throws CE::Exceptions::invalid_args For malformed argument arrays before parsing.
 * CLI parse errors and configuration invalid_args return before context creation. Other backend/allocation failures propagate. Once valid arrays begin parsing the object is consumed even if initialization returns help/error or throws; it is not synchronized.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @fn CE::Application::Startup::Startup(const Startup&)
 * @brief Copy/move is disabled because registered callbacks bind this object's stable configuration address.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @fn Startup& CE::Application::Startup::operator=(const Startup&)
 * @brief Copy/move is disabled because registered callbacks bind this object's stable configuration address.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @fn CE::Application::Startup::Startup(Startup&&)
 * @brief Copy/move is disabled because registered callbacks bind this object's stable configuration address.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @fn Startup& CE::Application::Startup::operator=(Startup&&)
 * @brief Copy/move is disabled because registered callbacks bind this object's stable configuration address.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @fn CE::Application::Startup CE::Application::make_startup(std::string description, ApplicationOptions configuration)
 * @brief Configure the standard backend compiled into Cheryl::Startup without constructing a context.
 * @param description Owned CLI help description.
 * @param configuration Window/backend/execution/runtime defaults; command-line options override registered values.
 * @return Single-use Startup with retained backend option state.
 * @throws CE::Exceptions::failed_operation If no standard backend was built or owned native input is unavailable when creation is attempted.
 * Unknown backend selections reject. This is a build-time backend integration; applications can supply a lower-level StartupBackend when another composition is needed.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */

/**
 * @fn CE::Application::Startup CE::Application::make_startup(CE::Input::System& input, std::string description, ApplicationOptions configuration)
 * @brief Configure the standard backend compiled into Cheryl::Startup without constructing a context.
 * @param input Borrowed adapter that must survive returned context teardown and detach while its window lives.
 * @param description Owned CLI help description.
 * @param configuration Window/backend/execution/runtime defaults; command-line options override registered values.
 * @return Single-use Startup with retained backend option state.
 * @throws CE::Exceptions::failed_operation If no standard backend was built.
 * Unknown backend selections reject. This is a build-time backend integration; applications can supply a lower-level StartupBackend when another composition is needed.
 * @see [Detailed guide](cheryl-guide:development/consuming-engine#command-line-startup)
 */
