# Development roadmap

The [planning catalogue](README.md) groups work by horizon. Linux/GLFW/X11/OpenGL
is the active native platform; deferred platform coverage does not gate Linux work.
Current architecture and API contracts live in the [subject guides](../README.md).

## Nearest planned work

This roadmap owns priority order; the catalogue and status inventory link here.
These priorities do not authorize implementation, builds or work-mode transitions.
The selected order is terminal acceptance, then NUMA/NUCA, then render batching.

| Order | Work | Next phase and checkpoint |
| --- | --- | --- |
| 1 | [Linux Debug terminal](../../projects/modules/platform/debug-terminal-linux/README.md) | Source implementation and controlled probes exist. Execute the configuration matrix and real-desktop QA in the [testing queue](../testing-requests.md#tr17-automated-linux-terminal-configuration-matrix) when authorized; an included implementation must preserve runner reporting and viewer lifetime. |
| 2 | [NUMA and NUCA integration](#numa-and-nuca) | Resolve hardware/workload value and scope, then settle an optional compile-time facility and a simple workload configuration interface before implementation. |
| 3 | [Render batching](#render-batching) | Discuss the design with the project owner, using representative measurements and settled sampling/retention contracts to select compatibility and ordering rules. |

Sampling/TGUI integration and indexed shader/material loading are source-complete.
Their contracts live in [rendering](../rendering/pipelines-and-materials.md),
[TGUI](../../projects/modules/ui/tgui/README.md#recording-and-publication) and
[graphics manifests](../assets/asset-manifests.md). Only main text/image materials
migrated; both UI adapters retain manual builders. Executable/native acceptance of
these changes remains in the testing queue. The completed implementation plans are
retired rather than retained as development journals.

### NUMA and NUCA

The [worker contract](../runtime/worker-execution.md) supports explicit Linux CPU
affinity. Required cache-domain/NUMA requests reject and preferred requests fall
back; automatic topology discovery and memory placement are absent.

First resolve benefit on the intended consumer hardware/workloads. If the useful
scope is limited to a future server build, defer NUMA indefinitely rather than
requiring a server architecture now; no client/server split is scheduled. This is
an unresolved scope checkpoint, not an already adopted blanket deferral. Evaluate
cache-locality/NUCA needs separately. Any selected facility must be compilable out
through CMake without changing ordinary worker submission or completion semantics.

Design workload-oriented defaults and configuration that a caller can use without
knowing CPU/node/cache IDs. Keep explicit low-level policy available where needed,
show requested/effective placement and fallback clearly, and preserve futures,
closure and drainage. Settle eligible CPUs, required/preferred failures,
capability reporting and unsupported-platform behavior before dependent code grows.

Distinguish CPU-to-node/cache mapping and worker-group placement from memory
allocation, first-touch or migration. Affinity alone cannot guarantee cache residency
or memory locality. Define NUCA in terms of discoverable locality and supported
controls. Discovery, CPU placement and any selected memory integration are separate
coherent implementation units. Single-node runs do not establish multi-node behavior;
identify suitable hardware and deterministic policy/error coverage before acceptance.

### Render batching

This task requires a design discussion before implementation. Collect representative
update, draw/state-switch, publication and backlog metrics to identify useful work.
Include UI clipping, glyph pages, sampling, retained resource generations,
parameters/image units, geometry ranges/topology and pipeline state in compatibility.
Start with compatible contiguous packets, preserving authored order and in-flight
resource lifetime; comparative measurements and visual/order/generation checks must
justify the change. Report a different dominant bottleneck at the checkpoint rather
than assuming batching is the next implementation. [Reorder-safe sorting](mid-term/README.md#reorder-safe-render-sorting)
remains a separate later task.

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
