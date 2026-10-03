# Cheryl Engine documentation

Start with [runtime architecture and backend boundaries](runtime/runtime-architecture.md)
for an overview of the engine and application APIs. The documents below cover
individual systems, development practices, and unfinished work.

## Runtime

- [Runtime architecture and backend boundaries](runtime/runtime-architecture.md)
- [Frame ownership and runtime lifecycle](runtime/runtime-frame-boundary.md)
- [Input actions, state, events, text, and focus](runtime/input-state-model.md)
- [Simulation timing and recovery](runtime/simulation-timing.md)
- [Thread dispatch](runtime/thread-dispatch.md)
- [Event buses and persistent registration](runtime/event-delivery.md)
- [Worker pools and groups](runtime/worker-execution.md)

## Assets

- [Asset loading](assets/asset-loading.md)
- [Asset manifest format](assets/asset-manifests.md)

## Rendering

- [Pipelines, materials, and render submission](rendering/pipelines-and-materials.md)
- [Asset and renderer header boundaries](rendering/asset-render-boundaries.md)

## Resources

- [Resource residency and maintenance](resources/resource-residency.md)
- [Resource lifetime and reservation](resources/resource-lifetime.md)

## Development

- [C++ code style](development/code-style.md)
- [Recorded validation and repeatable commands](development/architecture-validation.md)
- [Desktop smoke checks](development/native-desktop-checks.md)

## Planning

- [Unfinished engine work](planning/todo.md)
- [Unresolved asset-manifest metadata](planning/asset-manifest-todo.md)

Keep new documents in the matching topic folder and add them to this index. Use
lowercase, hyphenated filenames, with `README.md` reserved for documentation
indexes. Use relative links between documents and to source files so navigation
works from each document's location.
