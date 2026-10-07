# Development roadmap

## Purpose

This file is the current development roadmap. It records only information needed to
choose and sequence future work: prerequisites, unresolved work, discovery boundaries
and acceptance still required.

The [planning catalogue](README.md) groups short-, mid- and long-term work. This
roadmap owns short-term native acceptance and mid-term feature prerequisites;
consumer-selected extensions are deferred to the [long-term plan](long-term-plan.md).

Implementation history belongs in Git. Keep durable contracts in their subject
documents and retain only coverage limits or blockers that affect future work. Do not
append completed task narratives or routine successful execution reports here.

## Current baseline

The original review's U1–U8 source work is implemented. Use the current contracts
rather than the earlier implementation journal:

- Runtime ownership, callback and failure behavior:
  [runtime-architecture.md](../runtime/runtime-architecture.md) and
  [failure-reporting.md](../runtime/failure-reporting.md).
- Retained memory bookkeeping and transaction ownership:
  [resource-lifetime.md](../resources/resource-lifetime.md). Singleton initialization,
  publication and borrowed lifetime are specified in
  [singleton.h](../../projects/engine/include/cheryl/templates/singleton.h).

- Logging configuration, ownership and failure containment:
  [logging.md](../runtime/logging.md) and
  [logging-acceptance.md](../development/logging-acceptance.md).
- Diagnostic domains and unlocked observation boundaries:
  [subsystem-diagnostics.md](../runtime/subsystem-diagnostics.md).
- Build-tree consumption and public target behavior:
  [consuming-engine.md](../development/consuming-engine.md).

- Resource publication and consumer ownership:
  [consumer-resource-contract.md](../resources/consumer-resource-contract.md).
- Module ownership and selected integration targets:
  [modules.md](../development/modules.md).
- Independent TGUI/RmlUi adapters, retained runtime and coexistence boundaries:
  [ui-adapters.md](../development/ui-adapters.md). Their initial proof is accepted;
  the [demo guide](../../projects/apps/demo/README.md#interaction-checks) retains
  the user-reported native acceptance scope and platform limits.
- Architecture coverage and environment limits:
  [architecture-validation.md](../development/architecture-validation.md).
- Deprecated legacy FFont behavior: [legacy-ffont.md](../resources/legacy-ffont.md).

The current selected architecture has one neutral `Cheryl::Engine`, optional Native
GLFW, whole OpenGL and independent TGUI/RmlUi modules, owner-local tests, and
cross-project aggregate tests.
Reuse the [isolation and composition procedures](../development/architecture-validation.md)
when changing those boundaries. Installed/imported package support remains separate
packaging work.

The first native acceptance platform remains Linux with GLFW/X11 and OpenGL; normal
and sandbox engine configurations remain in scope. The advertised OpenGL 3.3 baseline,
physical GPU/compositor behavior, full Wayland and other operating systems require
their own acceptance. A software-driver run cannot establish those capabilities.

Unresolved source-facing work is summarized in [todo.md](todo.md). Artwork metadata
that cannot safely be inferred from the assets is tracked in
[asset-manifest-todo.md](asset-manifest-todo.md).

## Remaining roadmap

### Native input lifetime safety

Keep the supported default to one initialized native input owner. Resolve conflicting
Gainput HID lifetime and Windows notification ownership before broader native
adapter support; use the resulting contract in the
[native module guide](../../projects/modules/platform/native-glfw/README.md#input-lifetime-and-mapping).

- [x] Guard process-wide ownership without disrupting an existing adapter or leaking
  ownership when initialization throws.
- [x] Reject reattachment to a different Windows notification window while retaining
  devices and same-window reattachment.
- [x] Add owner-policy and native integration regressions; document supported lifetime.
- [ ] Accept the owner-policy checks and native attachment/destruction cases on Linux
  and Windows. Source inspection does not establish HID/device-notification behavior.

Execution requires `input_lifetime.*` in the native module and
`native_opengl.input_owner`, `input_window` and `input_reattach` in the existing native
acceptance suite. Select its real-display opt-in and include HID-enabled configurations
before claiming the process-global backend or Windows notifications are accepted.
The selected Gainput `InputManager::Init` discards `HIDInit`'s return code; successful
initialization or a device-free poll cannot establish HID readiness. Native acceptance
must independently observe controller reports and device notifications. Explicit
startup-failure reporting requires a dependency contract change if a consumer needs it.

### U10 — Deterministic tile selection

On the stable metadata contract, define a world-neutral neighbor sampler, edge/
unknown-terrain policy, Wang/bitmask mapping, seeded weighted choice and missing-rule
result. Settle connectivity and animation-phase semantics before publishing a map API;
animation substitution uses simulation-owned time.

**Acceptance:** boundary/missing-rule cases and identical samples/seed/time resolve
deterministically without graphics-thread world access. Missing artwork semantics
remain blocked on [asset-manifest-todo.md](asset-manifest-todo.md); sheet dimensions
cannot supply them. This work has no UI or batching prerequisite.

### U11 — Unicode text layout and glyph resources

Define decoding/error replacement, scalar versus cluster indices, fallback fonts,
shaping/bidi/line-breaking scope and layout-result ownership before choosing libraries.
Define glyph IDs/metrics/runs, atlas growth or replacement, retained generations and
platform upload dispatch against the resource contract. Settle any editing/IME effects
on cluster indices or platform services before publishing the layout API.

**Acceptance:** invalid UTF-8, multilingual/fallback and cluster cases have defined
results; multi-byte input is not rendered as a fallback per byte; glyph runs survive
in-flight rendering and atlas changes. Preserve ASCII compatibility. IME/preedit and
grapheme-aware editing remain separate consumer contracts.

### U12 — Measured optimization facilities

Collect representative update/draw/state-switch, publication and backlog measurements
using the existing diagnostic metrics. Settle relevant UI clipping and glyph-atlas
semantics before caching compatibility keys across frames. Keys must include retained
resource generations, parameters/image units, geometry ranges/topology and pipeline
state. Batch compatible contiguous packets first; sorting requires explicit
reorder-safe regions and authored order remains the default.

**Acceptance:** visual/order/material-generation checks preserve authored semantics
and comparative measurements justify the optimization. Defer batching if profiles
identify another bottleneck. A timing advisor must explain measured suggestions and
leave fixed-step, recovery and input policy under application control. Renderer and
timing changes are independent development units.

### U13 — Typed events

Define channel/type identity, payload ownership and mismatch policy, then add typed
registration/submission with compile-time payload constraints. Prove a representative
engine/native channel through existing registration, invalidation, waits, worker FIFO
and platform/simulation delivery, including queued error-handler ownership.

**Acceptance:** unchecked payload casts cannot reach typed callbacks and cancellation/
shutdown behavior is preserved. Retain the named/`std::any` interface according to its
architectural role. Serialization/versioned protocol concerns are separate.

### U14 — Optional engine expansion

Deferred engine and module candidates are owned by the
[long-term plan](long-term-plan.md). Steam API/Input and their SDK-free preparation
have no short- or mid-term scheduling commitment; a named consumer must establish
scope and prerequisites before this work enters the development sequence.

## Sequencing and evidence

Future adapter work follows the neutral contract/probe -> selected toolkit
requirements -> consumer-required Engine changes -> independent adapter proof order
in the [adapter-author guide](../development/ui-adapters.md). U10 and U13 may proceed
on their stable prerequisites. U11 follows an actual Unicode/layout consumer;
U12 follows measurements and settled render semantics. U14 remains consumer-driven.

Public declarations and focused subject documents define ownership, valid threads,
preconditions, units and failure/publication guarantees alongside each development
unit. If a guarantee is unresolved, classify it at the appropriate development
boundary instead of inventing it. Reconcile source TODOs with remaining work; a plan
does not close them. Update the affected subject documents and `todo.md` before
removing completed planning detail.

Repository execution and commit rules are defined in `AGENTS.md`. Build, compilation,
tests and remote pushes require their specified authorization. A checked planning
item or committed implementation cannot establish executable acceptance. Retain only
unresolved failures, prerequisites, environment/coverage limits or enduring constraints
that affect future work alongside active checklists. Retain checked subtasks until
their macro task completes, then remove that task's section and checklist while
preserving durable contracts and useful procedures. Do not append routine successful-
run reports.
