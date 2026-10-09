# Cheryl Engine documentation

Choose an entry point for the work you are doing:

| Task | Start here |
| --- | --- |
| Check implemented, partial, planned or deferred capabilities | [Status inventory and review baseline](STATUS.md) |
| Understand the engine | [Runtime architecture](runtime/runtime-architecture.md) and [module roles](../projects/modules/README.md) |
| Write a game | [Application composition and game hooks](development/consuming-engine.md), then [demo](../projects/apps/demo/README.md) |
| Configure, build or troubleshoot | [Setup](../README.md#setup) and [build reference](development/building.md) |
| Run checks | [Test selection](development/testing.md) and [pending checks and QA](testing-requests.md) |
| Extend the repository | [Module authoring](development/modules.md), [UI adapters](development/ui-adapters.md) and [contributing](development/contributing.md) |
| Pick up planned work | [Planning catalogue](planning/README.md) |

The system references below describe API contracts, ownership, units and failure
behavior. Read the relevant subject rather than the whole documentation tree.

## Runtime

- [Runtime architecture and backend boundaries](runtime/runtime-architecture.md)
- [Frame ownership and runtime lifecycle](runtime/runtime-frame-boundary.md)
- [Display and window ownership](runtime/display-and-window-contract.md)
- [Native callbacks and failure reporting](runtime/failure-reporting.md)
- [Input actions, state, events, text, and focus](runtime/input-state-model.md)
- [Audio ownership and playback](runtime/audio.md)
- [Simulation timing and recovery](runtime/simulation-timing.md)
- [State, timing and math utilities](runtime/utility-contracts.md)
- [Thread dispatch](runtime/thread-dispatch.md)
- [Event buses and persistent registration](runtime/event-delivery.md)
- [Worker pools and groups](runtime/worker-execution.md)
- [Logging configuration and emission](runtime/logging.md)
- [Subsystem diagnostics](runtime/subsystem-diagnostics.md)

## Assets

- [Asset package downloads and image placement](assets/catalog.md)
- [Asset loading](assets/asset-loading.md)
- [Asset values, playback and CPU submission](assets/asset-values-and-playback.md)
- [Text encoding and source indices](assets/text-encoding.md)
- [Unicode text layout and font resources](assets/text-layout.md)
- [File indexing and font discovery](assets/file-and-font-discovery.md)
- [Asset manifest format](assets/asset-manifests.md)

## Rendering

- [Pipelines, materials, and render submission](rendering/pipelines-and-materials.md)

## Resources

- [Cache ownership, immutable publication and native retirement](resources/resource-residency.md)
- [CPU memory lifetime and reservation](resources/resource-lifetime.md)
- [FFont deprecation and font-file migration](resources/legacy-ffont.md)

## Development

- [Pending automated and QA testing requests](testing-requests.md)
- [Build reference](development/building.md)
- [Test selection](development/testing.md)
- [Contributing](development/contributing.md)
- [C++ code style](development/code-style.md)
- [Architecture validation procedures](development/architecture-validation.md)
- [Logging acceptance](development/logging-acceptance.md)
- [Consuming the engine and command-line startup](development/consuming-engine.md)
- [Engine and integration modules](development/modules.md)
- [Module index and owner guides](../projects/modules/README.md)
- [TGUI adapter and session](../projects/modules/ui/tgui/README.md)
- [RmlUi adapter and native documents](../projects/modules/ui/rmlui/README.md)
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
