# Development roadmap

The [planning catalogue](README.md) groups work by horizon. Linux/GLFW/X11/OpenGL
is the active native platform; deferred platform coverage does not gate Linux work.
Current architecture and API contracts live in the [subject guides](../README.md).

## Nearest planned work

This roadmap owns priority order; the catalogue and status inventory link here.
Detailed requirements stay in the owning plan. These priorities do not authorize
implementation or work-mode transitions.

| Order | Work | Next phase and checkpoint |
| --- | --- | --- |
| 1 | [Texture sampling backend and TGUI integration](#texture-sampling-and-tgui) | Investigate the existing backend/provider and recording paths, then settle sampling ownership and immutable scene behavior before changing APIs. |
| 2 | [NUMA and NUCA integration](#numa-and-nuca) | Investigate native topology and hardware capabilities, then approve CPU, memory and cache-locality scope before implementation. |
| 3 | [Shader manifest schema and indexed material loading](short-term/indexed-material-loading.md) | Resolve document identity/dispatch, recipe contents, backend construction and demo loading/reload at the existing design checkpoint. |
| 4 | [Render batching](#render-batching) | Measure representative workloads and settle compatibility after the sampling contract; preserve authored order. |
| 5 | [External Debug terminal](short-term/debug-console.md) | Select the first platform, viewer transport and process/output ownership. This work is independent of rendering. |

The first two entries are the next integration workstreams. The remaining entries
are scheduled follow-ons with unresolved design checkpoints. Shader/material
manifests are one coordinated design unit until that checkpoint determines whether
separate document formats are needed. Batching depends on final sampling semantics;
NUMA/NUCA and the Debug terminal have no shader-manifest dependency.

### Texture sampling and TGUI

The OpenGL texture implementation has filtering controls, but the neutral provider
creates RGBA images with fixed smoothed/mipmap policy. TGUI rejects nearest texture
and font sampling. Current behavior is described in the
[TGUI guide](../../projects/modules/ui/tgui/README.md#recording-and-publication) and
[material contract](../rendering/pipelines-and-materials.md).

At the design checkpoint, settle nearest/linear minification and magnification,
mipmap and wrap behavior, image versus binding ownership, supported defaults and
unsupported-backend behavior. Define how toolkit smoothing changes are copied into
recordings so prior recordings and uploaded scenes retain their original appearance.
Include font atlases, shared images and atlas-edge behavior; shared cached textures
must not be mutated during frame preparation.

Implement the neutral contract and OpenGL backend as one coherent unit, then TGUI
texture/font recording and scene-upload integration as the next. Preserve platform
upload ownership, immutable resource generations and failure-safe scene adoption.
Acceptance must cover sampling changes, retained scenes, shared-image isolation,
font atlases and native nearest/linear appearance without silently dropping settings.

### NUMA and NUCA

The [worker contract](../runtime/worker-execution.md) currently supports explicit
Linux CPU affinity. Required cache-domain/NUMA requests reject and preferred requests
fall back; automatic discovery and memory placement are absent.

Investigate topology discovery dependencies and the available native controls first.
At the design checkpoint, distinguish CPU-to-node/cache mapping, worker-group CPU
placement and NUMA memory allocation/first-touch or migration policy. Define NUCA
support in terms of discoverable cache locality and hardware-supported controls;
CPU affinity alone does not guarantee cache residency or memory locality. Settle
eligible-CPU restrictions, requested/effective policies, required/preferred failures,
capability reporting, ownership and unsupported-platform behavior.

Keep topology discovery, worker-group placement and any selected memory-locality
integration as coherent implementation units. Preserve the neutral scheduler and
unconstrained worker behavior. Acceptance needs deterministic policy/error coverage
and native observations on suitable hardware. A single-node or unavailable-hardware
run cannot establish multi-node placement or NUCA behavior; identify those prerequisites
before scheduling execution.

### Render batching

Collect representative update, draw/state-switch, publication and backlog metrics.
Settle UI clipping, glyph-page and sampling semantics before caching compatibility
keys. Include retained resource generations, parameters/image units and sampling,
geometry ranges/topology, clip and pipeline state. Batch compatible contiguous
packets first, preserving authored order and in-flight resource lifetime.
Comparative measurements and visual/order/generation checks must justify the change;
if another bottleneck dominates, report that evidence at the design checkpoint.
[Reorder-safe sorting](mid-term/README.md#reorder-safe-render-sorting) remains a
separate later task.

## Work-mode progression

Finish each workstream's authorized phase before advancing: investigation and
research, architecture and design, test design, implementation/refactoring/tooling,
test implementation, test execution, then verification. Complete and commit coherent
implementation units independently. Keep documentation maintenance separate and
recommend synchronization when contracts stabilize; testing-request maintenance
also requires authorization.

User-directed progression remains the default under [AGENTS.md](../../AGENTS.md).
A bounded plan-directed sequence may be approved, with mandatory checkpoints for
unresolved design decisions, changed scope or protected unfinished user work.
Builds and test execution still require explicit authorization. Source completion
and executable/native acceptance remain distinct.

## Existing regression and composition follow-up

The remaining [Unicode font-selection regressions](short-term/unicode-text.md)
and startup/UI checklist below are independent testing/composition objectives.
They are not blanket pending QA for the accepted Linux baseline. Select them in
their own work modes rather than mixing them into integration implementation.
The [testing queue](../testing-requests.md) owns ready pending acceptance;
the [demo guide](../../projects/apps/demo/README.md#repeating-native-qa) preserves
reusable native procedures and coverage limits. Existing regressions do not close
the dedicated-case gaps listed in the plans.

### Startup and UI follow-up

The [application guide](../development/consuming-engine.md#command-line-startup)
owns startup/result lifetimes and option semantics; the UI guides own
[batch routing](../development/ui-adapters.md#routing-and-unavailable-services) and
[RmlUi correction declarations](../../projects/modules/ui/rmlui/README.md#placeholder-dependency-contract).
Font-weight acceptance remains with the Unicode plan rather than this checklist.

- [x] Add common/backend startup support and migrate the demo's application options.
- [x] Preserve session entry epochs across complete input batches with per-record controls.
- [x] Correct RmlUi field focus, placeholder markup and vertical list rows; apply the
  downstream source guard and require correction declarations for supplied Core.
- [x] Compile Engine-only/native UI selections and accept existing font/layout/resource
  and session/coexistence checks with Cheryl-owned dependencies.
- [ ] Add dedicated Startup parser/factory and support-header probes, callback/batch
  routing cases and placeholder hit-testing regressions in separately authorized
  testing phases. Existing suites do not exercise these new paths directly.
- [x] Accept Linux native startup/field/focus/list observations in the selected
  root composition; [demo QA](../../projects/apps/demo/README.md#startup-and-ui-validation)
  retains the procedure and coverage limits.
- [ ] Obtain a corrected supplied RmlUi target/package and consuming host, then
  accept declaration/rejection behavior independently of Cheryl-owned source.

The [upstream investigation](../external-work/rmlui-placeholder-issue.md) retains
the unmodified reproduction, sanitizer and submission prerequisites. Downstream
demo success does not close that work. Dynamic backend selection remains outside
the current programmatic startup contract and has no selected implementation phase.

## Deferred integrations

Multiple active rendering windows and HID integration belong to the
[long-term native plan](long-term/README.md#deferred-native-integration).
The RmlUi document/font/input/clipping and retained-upload integration is sufficient
for the selected Linux root composition. Further effects and OS services are
[deferred UI extensions](long-term/README.md#ui-adapters); the open regression and
supplied-Core cases above remain distinct from extending that core integration.

### Deferred HID integration

The [long-term HID section](long-term/README.md#deferred-hid-integration) owns the
backend deferral, Gainput handoff and observation prerequisites.
[TR6](long-term/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications)
is deferred there, outside the active testing queue.

## Later work

Use the [horizon catalogue](README.md) for work beyond the nearest sequence.
Color emoji remains unselected; text/timing optimization and reorder-safe sorting
wait for workload evidence and their own contracts. Platform, Steam and additional
optional modules require a named consumer and resolved prerequisites.
