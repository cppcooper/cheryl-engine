#include <backends/opengl/startup.h>
#include "startup-internal.h"

#include <internals/exceptions.h>
#if CHERYL_NATIVE_INPUT
#include <core/controls/input-system.h>
#include <core/logging.h>
#endif

#include <memory>
#include <utility>

namespace CE::Application::Detail {
    namespace {
        struct OpenGLStartupOptions {
            GLFWOpenGL::Options context;
            std::string backend = "opengl";
            bool input_diagnostics = false;
        };

        void add_options(CLI::App& options, OpenGLStartupOptions& configuration, const bool owned_input) {
            options.add_option("--backend", configuration.backend, "Configured startup backend")
                ->check(CLI::IsMember({"default", "opengl"}))
                ->capture_default_str()
                ->trigger_on_parse();
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
#if CHERYL_NATIVE_INPUT
            if (owned_input)
                options.add_flag("--input-diagnostics", configuration.input_diagnostics, "Enable native gamepad diagnostics at TRACE")
                    ->trigger_on_parse();
#else
            static_cast<void>(owned_input);
#endif
        }

        void validate(const OpenGLStartupOptions& configuration) {
            if (configuration.context.width <= 0 || configuration.context.height <= 0 || configuration.context.swap_interval < 0)
                throw CLI::ValidationError("Backend configuration", "Window dimensions must be positive and swap interval nonnegative");
#if CHERYL_NATIVE_INPUT
            if (configuration.input_diagnostics && !CE::Logging::Compile::enabled(CE::Logging::Compile::TRACE_))
                throw CLI::ValidationError("--input-diagnostics", "Requires a build with TRACE logging");
#endif
        }
    }

    Startup make_opengl_startup(CE::Input::System* input, std::string description, ApplicationOptions configuration) {
        if (configuration.backend != BackendSelection::Default && configuration.backend != BackendSelection::GLFWOpenGL)
            throw CE::Exceptions::invalid_args(CE_HERE, "The requested startup backend is not configured");
        auto options = std::make_shared<OpenGLStartupOptions>(OpenGLStartupOptions{
            {.width = configuration.width,
             .height = configuration.height,
             .mode = configuration.mode,
             .title = std::move(configuration.title),
             .swap_interval = configuration.swap_interval,
             .execution = configuration.execution}
        });
        StartupBackend backend{
            .arguments = [input, options](CLI::App& arguments) { add_options(arguments, *options, input == nullptr); },
            .validate = [options] { validate(*options); },
            .create = [input, options](const CE::Runtime::ExecutionOptions& execution) -> std::unique_ptr<CE::Runtime::Context> {
                auto context = options->context;
                context.execution = execution;
                if (input)
                    return GLFWOpenGL::make_context(*input, context);
#if CHERYL_NATIVE_INPUT
                if (options->input_diagnostics) {
                    auto logging = CE::Logging::Config::for_logger(CE::Logging::Categories::platform);
                    logging.logger_level = logging.file_level = CE::Logging::Level::trace;
                    CE::Logging::Logger<CE::Logging::Categories::platform>::initialize(CE::Logging::FileEventHandlers{}, logging);
                }
                auto result = GLFWOpenGL::make_context(context);
                if (options->input_diagnostics)
                    static_cast<CE::Platform::GLFW::InputSystem&>(result->input()).set_gamepad_diagnostics(true);
                return result;
#else
                throw CE::Exceptions::failed_operation(CE_HERE, "The selected startup backend requires application-supplied input");
#endif
            }
        };
        return Startup(
            std::move(backend), std::move(description), {.execution = configuration.execution, .runtime = configuration.runtime}
        );
    }
}

namespace CE::Application::GLFWOpenGL {
    namespace {
        ApplicationOptions generic_options(Options configuration, RuntimeOptions runtime) {
            return {
                .width = configuration.width,
                .height = configuration.height,
                .mode = configuration.mode,
                .title = std::move(configuration.title),
                .swap_interval = configuration.swap_interval,
                .execution = configuration.execution,
                .runtime = runtime,
                .backend = BackendSelection::GLFWOpenGL
            };
        }
    }

    Startup make_startup(CE::Input::System& input, std::string description, Options configuration, RuntimeOptions runtime) {
        return CE::Application::make_startup(input, std::move(description), generic_options(std::move(configuration), runtime));
    }
#if CHERYL_NATIVE_INPUT
    Startup make_startup(std::string description, Options configuration, RuntimeOptions runtime) {
        return CE::Application::make_startup(std::move(description), generic_options(std::move(configuration), runtime));
    }
#endif
}
