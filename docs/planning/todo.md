# Unfinished engine work

This is the concise list of unresolved engine facilities. Completed implementation
history belongs in Git and durable contracts belong in their subject documents.
Current roadmap sequencing is in
[develop-review-and-development-plan.md](develop-review-and-development-plan.md).
Unresolved artwork metadata is tracked separately in
[asset-manifest-todo.md](asset-manifest-todo.md).

## Validation and integration

- Coordinate Gainput's process-global HID initialization/shutdown and native
  notification ownership before supporting simultaneous initialized native input
  adapters or reattachment to a different Windows window. `Init`/`Exit` share HID
  state, and Windows notifications retain the first native handle.
- Complete the remaining public-contract documentation, especially FileMgr indexing/
  borrowed lookups, system-font discovery/selection and display/window lifetime/
  capability semantics. See [U15](develop-review-and-development-plan.md#u15--documentation-and-roadmap-reconciliation).

## Gameplay and presentation facilities

- Add a tile-map selection layer: derive neighbor signatures/masks, choose weighted
  candidates deterministically from an explicit seed, and apply animated-target
  substitution using simulation-owned time. Current Tileset APIs expose metadata but
  do not select world neighbors.
- Add Unicode text layout/glyph runs before treating committed Unicode input as fully
  renderable text. Define decoding, fallback, shaping/bidi/line-breaking scope and
  retained atlas ownership before choosing dependencies. IME/editing is a separate
  consumer requirement unless selected UI work needs it.
- Add broader pointer capture, modal/controller routing or optional platform services
  only for an explicit consumer contract. Current adapters cover rectangular clipping,
  colored geometry and routed committed text; their
  [capability boundaries](../development/ui-adapters.md#routing-and-unavailable-services)
  define what additional requirements must establish before exposure.

## Optional and measured extensions

- Derive complete render compatibility keys and explicit reorder-safe regions before
  batching or sorting. Authored packet order remains the baseline.
- Add a profiling-based timing adviser only from measured workload data; it must not
  silently replace explicit fixed-step, input-retention or recovery policy.
- Add typed event channels while preserving existing registration/delivery/lifetime
  ownership and the current named/`std::any` interface contract.
- Treat 3D, topology/NUMA adapters, audio, networking, world/entity/physics,
  serialization, additional graphics backends and broader platform/device support as
  separate consumer-driven roadmaps.
