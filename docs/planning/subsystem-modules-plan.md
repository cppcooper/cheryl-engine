# Module boundaries and initial setup

Revised 2026-10-04 after owner review. Keep OpenGL-dependent code together in
one module. Native GLFW and the whole OpenGL backend now have optional targets
under `projects/modules/`; standalone composition and owner-local tests are implemented.
Executable isolation/conformance acceptance remains pending authorization.
The ordered work is in [the groundwork and extraction plan](module-groundwork-and-extraction-plan.md).

## What earns a module

Keep one shared engine library. Add a module where an application gains a useful
choice: an optional facility, an alternative implementation, or an external
dependency it can leave out. Each selected module gets its own CMakeLists.

A directory or an internal interface does not by itself justify another library.
Internals, logging, memory, workers, events and general utilities stay in the engine.
Separate them later only if an actual consumer or measured build problem warrants it.

## The useful candidates

| Candidate | What separation buys us | Existing boundary |
| --- | --- | --- |
| OpenGL backend | Select or omit the whole graphics implementation and its OpenGL/GLAD requirements; test backend behavior separately. | Rendering/resource interfaces connect it to the engine; [iOpenGLContext](../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/context.h) supports different context implementations within the module. |
| Native GLFW integration | Omit the coupled window/display and native input implementation, including GLFW/Gainput/native platform requirements. | Existing neutral display/window/input interfaces; keep the concrete implementations together because InputSystem requires Window. |
| UI adapter | Enable a toolkit only in applications that use it; test its translation into engine facilities separately. | Engine input, retained drawing and resource contracts; U9 adds capabilities the chosen toolkit actually needs. |
| Steam integration | Games without Steam avoid its SDK and session requirements; policy tests can use a fake SDK driver. | New application-owned integration, with Steam Input as one responsibility inside it. |
| Input provider | Choose one provider by default; optionally combine specialized controller support with native keyboard/mouse input. | [iInputSystem](../../projects/engine/include/cheryl/core/controls/input-interface.h); combining sources still needs an explicit coordinator. |

These are candidates, not a required target list. Steam services and Steam Input
can initially share one module. Further modules need the same concrete justification.

Composition is optional for a specific benefit, such as native keyboard/mouse with
Steam-managed controllers. SDK-specific collection belongs to the module owning
that SDK; selected sources feed one engine input coordinator and one published
snapshot. Assign each controller one collection path to avoid duplicate input.
Prove both ordinary single-provider use and the chosen composition with fakes
before adding real SDK transport.

## One OpenGL module

Put all OpenGL-dependent implementation and headers together: renderer, resources,
shaders, textures, OpenGL diagnostics, context contract, GLFW binding and factories.
There is no demonstrated need for separate context or bridge targets.

The context interface is still useful inside that module. It lets another window
integration supply context operations without rewriting the renderer. That seam
can support multiple implementations within one library. Splitting any OpenGL piece
later requires an explicit benefit and justification.

Generic rendering/presentation/resource contracts stay in the engine. Put GLFW
window/display and Gainput input together in their native module. OpenGL depends
on that module and the engine; neither integration is an engine dependency. Perform
both ownership changes together after preparation to avoid an intermediate cycle.
Preserve window/context/resource lifetimes and explicitly settle compatibility for
consumers that currently receive the backend through Cheryl::Engine.

## Target-centric build structure

Every project-owned target gets an owning directory, including the shared engine,
executables, tests, and existing object/interface build-support targets. Each owner
defines its CMake target, sources, headers and dependencies. Aliases share their
underlying target's home; upstream dependencies retain their upstream layout.
Internal engine folders remain organization within one library.

The agreed common parent is `projects/`. Current target homes and later module homes:

```text
cheryl-engine/
  CMakeLists.txt                # Select and compose targets
  projects/
    engine/
      CMakeLists.txt
      include/                 # Public headers, preserving include spellings
      src/
      tests/
        all-tests/
        logging-tests/
        logging-acceptance/
        consumer/
        include/               # Owned test helpers
        fixtures/
        support/               # Shared test runner entry point
      support/
        signal-handlers/
        logging-config/
    modules/                   # Owners grouped by engine role
      README.md                # Engine contract index
      platform/
        native-glfw/           # Display/window/input owner
          CMakeLists.txt
          README.md            # Concrete implementation-to-contract map
          include/
          src/
          tests/
      graphics/
        opengl/                # Whole rendering/presentation/resource owner
          CMakeLists.txt
          README.md
          include/
          src/
          tests/
    apps/demo/
    tools/backward-cpp/
  cmake/                       # Shared helpers
  docs/
  extern/
```

Tests live with the library or module they exercise. Engine tests, logging checks
and engine consumer/header probes belong under `engine/tests/`; OpenGL tests belong
under `modules/graphics/opengl/tests/`. Each test target can own a child directory there.
The `all-tests` runner can combine suites across owners without making the engine
library depend on its modules.

Each owner publishes its public include roots with `target_include_directories`
using paths relative to its own CMake directory. Consumers obtain those paths by
linking the target with `target_link_libraries`, rather than naming another owner's
folders. A dependency is PUBLIC when exported headers require it and PRIVATE when
only implementation uses it. Preserve current include spellings during migration,
including the engine's `include/cheryl` root; keep private source paths private.

The initial directory migration preserved the combined archive. The subsequent
coordinated extraction moves native/OpenGL implementations and their tests to the
selected owners. Granular include spellings remain; consumers explicitly select
module links. See [the implemented composition contract](../development/modules.md).

Begin in this repository. A module's standalone CMake entry point reuses supplied
engine targets or accepts an explicit engine checkout path. It must not assume a
particular parent directory, compile copies of engine sources, or add the engine
twice. This leaves a later companion repository possible without requiring one now.

New module dependencies are discovered only when enabled. Standalone bootstrap
suppresses the engine's module catalog locally to prevent recursion. Applications
or the default assembly select the engine and modules; the shared engine must not
link back to a module that depends on it. These composition rules are implemented;
their executable acceptance is recorded separately from the directory migration.

## Testing and the next work

Smaller test executables can link the existing engine and compile only the relevant
cases. That reduces test translation units without creating a library for every
internal facility; the existing cheryl-logging-tests already uses this pattern.
Module tests then link their actual implementation and fake only
its external boundary. Keep integration tests for startup, threads and shutdown.
Classify tests by the guarantee they prove: engine contract, specific implementation,
or composed integration. Separation can require splitting/rewriting tests and fixtures,
not just moving files. Engine runtime contract checks use real engine code with
controlled dependencies; native implementation checks exercise the selected module.
Keep the engine's default unit suite cheap with small dummy contract implementations.
Each implementing module owns checks of its real behavior and contract compliance;
dummy-backed engine checks are not substitutes for those. Costly acceptance stays
with its implementation owner and is selected separately from the cheap unit suite.
Removing native dependency discovery requires a real extraction; folder changes
and runtime test filters cannot provide it.

1. **Groundwork/layout:** map current targets to owning directories, settle shared
   engine/default-build compatibility, then establish the target-centric layout,
   optional-module selection, standalone convention and one template in coherent
   units. Include the core engine's own directory, colocated tests and public target
   usage requirements; do not move everything at once.
2. **Native/OpenGL extraction:** establish the two cohesive owners in one cutover,
   including the whole OpenGL backend, with consumer/backend checks and an explicit
   default-build compatibility decision. Prove engine-only SDK isolation separately.
3. **UI/Steam integrations:** implement actual consumer requirements. Keep generic
   U9 repairs in the engine. Prototype Steam session policy and optional input
   composition with fakes before SDK transport; callbacks must progress when polling pauses.
4. **Further extraction:** require a demonstrated replacement, dependency or testing
   benefit. Another graphics API must prove that the engine's contracts fit it.

Steam callback/session ownership follows the
[Steamworks API contract](https://partner.steamgames.com/doc/sdk/api); concrete
SDK/version/application choices belong to its implementation unit.

This revision replaces the broad target-family and M0–M10 roadmap. The earlier
research remains in Git history; it is not a prerequisite list for the skeleton.
The original architecture review used source and document checks. Directory-migration
acceptance is recorded separately; it does not prove the later module isolation.
