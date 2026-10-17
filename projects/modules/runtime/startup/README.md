# Startup module

Startup is the public application-bootstrap module. It registers command-line
options, validates configuration, constructs the selected context and owns a
process-logging lifetime around application execution. Engine supplies the
portable runtime contracts; Startup privately integrates configured concrete
modules without adding reverse dependencies to Engine.

## Selection and dependency

Select `CHERYL_BUILD_STARTUP=ON` and link `Cheryl::Startup`. The canonical public
header is `<cheryl/application/startup.h>`; `<cheryl/modules/startup.h>` is its
module umbrella. Startup is one compiled library following Cheryl's selected
shared/static mode. CLI11 remains the explicit header-only option-registration
frontend; consumers using `CLI::App` receive its public usage requirements.

`CHERYL_STARTUP_OPENGL=ON` includes private GLFW/OpenGL integration and requires
the selected OpenGL/Native GLFW composition. Its public feature is
`CHERYL_HAS_STARTUP_OPENGL`; the target property `CHERYL_STARTUP_OPENGL` records
the configured choice. Disabling that integration leaves the lower-level
`StartupBackend` API available, but the standard `make_startup()` factory rejects
because no standard backend was included.

```cmake
target_link_libraries(game PRIVATE Cheryl::Startup)
```

A Startup binary with OpenGL integration requires the corresponding module
binaries at deployment even if a particular run never creates a window.
Omitting those binaries requires a Startup build without that integration.
Backend selection is configuration over compiled integration, not runtime plugin
discovery or permission to omit directly linked libraries.

## Standard application path

```cpp
#include <cheryl/application/startup.h>

CE::Application::ApplicationOptions options;
options.width = 960;
options.height = 540;
options.title = "My game";
auto startup = CE::Application::make_startup("My game", options);
auto result = startup.initialize(argc, argv);
if (!result.should_start())
    return result.exit_code;

MyGame game(*result.context);
auto runtime = result.make_runtime(game);
runtime.run();
```

`MyGame` is the application's `AbstractGame` implementation. The maintained
[tutorial game](../../../apps/tutorial-game/src/main.cpp) supplies the complete
main function and application/resource setup. Standard startup owns backend
argument registration, validation and default context creation; the application
does not write ordinary GLFW/OpenGL integration callbacks.

`ApplicationOptions` contains initial window/presentation defaults, execution
capacity and runtime options. `BackendSelection::Default` chooses the configured
standard backend; `GLFWOpenGL` explicitly selects that supported integration.
The OpenGL composition accepts `--backend default` or `--backend opengl`.
Command-line options override initial defaults. Runtime options include selected
sequential/concurrent execution, input polling and simulation timing policy.

## Application options and supplied services

Register application-specific options before initialization with
`add_application_options()`. Its `CLI::App&` callback runs immediately to register
parser definitions; variables bound by those definitions must survive parsing.
Do not pass destroyed local storage into the parser.

The overload `make_startup(input, description, options)` borrows an existing
input system. `ApplicationOptions::execution.shared_pool` supplies existing
worker capacity. Borrowed input survives context cleanup/destruction and detaches
from its live window. An injected pool's unrelated groups remain independent.
Applications needing another composition can provide owned `StartupBackend`
callbacks to `Startup` directly or use lower-level context/module APIs.

## Lifecycle and execution

Startup is single-use, noncopyable and nonmovable. Construct/register/initialize
on the platform owner. Help or validation failure returns an exit code with no
context; `should_start()` distinguishes that result. Other backend failures
propagate. Successful initialization constructs context/configuration; the
runtime's `run()` subsequently owns adapter/game initialization and shutdown.

Keep Startup alive through the returned context and game, or retain an outer
`Logging::ProcessLifetime`. `StartupResult::make_runtime()` borrows the result's
context and the game; both survive its blocking session. The result cannot be a
temporary used to manufacture a dangling runtime. Partial game initialization
and accepted-work cleanup follow the
[runtime lifecycle](../../../../docs/runtime/runtime-frame-boundary.md).

## Compatibility and further reading

Old startup header paths forward to the canonical module header.
`Cheryl::OpenGL::Startup` remains a compatibility interface when OpenGL startup
is configured; it does not create a second OpenGL-startup binary. The concrete
`CE::Application::GLFWOpenGL::make_startup()` factory delegates to the generic
path. New applications should use `Cheryl::Startup` and the generic factory.

Read [source/SDK consumption](../../../../docs/development/consuming-engine.md),
[module selection](../../../../docs/modules/index.md),
[Project setup](../../../../docs/getting-started/project-setup.md) and
[Engine Systems: Runtime](../../../../docs/engine-systems/runtime.md).
