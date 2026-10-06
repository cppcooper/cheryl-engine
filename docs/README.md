# Cheryl Engine documentation

Start with [runtime architecture and backend boundaries](runtime/runtime-architecture.md)
for an overview of the engine and application APIs. The documents below cover
individual systems, development practices, and unfinished work.

For [setup and build options](../README.md#setup) and
[grouped targets/output filenames](../README.md#targets), start with the root README.
For a running application and controls, use the [demo guide](../projects/apps/demo/README.md).

## Contents

- [Runtime](#runtime)
- [Assets](#assets)
- [Rendering](#rendering)
- [Resources](#resources)
- [Development](#development)
- [Planning](#planning)

## Runtime

- [Runtime architecture and backend boundaries](runtime/runtime-architecture.md)
- [Frame ownership and runtime lifecycle](runtime/runtime-frame-boundary.md)
- [Native callbacks and failure reporting](runtime/failure-reporting.md)
- [Input actions, state, events, text, and focus](runtime/input-state-model.md)
- [Simulation timing and recovery](runtime/simulation-timing.md)
- [Thread dispatch](runtime/thread-dispatch.md)
- [Event buses and persistent registration](runtime/event-delivery.md)
- [Worker pools and groups](runtime/worker-execution.md)
- [Logging configuration and emission](runtime/logging.md)
- [Subsystem diagnostics](runtime/subsystem-diagnostics.md)

## Assets

- [Asset loading](assets/asset-loading.md)
- [Asset manifest format](assets/asset-manifests.md)

## Rendering

- [Pipelines, materials, and render submission](rendering/pipelines-and-materials.md)
- [Asset and renderer header boundaries](rendering/asset-render-boundaries.md)

## Resources

- [Resource residency and maintenance](resources/resource-residency.md)
- [Consumer resource contract](resources/consumer-resource-contract.md)
- [Resource lifetime and reservation](resources/resource-lifetime.md)
- [FFont deprecation and font-file migration](resources/legacy-ffont.md)

## Development

- [C++ code style](development/code-style.md)
- [Recorded validation and repeatable commands](development/architecture-validation.md)
- [Logging acceptance](development/logging-acceptance.md)
- [Consuming the engine](development/consuming-engine.md)
- [Engine and integration modules](development/modules.md)
- [Module index and owner guides](../projects/modules/README.md)
- [TGUI adapter and session](../projects/modules/ui/tgui/README.md)
- [Writing a UI adapter](development/ui-adapters.md)
- [Neutral UI probe](development/ui-probe.md)
- [Desktop smoke checks](development/native-desktop-checks.md)

## Planning

- [Develop review and development plan](planning/develop-review-and-development-plan.md)
- [UI integration checklist](planning/cheryl-ui-integration-plan.md)
- [TGUI requirements and decisions](planning/tgui-adapter-requirements.md)
- [RmlUi requirements and decisions](planning/rmlui-adapter-requirements.md)
- [Module boundaries and initial setup](planning/subsystem-modules-plan.md)
- [Groundwork and module extraction plan](planning/module-groundwork-and-extraction-plan.md)
- [Unfinished engine work](planning/todo.md)
- [Unresolved asset-manifest metadata](planning/asset-manifest-todo.md)

Keep new documents in the matching topic folder and add them to this index. Use
lowercase, hyphenated filenames, with `README.md` reserved for documentation
indexes. Use relative links between documents and to source files so navigation
works from each document's location.
