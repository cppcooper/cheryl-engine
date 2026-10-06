# Develop review and development plan

## Purpose

This file is the current development roadmap. It records only information needed to
choose and sequence future work: prerequisites, unresolved work, discovery boundaries
and acceptance still required.

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
- Architecture coverage and environment limits:
  [architecture-validation.md](../development/architecture-validation.md).
- Deprecated legacy FFont behavior: [legacy-ffont.md](../resources/legacy-ffont.md).

<a id="u5-executable-gate-and-u6-implementation-boundaries"></a>

U5's logging gate covers the pre-extraction normal/sandbox compile-profile matrix,
isolated queue/fault/reentry/teardown scenarios and supplementary Debug ASan/UBSan
checks. It does not establish all interleavings or arbitrary callback termination;
TSan needs a separate configuration without the ASan/UBSan combination. The original
gate record, finding classification and U0–U8 implementation rationale are recoverable
with `git show f03d2f8:docs/planning/develop-review-and-development-plan.md`.

<a id="u8-resolve-resource-extension-requirements-before-consumers"></a>

U8 selects the library-neutral probe's one provider/window, immutable image
replacement, expanded triangles and ASCII atlas. Mutable updates, multiple domains,
atomic batch reload and automatic residency are outside that scope; reopen the
[resource contract](../resources/consumer-resource-contract.md) before adding them.

The current selected architecture has one neutral `Cheryl::Engine`, optional Native
GLFW and whole OpenGL modules, owner-local tests, and cross-project aggregate tests.
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

## Active work

### U9 — Prove generic UI-facing facilities with a real adapter

Prerequisites are the current failure, logging, consumption, resource and module
contracts. Use [the U9 checklist](cheryl-ui-integration-plan.md#remaining-development-sequence) for
subtask progress, neutral probe scope and adapter acceptance.

The neutral recording probe establishes the baseline; TGUI 1.13.0 is selected under
the [adapter requirements](tgui-adapter-requirements.md). Its optional owner,
session, neutral bridges and selected demo panel are source-complete, with
controlled module acceptance established. Complete downstream composition and
native widget/runtime/lifetime acceptance next. Add pointer/modal/controller
routing or platform services only for an explicit consumer need with a
capability/failure contract.

The neutral probe uses one provider/window, immutable image replacement and the
existing committed-text/ASCII presentation baseline. A selected toolkit's font/atlas
needs must be checked separately; the engine's STBFont baseline does not define a
toolkit's capabilities. Multiple domains, image mutation, complex effects, Unicode
layout or IME reopen their resource/render/platform contracts before dependent work.

**Acceptance:** the adapter's public/runtime architecture remains backend-neutral;
physical input is collected once and routed without destructive mutation; UI
resources obey the established retained-frame lifetime. A single adapter cannot
establish multi-library extensibility; that claim needs an independent adapter proof.

**Discovery boundary:** any required direct backend/native access is an engine
contract gap. Resolve the smallest generic seam before dependent adapter code grows.

## Remaining roadmap

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

3D, topology/NUMA adapters, audio, networking, world/entity/physics, serialization,
additional render backends, device-loss recovery and broader OS/device validation stay
optional until a named application consumer establishes scope and prerequisites.
Placeholders do not imply supported facilities. The remaining optional input-provider
composition and Steam session/adapter gates are in
[module-groundwork-and-extraction-plan.md](module-groundwork-and-extraction-plan.md).

### U15 — Documentation and roadmap reconciliation

Complete the still-open public-contract inventory alongside each implementation unit:

- [FileMgr](../../projects/engine/include/cheryl/core/resources/fileio/file-mgr.h):
  incremental indexing, missing roots/errors, ordering and borrowed lookup
  lifetime; correct stale loader-discovery comments. Decide refresh support only if
  a consumer needs it.
- [System-font discovery/default selection](../../projects/engine/include/cheryl/core/resources/fileio/fonts-system.h):
  skipped-root/error behavior, preference/enumeration rules and supported
  collection-face selection in FontMgr.
- Display/window declarations: platform affinity, monitor snapshot freshness, scale
  changes and borrowed window lifetime, incorporating U9 capability decisions.
- Remaining exported asset, submission, parameter, camera, input, dispatch and utility
  declarations: missing ownership, valid thread, preconditions, failure/publication
  guarantees and units. Correct stale examples without uniform boilerplate or cosmetic
  reformatting.
- Reconcile stale U5/U8 status/evidence referrals when affecting their subject documents.
  Preserve useful procedures, current contracts and coverage limits without relocating
  routine reports.

**Acceptance:** callers can determine supported behavior from public declarations and
focused links. If documentation cannot state a guarantee, classify the missing
contract and resolve it at the appropriate development boundary instead of inventing
one. Reconcile unresolved source TODOs with remaining work; a plan does not close them.
When a unit completes, update its subject documentation and `todo.md`, then remove
superseded alternatives and resolved planning detail.

## Sequencing and evidence

The dependent order is neutral U9 probe -> selected adapter requirements ->
consumer-required engine changes -> adapter proof. U10 and U13 may proceed on their
stable prerequisites. U11 follows an actual Unicode/layout consumer;
U12 follows measurements and settled render semantics. U14 remains consumer-driven
and U15 accompanies each unit.

Repository execution and commit rules are defined in `AGENTS.md`. Build, compilation,
tests and remote pushes require their specified authorization. A checked planning
item or committed implementation cannot establish executable acceptance. Retain only
unresolved failures, prerequisites, environment/coverage limits or enduring constraints
that affect future work alongside active checklists. Retain checked subtasks until
their macro task completes, then remove that task's section and checklist while
preserving durable contracts and useful procedures. Do not append routine successful-
run reports.
