# Development roadmap

## Purpose

This file is the current development roadmap. It records only information needed to
choose and sequence future work: prerequisites, unresolved work, discovery boundaries
and acceptance still required.

The [planning catalogue](README.md) groups short-, mid- and long-term work. This
roadmap links the short-term [audio integration](audio-integration.md) and owns
Unicode text work and Linux acceptance, and mid-term feature
prerequisites;
multi-platform acceptance and consumer-selected extensions are deferred to the
[long-term plan](long-term-plan.md).

Implementation history belongs in Git. Keep durable contracts in their subject
documents and retain only coverage limits or blockers that affect future work. Do not
append completed task narratives or routine successful execution reports here.
User-run commands, QA requirements and unavailable acceptance prerequisites are in
the [testing request queue](../testing-requests.md).

The short-term [demo asset showcase](demo-assets.md) supplies visible static and
animated tile/sprite observations while retaining startup without package images;
its Linux QA reuses the pending native demo build.

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

Keep the supported default to one initialized native input owner. Accept the pending
Linux joystick regressions with HID disabled, using the current contract in the
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

Gainput backend work remains deferred at the detached `d94c60f` pin. The
[submodule handoff](../../extern/gainput/TODO.md#ordered-implementation) owns the
backend sequence, followed by adaptive-trigger output and its Cheryl consumer
contract. When scheduled, work in the existing submodule from Cheryl and retain
one native owner and one report source per controller. Linux HID observations also
remain deferred and blocked on initialization, enumeration/open, source and report
instrumentation; the demo counter cannot establish those stages. The pending
[QA request](../testing-requests.md#tr6-qa-hid-lifecycle-and-notification-observations)
preserves the required observations.

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
contract. Choose other optional feature requirements using the
[HID capability scope](../../projects/modules/platform/native-glfw/README.md#hid-capability-and-platform-scope).
Battery exposure and touch/motion acceptance remain separate from the handoff's
initial DualSense input and adaptive-trigger scope.

Cheryl's submodule URL selects the user's personal Gainput fork. New submodule
commits remain local until explicitly authorized publication; update the parent
gitlink and keep pending source acceptance distinct from remote availability.

### U10 — Deterministic tile selection

Source implementation is complete; Linux executable acceptance remains pending.
Use stable metadata and the
[tile selection contract](../assets/asset-values-and-playback.md#tile-selection).
The consumer owns world coordinates and supplies shared Wang edge/corner labels or
bitmask cell terrain, a stable per-location seed and simulation-owned animation time.
Mixed cell terrain does not imply a Wang labeling rule. Selection has no UI or batching
prerequisite and must complete before publishing a world/map API that depends on it.

- [x] Add stateless tile-animation cell lookup from nonnegative elapsed milliseconds,
  with caller-owned phase, exact frame boundaries, looping and nonlooping behavior,
  invalid-timeline checks and source regressions.
  The [tile timing contract](../assets/asset-values-and-playback.md#tile-playback-and-rules)
  defines current behavior.
- [x] Accept tile-animation timing and neutral header probes in the Linux Engine-only
  assembly through user-run testing.
- [x] Settle sampling sites, connectivity, boundary policies, deterministic seed and
  simulation-owned phase before introducing the selector API.
- [x] Implement CPU-only Wang/bitmask selection, explicit failure results and weighted
  candidates; add focused regressions and a neutral first-include header probe.
  Reject malformed direct rule metadata needed by the selected query, and align
  manifest/schema validation with the four-neighbor cardinal-only contract.
- [x] Integrate selection and one-time animated-target substitution with Tileset;
  validate original and resolved grid cells, and cover passing the resulting cell
  to existing CPU submission without retaining or resampling the world.
- [x] Publish durable selection contracts and reconcile the aggregated Linux Engine
  request after each source unit. Keep artwork metadata prerequisites separate.
- [ ] Accept deterministic, sampling/boundary, invalid-rule, missing-rule, animation
  and CPU submission regressions plus public header probes through user-run
  [TR7](../testing-requests.md#tr7-automated-engine-asset-preparation).

**Acceptance:** boundary/missing-rule cases and identical samples/seed/time resolve
deterministically without graphics-thread world access. Missing artwork semantics
remain blocked on [asset-manifest-todo.md](asset-manifest-todo.md); sheet dimensions
cannot supply them. This work has no UI or batching prerequisite.

### U11 — Unicode text layout and glyph resources

Source implementation is complete; executable and native acceptance remain pending.
The builtin demo uses the separate
[Unicode text service](../assets/text-layout.md), while existing STBFont/FFont
consumers and toolkit layout services retain their own APIs. The legacy Font interface
exposes quad indices and one atlas/geometry pair; the new service retains each complete
message generation to support fallback without constraining that interface.

The initial rendering scope covers English, accented Latin including French/German,
and Russian/Cyrillic. Implement Unicode paragraph direction (automatic or explicitly
LTR/RTL), mixed-direction runs and optional width-constrained wrapping. Callers supply
width in local font pixels; UI code derives that constraint from its own layout/scale.
Color emoji, full CJK acceptance and editing/IME are outside this batch. Additional
scripts use the same pipeline but need suitable fonts and their own acceptance.

Font selection accepts ordered application font files and installed family names,
then optional automatic common-family selection, then an embedded licensed fallback.
Common system families are preferences rather than availability guarantees. The
embedded fallback supplies the initial alphabets and a visible replacement when no
face covers an entire grapheme; it cannot promise every Unicode character.

Complete these coherent units in dependency order:

- [x] Add neutral UTF-8 scalar decoding with owned byte-offset/byte-count records and
  an explicit malformed-input flag. Preserve embedded NUL, BOM and unassigned/noncharacter
  scalars; replace ill-formed input by maximal subpart without swallowing valid successor
  bytes. Keep scalar positions distinct from grapheme and shaping-cluster indices.
- [x] Add decoder source regressions and a first-include probe; aggregate Linux
  Engine-only acceptance with the existing asset request.
- [x] Make STBFont layout and callback traversal consume decoded scalars, preserving
  printable ASCII, newline/CR/tab behavior and callback exception semantics. Emit one
  fallback per unsupported scalar or malformed subpart. Retain FFont's legacy byte
  contract and keep atlas contents unchanged.
- [x] Add ASCII, multilingual fallback, malformed-input and CPU submission regressions;
  document current encoding behavior and reconcile the same acceptance request.
- [x] Settle the initial scripts, direction, fallback guarantee, optional wrapping and
  grayscale rendering scope. Keep source mapping for display separate from caret/IME.
- [x] Add immutable owned font selection, family discovery/coverage inspection and an
  independently bundled/embedded fallback; preserve the legacy Font/FFont APIs. Use
  FreeType for font inspection/rasterization with neutral headers and private linkage.
- [x] Add HarfBuzz shaping and ICU paragraph bidi, grapheme and line boundaries. Define
  owned face-qualified glyphs, byte/scalar cluster ranges and line metrics, optional
  language hints, explicit paragraph direction and local maximum width. Select fallback
  for entire graphemes and reshape accepted lines after breaking.
- [x] Prepare grayscale glyph pages on the CPU and upload complete immutable text
  generations through the existing provider owner. Add retained-run submission alongside
  the legacy API; failure publishes no replacement, and earlier frames retain their
  original geometry/atlas pair. Each preparation admits only its message's glyphs;
  automatic residency/budgets and a shared mutable atlas are outside this unit.
- [x] Integrate the builtin demo through dispatcher uploads, preserving working ASCII
  consumers. Provide multilingual, combining, bidi, fallback, wrapping, upload-failure
  and retained-generation regressions and a runnable Linux visual observation harness.
- [ ] Accept decoder/font selection, multilingual/shaping/bidi/wrapping, resource
  failure/retention and header probes through
  [TR7](../testing-requests.md#tr7-automated-engine-asset-preparation); compile the
  changed demo in the batched
  [TR8](../testing-requests.md#tr8-automated-controller-diagnostic-build), then accept
  Linux sequential/concurrent visual, fallback and replacement observations through
  [TR9](../testing-requests.md#tr9-qa-unicode-text-rendering).

**Discovery boundary:** font selection must establish real coverage and the bundled
fallback before layout depends on it. Keep ICU indices internal and verify owned
source mappings and actual shaped-line widths before resource preparation. Resource uploads
follow the [consumer resource contract](../resources/consumer-resource-contract.md);
layout cannot mutate an atlas retained by a submitted frame. IME/preedit and
grapheme-aware editing remain separate consumer contracts until explicitly selected.

The existing resource boundary determines the safe run model: glyph IDs are qualified
by their font face, and placements retain the matching atlas/geometry generation as
one immutable snapshot. A later upload/repack publishes fresh handles through the
provider owner; old submitted runs keep the old handles. CPU layout must not pair
placements from one generation with subsequently borrowed Font handles. Preserve the
existing Font/FFont APIs and introduce the selected run service alongside them.

[HarfBuzz](https://harfbuzz.github.io/what-is-harfbuzz.html) supplies shaping;
[FreeType](https://freetype.org/freetype2/docs/index.html) supplies font access and
grayscale rasterization. HarfBuzz's
[scope](https://harfbuzz.github.io/what-harfbuzz-doesnt-do.html) requires separate
paragraph bidi and line breaking; use ICU's
[bidi](https://unicode-org.github.io/icu/userguide/transforms/bidi.html) and
[boundary analysis](https://unicode-org.github.io/icu/userguide/boundaryanalysis/)
instead of handwritten Unicode tables. Library-specific types remain private.

**Acceptance:** invalid UTF-8, multilingual/fallback and cluster cases have defined
results; multi-byte input is not rendered as a fallback per byte; glyph runs survive
in-flight rendering and atlas changes. Preserve ASCII compatibility. IME/preedit and
grapheme-aware editing remain separate consumer contracts.

### U11 follow-on — Color emoji

Consider color emoji for the next text batch after the initial Unicode acceptance.
Select the required color-font formats, font sources and supported emoji sequences
before implementation. Establish RGBA page orientation, material/alpha handling and
sequence/fallback policy while retaining source clusters and immutable generations.
Keep the existing grayscale path independently usable. Color glyph and sequence
appearance need their own CPU/native acceptance; none is requested in this batch.

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
Measure repeated shaping of candidate prefixes in constrained paragraphs and
message-specific glyph preparation/upload before introducing reusable glyph caches.
Any faster fitter must preserve actual accepted-line widths, source clusters and bidi.

### U14 — Optional engine expansion

Deferred engine and module candidates are owned by the
[long-term plan](long-term-plan.md). Steam API/Input and their SDK-free preparation
have no short- or mid-term scheduling commitment; a named consumer must establish
scope and prerequisites before this work enters the development sequence.

## Sequencing and evidence

Future adapter work follows the neutral contract/probe -> selected toolkit
requirements -> consumer-required Engine changes -> independent adapter proof order
in the [adapter-author guide](../development/ui-adapters.md). U10 retains Linux
acceptance while U11 begins with the builtin font consumer's UTF-8 foundation.
Gainput backend work remains deferred at its pin;
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
