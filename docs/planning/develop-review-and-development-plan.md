# Development roadmap

The [planning catalogue](README.md) groups work by horizon. Linux/GLFW/X11/OpenGL
is the active native platform; deferred platform coverage does not gate Linux work.
Current architecture and API contracts live in the [subject guides](../README.md).

## Nearest planned work

This roadmap owns priority order; the catalogue and status inventory link here.
These priorities do not authorize implementation, builds or work-mode transitions.
Terminal source work is complete with acceptance pending. The selected engineering
order is worker cache locality, then render batching; build/release automation is
the intended follow-up. NUMA is deferred indefinitely.

| Order | Work | Next phase and checkpoint |
| --- | --- | --- |
| 1 | [Linux Debug terminal](../../projects/modules/platform/debug-terminal-linux/README.md) | Source implementation and controlled probes exist. Execute the configuration matrix and real-desktop QA in the [testing queue](../testing-requests.md#tr17-automated-linux-terminal-configuration-matrix) when authorized; an included implementation must preserve runner reporting and viewer lifetime. |
| 2 | [Worker cache locality](short-term/worker-cache-locality.md) | Design choices are settled: general best-effort discovery/placement, optional Engine support targets, hwloc, and both C++ and JSON configuration. Implement the owning plan; no advance speedup demonstration on the current hardware is required. |
| 3 | [Render batching](#render-batching) | Discuss the design with the project owner, using representative measurements and settled sampling/retention contracts to select compatibility and ordering rules. |
| 4 | [Build/release automation](#build-and-release-automation) | After cache locality and batching, plan develop-branch build/test workflows and main-branch versioned package preparation. Settle supported platforms and package contents before implementation. |

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

NUMA discovery and memory placement are deferred indefinitely by the project owner;
no client/server split is scheduled. The [long-term scope](long-term/README.md#other-engine-extensions)
retains the conditions for reopening it.

General best-effort cache/core locality is selected independently of a measured
speedup on the current machine. The
[worker cache-locality plan](short-term/worker-cache-locality.md) owns the settled
design, source handoff, ordered implementation checklist and future acceptance.
It covers reported L1/L2/L3 and other cache levels, optional hwloc support under
`projects/engine/support/`, simple automatic/reuse/spread policies, and both typed
C++ and JSON settings. The application identifies related work and owns its data;
the facility does not promise cache residency or internal bank control.

Implementation is the next phase when requested. Preserve existing submission,
fairness, completion and shutdown contracts; use runtime capabilities rather than
assuming every CPU/OS exposes the same topology or binding controls. Cache-locality
performance measurements and render-batching design remain independent objectives.

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

### Build and release automation

The project owner's intended follow-up after cache locality and batching is GitHub
Actions: `develop` builds and tests, while `main` prepares versioned release packages.
No workflow exists in the inspected source, and this is a future workstream rather
than authorization to configure CI, publish artifacts or run builds/tests now.

Settle the OS/compiler/architecture matrix, package contents, version source and
release trigger before implementation. Start from the accepted Linux composition;
SDK portability alone does not establish Cheryl support on another platform.
CI must provision a C++23 standard library with `std::format`, pinned submodules
and the selected system dependencies described in the [build guide](../development/building.md).
Include optional hwloc-enabled and disabled selections without depending on one
runner's CPU/cache layout. Separate deterministic headless checks from real
desktop/device acceptance; skipped native checks do not count as that coverage.

The current consumer boundary is [build-tree composition](../development/consuming-engine.md).
There is no project install/export/CPack pipeline. Decide whether the first package
contains source, applications, SDK libraries or a combination before designing
staging and consumption checks. Binary packages need explicit OS/architecture,
compiler/ABI and configuration identities plus their dependency and license
requirements. Keep runtime topology discovery independent of the machine producing
the package, and do not promote unaccepted platform configurations into releases.

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
