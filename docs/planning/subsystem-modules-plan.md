# Module boundaries and initial setup

Revised 2026-10-04 after owner review. Keep OpenGL-dependent code together in
one module. This is a proposal; the skeleton and extractions are not implemented.

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
| OpenGL backend | Select or omit the whole graphics implementation and its OpenGL/GLAD requirements; test backend behavior separately. | Rendering/resource interfaces connect it to the engine; [iOpenGLContext](../../include/cheryl/backends/opengl/context.h) supports different context implementations within the module. |
| UI adapter | Enable a toolkit only in applications that use it; test its translation into engine facilities separately. | Engine input, retained drawing and resource contracts; U9 adds capabilities the chosen toolkit actually needs. |
| Steam integration | Games without Steam avoid its SDK and session requirements; policy tests can use a fake SDK driver. | New application-owned integration, with Steam Input as one responsibility inside it. |
| Input provider | Choose native or Steam-supported devices while retaining the engine's action/snapshot model. | [iInputSystem](../../include/cheryl/core/controls/input-interface.h); combining sources still needs an explicit coordinator. |

These are candidates, not a required target list. Steam services and Steam Input
can initially share one module. Further modules need the same concrete justification.

## One OpenGL module

Put all OpenGL-dependent implementation and headers together: renderer, resources,
shaders, textures, OpenGL diagnostics, context contract, GLFW binding and factories.
There is no demonstrated need for separate context or bridge targets.

The context interface is still useful inside that module. It lets another window
integration supply context operations without rewriting the renderer. That seam
can support multiple implementations within one library. Splitting any OpenGL piece
later requires an explicit benefit and justification.

Generic rendering/presentation/resource contracts stay in the engine. GLFW-only
window/display support and Gainput input can stay there initially too; moving them
is a separate decision. The OpenGL module then depends on that shared engine, with
no reverse dependency. This still retains GLFW/Gainput dependencies. Preserve
window/context/resource lifetimes and explicitly settle compatibility for consumers
that currently receive the backend through Cheryl::Engine.

## Small build structure

```text
cheryl-engine/
  CMakeLists.txt
  include/ and src/             # Shared engine, existing paths
  modules/
    CMakeLists.txt              # Select optional modules
    <actual-module>/
      CMakeLists.txt
      include/ and src/
      tests/
```

Begin in this repository. A module's standalone CMake entry point reuses supplied
engine targets or accepts an explicit engine checkout path. It must not assume a
particular parent directory, compile copies of engine sources, or add the engine
twice. This leaves a later companion repository possible without requiring one now.

New module dependencies are discovered only when enabled. Standalone bootstrap
suppresses the engine's module catalog locally to prevent recursion. Applications
or the default assembly select the engine and modules; the shared engine must not
link back to a module that depends on it. The skeleton keeps today's build intact.

## Testing and the next work

Smaller test executables can link the existing engine and compile only the relevant
cases. That reduces test translation units without creating a library for every
internal facility; the existing cheryl-logging-tests already uses this pattern.
Module tests then link their actual implementation and fake only
its external boundary. Keep integration tests for startup, threads and shutdown.
Removing native dependency discovery requires a real extraction; folder changes
and runtime test filters cannot provide it.

1. **Skeleton:** add optional-module selection, a reusable standalone convention
   and one template. Establish the pattern without creating all candidate modules.
2. **OpenGL module:** extract the whole backend together, with consumer/backend
   checks and an explicit default-build compatibility decision. Do not claim a
   native-free engine while GLFW/Gainput remain in it.
3. **UI/Steam integrations:** implement actual consumer requirements. Keep generic
   U9 repairs in the engine. Prototype Steam session policy and combined input with
   fakes before SDK transport; callbacks must progress even when input polling pauses.
4. **Further extraction:** require a demonstrated replacement, dependency or testing
   benefit. Another graphics API must prove that the engine's contracts fit it.

Steam callback/session ownership follows the
[Steamworks API contract](https://partner.steamgames.com/doc/sdk/api); concrete
SDK/version/application choices belong to its implementation unit.

This revision replaces the broad target-family and M0–M10 roadmap. The earlier
research remains in Git history; it is not a prerequisite list for the skeleton.
Validation here is source and document review, with no builds or executable tests.
