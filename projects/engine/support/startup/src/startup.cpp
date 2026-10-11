#include <core/engine/startup.h>

#include <internals/exceptions.h>

#include <chrono>
#include <cstddef>
#include <string_view>
#include <utility>

namespace CE::Engine {
    GFramework::GameRuntime StartupResult::make_runtime(GFramework::AbstractGame& game) const& {
        if (!engine)
            throw Exceptions::failed_operation(CE_HERE, "A runtime requires successful engine startup");
        return GFramework::GameRuntime(*engine, game, runtime.mode, runtime.polling, runtime.timing);
    }

    Startup::Startup(StartupBackend backend, std::string description, StartupConfiguration configuration)
    : configuration_(std::move(configuration)), backend_(std::move(backend)), parser_(std::move(description)) {
        if (!backend_.create)
            throw Exceptions::invalid_args(CE_HERE, "Engine startup requires a backend factory");
        add_engine_options();
        if (backend_.arguments)
            backend_.arguments(*parser_.add_option_group("Backend"));
        application_options_ = parser_.add_option_group("Application");
    }

    void Startup::add_engine_options() {
        auto& options = *parser_.add_option_group("Engine");
#if CHERYL_DEBUG_TERMINAL_AVAILABLE
        options.add_flag("--debug-terminal", "Display the native output terminal (automatic in Debug)");
        options.add_flag("--no-debug-terminal", "Use inherited output without a native terminal");
#endif
        options
            .add_flag_callback(
                "--concurrent", [this] { configuration_.runtime.mode = GFramework::RunMode::Concurrent; },
                "Run simulation on a separate worker"
            )
            ->disable_flag_override()
            ->trigger_on_parse();
        options
            .add_flag_callback(
                "--fixed", [this] { configuration_.runtime.timing.mode = GFramework::SimulationMode::Fixed; }, "Use fixed simulation steps"
            )
            ->disable_flag_override()
            ->trigger_on_parse();
        options
            .add_flag_callback(
                "--variable-catch-up",
                [this] {
                    configuration_.runtime.timing.mode = GFramework::SimulationMode::Fixed;
                    configuration_.runtime.timing.recovery = GFramework::LagRecovery::VariableCatchUp;
                },
                "Recover fixed-step lag with a bounded variable update"
            )
            ->disable_flag_override()
            ->trigger_on_parse();
        options
            .add_option_function<unsigned int>(
                "--fixed-step-ms",
                [this](const unsigned int value) {
                    configuration_.runtime.timing.mode = GFramework::SimulationMode::Fixed;
                    configuration_.runtime.timing.fixed_step = std::chrono::milliseconds(value);
                },
                "Fixed simulation step in milliseconds"
            )
            ->check(CLI::NonNegativeNumber)
            ->trigger_on_parse();
        options
            .add_option_function<unsigned int>(
                "--variable-interval-ms",
                [this](const unsigned int value) { configuration_.runtime.timing.variable_interval = std::chrono::milliseconds(value); },
                "Variable simulation interval in milliseconds; zero runs unpaced"
            )
            ->check(CLI::NonNegativeNumber)
            ->trigger_on_parse();
        options
            .add_option("--max-fixed-updates", configuration_.runtime.timing.max_fixed_updates, "Maximum fixed updates before lag recovery")
            ->check(CLI::NonNegativeNumber)
            ->trigger_on_parse();
        options
            .add_option(
                "--recovery-prefix", configuration_.runtime.timing.fixed_updates_before_recovery,
                "Fixed updates before a variable catch-up update"
            )
            ->check(CLI::NonNegativeNumber)
            ->trigger_on_parse();
        options
            .add_option_function<unsigned int>(
                "--recovery-cap-ms",
                [this](const unsigned int value) { configuration_.runtime.timing.recovery_cap = std::chrono::milliseconds(value); },
                "Variable catch-up limit in milliseconds; zero removes the limit"
            )
            ->check(CLI::NonNegativeNumber)
            ->trigger_on_parse();
        options
            .add_flag_callback(
                "--input-unlimited", [this] { configuration_.runtime.polling.policy = Input::PollingPolicy::Unlimited; },
                "Allow unlimited completed polls between simulation updates"
            )
            ->disable_flag_override()
            ->trigger_on_parse();
        options
            .add_option_function<std::size_t>(
                "--input-capacity",
                [this](const std::size_t value) {
                    configuration_.runtime.polling.policy = Input::PollingPolicy::Finite;
                    configuration_.runtime.polling.capacity = value;
                },
                "Maximum completed polls between simulation updates"
            )
            ->check(CLI::NonNegativeNumber)
            ->trigger_on_parse();
        options
            .add_option_function<unsigned int>(
                "--input-spacing-ms",
                [this](const unsigned int value) { configuration_.runtime.polling.spacing = std::chrono::milliseconds(value); },
                "Minimum delay between completed polls in milliseconds"
            )
            ->check(CLI::NonNegativeNumber)
            ->trigger_on_parse();
        options.add_option("--worker-count", configuration_.execution.worker_count, "Capacity of the engine-owned worker pool")
            ->check(CLI::NonNegativeNumber)
            ->trigger_on_parse();
    }

    void Startup::add_application_options(const ArgumentDefinitions& definitions) {
        if (initialized_)
            throw Exceptions::failed_operation(CE_HERE, "Application arguments must be registered before engine startup");
        if (!definitions)
            throw Exceptions::invalid_args(CE_HERE, "Application argument definitions must not be empty");
        definitions(*application_options_);
    }

    void Startup::validate_configuration() const {
        static_cast<void>(Input::PollingBacklog(configuration_.runtime.polling));
        static_cast<void>(GFramework::SimulationScheduler(configuration_.runtime.timing));
        if (configuration_.runtime.mode != GFramework::RunMode::Sequential &&
            configuration_.runtime.mode != GFramework::RunMode::Concurrent)
            throw Exceptions::invalid_args(CE_HERE, "Unknown game runtime mode");
        if (!configuration_.execution.shared_pool && configuration_.execution.worker_count == 0)
            throw Exceptions::invalid_args(CE_HERE, "Owned execution requires a positive worker count");
    }

    StartupResult Startup::initialize(const int argc, const char* const* argv) {
        if (initialized_)
            throw Exceptions::failed_operation(CE_HERE, "Engine startup is single-use");
        if (argc <= 0 || !argv)
            throw Exceptions::invalid_args(CE_HERE, "Engine startup requires an executable name and argument array");
        for (int index = 0; index < argc; ++index) {
            if (!argv[index])
                throw Exceptions::invalid_args(CE_HERE, "Engine arguments must not contain null strings");
        }
        initialized_ = true;
        try {
            parser_.parse(argc, argv);
            validate_configuration();
            // TODO: Resolve backend selection from CLI overrides, application preferences, and the build default here.
            // Runtime-loaded backend discovery can later supply the same factory interface.
            if (backend_.validate)
                backend_.validate();
        } catch (const CLI::ParseError& error) {
            return {.exit_code = parser_.exit(error)};
        } catch (const Exceptions::invalid_args& error) {
            // Keep the validation cause; the engine's location/trace prefix is not CLI help.
            const std::string_view summary = error.diagnostic_summary();
            const auto separator = summary.find('\n');
            const auto cause = summary.substr(separator == std::string_view::npos ? 0 : separator + 1);
            return {.exit_code = parser_.exit(CLI::ValidationError("Engine configuration", std::string(cause)))};
        }
        auto engine = backend_.create(configuration_.execution);
        if (!engine)
            throw Exceptions::failed_operation(CE_HERE, "The startup backend factory returned no engine context");
        return {.engine = std::move(engine), .runtime = configuration_.runtime};
    }
}
