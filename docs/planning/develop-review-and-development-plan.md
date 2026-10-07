# Development roadmap

## Purpose

This file is the current development roadmap. It records only information needed to
choose and sequence future work: prerequisites, unresolved work, discovery boundaries
and acceptance still required.

The [planning catalogue](README.md) groups short-, mid- and long-term work. This
roadmap owns short-term Linux native work and mid-term feature prerequisites;
multi-platform acceptance and consumer-selected extensions are deferred to the
[long-term plan](long-term-plan.md).

Implementation history belongs in Git. Keep durable contracts in their subject
documents and retain only coverage limits or blockers that affect future work. Do not
append completed task narratives or routine successful execution reports here.
User-run commands, QA requirements and unavailable acceptance prerequisites are in
the [testing request queue](../testing-requests.md).

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
- Typed channels, delivery lifetime and native resize compatibility:
  [event-delivery.md](../runtime/event-delivery.md) and
  [display-and-window-contract.md](../runtime/display-and-window-contract.md).
  Linux/X11 acceptance is complete; the named/`std::any` API retains its architectural
  role. Serialization/versioned protocols remain separate consumer requirements.
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
Windows, macOS, Wayland and other platform testing are shelved in the
[deferred platform plan](platform-acceptance.md); pending platform coverage does not
gate Linux short-term work.

Unresolved source-facing work is summarized in [todo.md](todo.md). Artwork metadata
that cannot safely be inferred from the assets is tracked in
[asset-manifest-todo.md](asset-manifest-todo.md).

## Remaining roadmap

### Native input lifetime safety

Keep the supported default to one initialized native input owner. Resolve Gainput
HID lifetime and Linux controller integration before enabling the HID path; use the
resulting contract in the
[native module guide](../../projects/modules/platform/native-glfw/README.md#input-lifetime-and-mapping).

- [x] Guard process-wide ownership without disrupting an existing adapter or leaking
  ownership when initialization throws.
- [x] Reject reattachment to a different Windows notification window while retaining
  devices and same-window reattachment.
- [x] Add owner-policy and native integration regressions; document supported lifetime.
- [x] Accept native owner-policy acquisition, failed initialization and window
  compatibility cases on Linux through user-run testing.
- [x] Accept native owner rejection, attachment/destruction and reattachment
  regressions on Linux through user-run native OpenGL acceptance.
- [x] Accept native consumer/header probes on Linux through user-run testing.
- [x] Add opt-in controller diagnostics before changing the backend:
  - [x] Report Gainput initialization, callback device IDs and sampled pad state
    without changing mapping or recording keyboard/text input.
  - [x] Add a demo launch option for TRACE file logging and document its scope.
  - [x] Submit a compiler-command check and short Bluetooth controller capture in
    the testing queue; distinguish callback evidence from HID backend readiness.
- [x] Inspect sequential/concurrent Bluetooth traces and the dependency compiler
  command. Axis-only delivery to pad `2` with the HID runtime compiled out identifies
  the Linux joystick translation as the cause of the original Cross-button failure.
- [ ] Correct the Linux joystick path independently of the unresolved HID path:
  - [x] Use kernel button/axis maps for controllers outside the existing legacy
    dialects, retaining created device IDs and state updates.
  - [x] Clear held state and close the descriptor on disconnect; rebuild mappings
    when the joystick reconnects.
  - [x] Add isolated synthetic-device regressions for button actions, sticks/hats,
    remapping and reconnection without requiring a controller or HID startup.
  - [x] Submit the focused build/regressions and Bluetooth Cross/reconnect QA.
  - [x] Accept Bluetooth DualSense physical controller QA with HID disabled: mapped
    buttons/axes, A press behavior, off-center disconnect/reconnect, startup with
    the controller attached and attachment after startup in both runtime modes.
  - [ ] Accept the build/regressions through
    [TR8](../testing-requests.md#tr8-automated-controller-diagnostic-build).
- [ ] Accept the Gainput HID input foundation described in the
  [submodule handoff](../../extern/gainput/TODO.md#ordered-implementation). That file
  owns the active Linux foundation and remaining backend sequence, followed by
  adaptive-trigger output and its Cheryl consumer contract. Work in the existing
  submodule from Cheryl; retain one native owner and one report source for each controller.
- [ ] Expose Linux HID initialization, enumeration/open, source and report observations
  for lifecycle acceptance. The demo counter cannot establish those stages;
  prerequisites and observations are in [testing requests](../testing-requests.md).

Windows owner-policy, attachment/destruction, consumer/header and notification
acceptance are deferred to the [platform plan](platform-acceptance.md). Broader
native ownership remains a separate consumer requirement.
When HID is compiled in, the selected Gainput `InputManager::Init` discards
`HIDInit`'s return code; successful initialization or a device-free poll cannot
establish HID readiness. Linux acceptance must independently observe HID reports
and lifecycle. Explicit startup-failure reporting requires a dependency contract
change if a consumer needs it.

Linux joystick acceptance uses HID disabled; enabling the HID runtime requires the
compiler-definition and report/state corrections together, with one report source
per controller. The [Gainput handoff](../../extern/gainput/TODO.md) owns controller
identity, source selection, lifecycle corrections and the adaptive-trigger feature
contract. Its [selected contracts](../../extern/gainput/HID.md) settle identity,
source fallback and feedback ownership; the tracker prerequisite has source fixes
and isolated regression coverage pending [TR8](../testing-requests.md#tr8-automated-controller-diagnostic-build).
Runtime activation follows physical association, connection generations and retained
pad-state integration. Choose other optional feature requirements using the
[HID capability scope](../../projects/modules/platform/native-glfw/README.md#hid-capability-and-platform-scope).
Battery exposure and touch/motion acceptance remain separate from the handoff's
initial DualSense input and adaptive-trigger scope.

Cheryl's submodule URL selects the user's personal Gainput fork. New submodule
commits remain local until explicitly authorized publication; update the parent
gitlink and keep pending source acceptance distinct from remote availability.

### U10 — Deterministic tile selection

On the stable metadata contract, define a world-neutral neighbor sampler, edge/
unknown-terrain policy, Wang/bitmask mapping, seeded weighted choice and missing-rule
result. Settle connectivity and animation-phase semantics before publishing a map API;
animation substitution uses simulation-owned time.

- [x] Add stateless tile-animation cell lookup from nonnegative elapsed milliseconds,
  with caller-owned phase, exact frame boundaries, looping and nonlooping behavior,
  invalid-timeline checks and source regressions.
  The [tile timing contract](../assets/asset-values-and-playback.md#tile-playback-and-rules)
  defines current behavior.
- [x] Accept tile-animation timing and neutral header probes in the Linux Engine-only
  assembly through user-run testing.
- [ ] Define the neighbor sampler, connectivity and unknown/edge-terrain policies;
  derive Wang signatures and bitmasks without borrowing a graphics-owned world.
- [ ] Add seeded weighted candidate selection and an explicit missing-rule result;
  resolve animation only after choosing the original target cell.
- [ ] Integrate selection with Tileset and publish resolved cells to CPU submission;
  accept deterministic, boundary and missing-rule cases with authorized execution.

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

### U14 — Optional engine expansion

Deferred engine and module candidates are owned by the
[long-term plan](long-term-plan.md). Steam API/Input and their SDK-free preparation
have no short- or mid-term scheduling commitment; a named consumer must establish
scope and prerequisites before this work enters the development sequence.

## Sequencing and evidence

Future adapter work follows the neutral contract/probe -> selected toolkit
requirements -> consumer-required Engine changes -> independent adapter proof order
in the [adapter-author guide](../development/ui-adapters.md). Linux HID work and U10
may proceed on their stable prerequisites. U11 follows an actual Unicode/layout consumer;
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
