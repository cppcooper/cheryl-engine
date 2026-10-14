#include <backends/opengl/startup.h>

#if CHERYL_NATIVE_INPUT
#include <core/controls/input-system.h>
#include <core/logging.h>
#endif

#include <memory>
#include <utility>

namespace CE::Application::GLFWOpenGL {
    namespace {
        struct GlfwStartupOptions {
            Options context;
            bool input_diagnostics = false;
        };

        void add_glfw_options(CLI::App& options, GlfwStartupOptions& configuration) {
            options.add_option("--window-width", configuration.context.width, "Window width")
                ->check(CLI::PositiveNumber)
                ->capture_default_str()
                ->trigger_on_parse();
            options.add_option("--window-height", configuration.context.height, "Window height")
                ->check(CLI::PositiveNumber)
                ->capture_default_str()
                ->trigger_on_parse();
            options.add_option("--window-title", configuration.context.title, "Window title")->trigger_on_parse();
            options.add_option("--swap-interval", configuration.context.swap_interval, "OpenGL presentation swap interval")
                ->check(CLI::NonNegativeNumber)
                ->capture_default_str()
                ->trigger_on_parse();
        }

        void validate_glfw_options(const GlfwStartupOptions& configuration) {
            if (configuration.context.width <= 0 || configuration.context.height <= 0 || configuration.context.swap_interval < 0)
                throw CLI::ValidationError("Backend configuration", "Window dimensions must be positive and swap interval nonnegative");
#if CHERYL_NATIVE_INPUT
            if (configuration.input_diagnostics && !CE::Logging::Compile::enabled(CE::Logging::Compile::TRACE_))
                throw CLI::ValidationError("--input-diagnostics", "Requires a build with TRACE logging");
#endif
        }
    }

    CE::Application::Startup
    make_startup(CE::Input::System& input, std::string description, Options configuration, CE::Application::RuntimeOptions runtime) {
        auto options = std::make_shared<GlfwStartupOptions>(GlfwStartupOptions{std::move(configuration)});
        CE::Application::StartupBackend backend{
            .arguments = [options](CLI::App& arguments) { add_glfw_options(arguments, *options); },
            .validate = [options] { validate_glfw_options(*options); },
            .create =
                [&input, options](const CE::Runtime::ExecutionOptions& execution) {
                    auto context = options->context;
                    context.execution = execution;
                    return make_context(input, context);
                }
        };
        return CE::Application::Startup(
            std::move(backend), std::move(description), {.execution = options->context.execution, .runtime = runtime}
        );
    }

#if CHERYL_NATIVE_INPUT
    CE::Application::Startup make_startup(std::string description, Options configuration, CE::Application::RuntimeOptions runtime) {
        auto options = std::make_shared<GlfwStartupOptions>(GlfwStartupOptions{std::move(configuration)});
        CE::Application::StartupBackend backend{
            .arguments =
                [options](CLI::App& arguments) {
                    add_glfw_options(arguments, *options);
                    arguments.add_flag("--input-diagnostics", options->input_diagnostics, "Enable native gamepad diagnostics at TRACE")
                        ->trigger_on_parse();
                },
            .validate = [options] { validate_glfw_options(*options); },
            .create =
                [options](const CE::Runtime::ExecutionOptions& execution) {
                    auto context = options->context;
                    context.execution = execution;
                    if (options->input_diagnostics) {
                        auto config = CE::Logging::Config::for_logger(CE::Logging::Categories::platform);
                        config.logger_level = config.file_level = spdlog::level::trace;
                        CE::Logging::Logger<CE::Logging::Categories::platform>::initialize(spdlog::file_event_handlers{}, config);
                    }
                    auto engine = make_context(context);
                    if (options->input_diagnostics)
                        static_cast<CE::Platform::GLFW::InputSystem&>(engine->input()).set_gamepad_diagnostics(true);
                    return engine;
                }
        };
        return CE::Application::Startup(
            std::move(backend), std::move(description), {.execution = options->context.execution, .runtime = runtime}
        );
    }
#endif
}
