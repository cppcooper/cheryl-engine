# Cheryl Engine documentation

Choose an entry point for the work you are doing:

| Task | Start here |
| --- | --- |
| Look up a declaration | [C++ API](api/index.md) |
| Write a game | [Project setup](getting-started/project-setup.md), then [First Game](getting-started/first-game.md) |
| Choose a system or public type | [Engine Systems](engine-systems/index.md) |
| Perform an operation | [Detailed Guides](guides/index.md) |
| Select a concrete integration | [Modules](modules/index.md) |
| Connect, extend or replace components | [Architecture](architecture/index.md) |
| Configure, build or troubleshoot | [Setup](../README.md#setup) and [build reference](development/building.md) |
| Run checks | [Test selection](development/testing.md) and [pending checks and QA](testing-requests.md) |
| Check current capabilities or pick up planned work | [Status inventory](STATUS.md) and [planning catalogue](planning/README.md) |

## API Reference

- [Generated C++ symbols, entry points and migration](api/index.md)

## Getting Started

- [Project setup, CMake, and source/SDK choices](getting-started/project-setup.md)
- [First game: startup, architecture, sprites, and text](getting-started/first-game.md)
- [Create a Player, Enemy or HUD](getting-started/create-a/index.md)
- [How to: focused game-development tasks](getting-started/how-to/index.md)
- [Maintained Player/Enemy/HUD and interactive UI example](../projects/apps/tutorial-game/README.md)

## Engine Systems

[Start with the ownership and execution overview](engine-systems/index.md), then
read a system's purpose, type/operation tables, important enums, relationships,
contracts and lifecycle. Table entries link to the detailed guides and API:

- [Runtime](engine-systems/runtime.md), [Display](engine-systems/display.md) and [Input](engine-systems/input.md)
- [Assets](engine-systems/assets.md), [Resources](engine-systems/resources.md), [Rendering](engine-systems/rendering.md) and [Text](engine-systems/text.md)
- [GUI](engine-systems/gui.md) and [Audio](engine-systems/audio.md)
- [Events](engine-systems/events.md), [Dispatch](engine-systems/dispatch.md) and [Workers](engine-systems/workers.md)
- [Memory](engine-systems/memory.md), [State](engine-systems/state.md), [Logging](engine-systems/logging.md), [Diagnostics](engine-systems/diagnostics.md) and [Utilities](engine-systems/utilities.md)

## Detailed Guides

- [Assets](assets/index.md): loading, manifests, animation and tile selection.
- [Text](text/index.md): font selection, shaping, encoding and legacy compatibility.
- [Rendering and Resources](rendering/index.md): frames, materials, sampling and retirement.
- [Input](input/index.md): actions, records, capture and focus.
- [GUI](gui/index.md): interface construction, publication and cleanup.
- [Audio](audio/index.md): clips, voices, streams and output lifetime.
- [Runtime and Concurrency](runtime/index.md): lifecycle, timing, windows, dispatch, events and workers.
- [Memory, State and Utilities](utilities/index.md): object allocation, state choices and CPU helpers.
- [Diagnostics](diagnostics/index.md): logging, observations and failure reporting.

## Modules

- [Selection and roles](modules/index.md)
- [Source/SDK consumption and startup](development/consuming-engine.md)
- [Adapter contracts required by Engine and optional integrations](architecture/interfaces-adapters.md)
- [Startup](modules/startup.md) and [Workers](modules/workers.md)
- [Native GLFW](modules/native-glfw.md) and [OpenGL](modules/opengl.md)
- [TGUI](modules/tgui.md) and [RmlUi](modules/rmlui.md)
- [miniaudio](modules/miniaudio.md)
- [Linux Debug terminal](modules/debug-terminal-linux.md)

The module pages publish the owner guides; the original owner READMEs remain
available beside their source and in the [repository module index](../projects/modules/README.md).
[Module authoring](development/modules.md) and
[UI adapter authoring](development/ui-adapters.md) belong to Development.

## Architecture

- [Interfaces and Adapters](architecture/interfaces-adapters.md): contracts, implementations and replacement dependencies.
- [Assembling the Engine](architecture/assembling-engine.md): facades, factories and compatible context composition.
- [Ownership and Cleanup](architecture/ownership-cleanup.md): borrowed objects, retained handles, leases and shutdown.
- [Threads and Data Flow](architecture/threads-data-flow.md): execution owners, delivery choices and publication.

## Development

- [Generate documentation](development/documentation.md) and [publish versioned documentation](development/publishing-documentation.md)
- [Pending automated and QA testing requests](testing-requests.md)
- [Build and produce SDKs](development/building.md) and [operate CI and SDK collection](development/ci.md)
- [Test selection](development/testing.md)
- [Contributing](development/contributing.md)
- [C++ code style](development/code-style.md)
- [Architecture validation procedures](development/architecture-validation.md)
- [Logging acceptance](development/logging-acceptance.md)
- [Engine and integration modules](development/modules.md)
- [RmlUi placeholder investigation and contribution guidance](external-work/rmlui-placeholder-issue.md)
- [Writing a UI adapter](development/ui-adapters.md)
- [Neutral UI consumer checks](development/architecture-validation.md#neutral-ui-consumer)
- [Desktop smoke checks](development/native-desktop-checks.md)

## Planning

- [Planning catalogue](planning/README.md): horizons, owning plans and prerequisites.
- [Development roadmap](planning/develop-review-and-development-plan.md): active sequencing.

Keep new documents in the matching topic folder and add them to this index. Use
lowercase, hyphenated filenames, with `README.md` reserved for documentation
indexes. Use relative links between documents and to source files so navigation
works from each document's location.
