# Unfinished engine work

This is the concise list of unresolved engine facilities. Completed implementation
history belongs in Git and durable contracts belong in their subject documents.
Current roadmap sequencing is in
[develop-review-and-development-plan.md](develop-review-and-development-plan.md).
Unresolved artwork metadata is tracked separately in
[asset-manifest-todo.md](asset-manifest-todo.md).

## Validation and integration

- Accept Linux native consumer checks, Windows native input ownership
  and attachment/destruction, and physical HID/notification behavior. Outstanding
  prerequisites and execution are tracked in the
  [native lifetime task](develop-review-and-development-plan.md#native-input-lifetime-safety).
- Complete desktop resize QA and Windows native resize/neutral typed-event acceptance
  through existing registration/delivery/lifetime ownership.
  The named/`std::any` contract remains; outstanding execution is in the
  [typed-event task](develop-review-and-development-plan.md#u13--typed-events).

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
- Treat 3D, topology/NUMA adapters, audio, networking, world/entity/physics,
  serialization, additional graphics backends and broader platform/device support as
  separate consumer-driven work in the [long-term plan](long-term-plan.md), alongside
  deferred Steam API/Input and optional input composition.
