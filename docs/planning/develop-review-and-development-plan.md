# Development roadmap

The [planning catalogue](README.md) groups work by horizon. Linux/GLFW/X11/OpenGL
is the active native platform; deferred platform coverage does not gate Linux work.
Current architecture and API contracts live in the [subject guides](../README.md).

## Nearest planned work

This roadmap owns the nearest selected work; the catalogue and status inventory
link here. Priorities do not authorize implementation, builds or mode transitions.
The selected cache-locality → positioned-text → CI/SDK source work is complete.
Its contracts live in the [worker guide](../runtime/worker-execution.md),
[text guide](../assets/text-layout.md#positioned-text-runs) and
[build/CI guide](../development/building.md#github-ci-and-sdk-artifacts).

Remaining acceptance has independent prerequisites:

| Work | Next phase and checkpoint |
| --- | --- |
| Hosted CI and SDK artifacts | Reconcile the manifest-count fixture after the assets update, then run the exact Ubuntu/Windows jobs and verify the first successful main-push collection gate in [TR20](../testing-requests.md#tr20-automated-hosted-ci-and-sdk-artifacts). Local Linux checks do not establish hosted Windows support. |
| Linux Debug terminal | Execute the configuration matrix [TR17](../testing-requests.md#tr17-automated-linux-terminal-configuration-matrix), then real-desktop [TR18](../testing-requests.md#tr18-qa-linux-debug-terminal-on-a-real-desktop). Included builds must preserve runner reporting and viewer lifetime. |
| Engine-only graph | Complete the independent Release compilation/link inspection in [TR15](../testing-requests.md#tr15-automated-engine-only-composition-graph); accepted behavioral tests need no blanket rerun. |
| Demo material/artwork QA | Complete shader-pair failure/recovery and available artwork observations in [TR19](../testing-requests.md#tr19-qa-indexed-demo-materials-and-recovered-graphics). Missing images/showcase fixtures remain explicit blockers. |

Sampling/TGUI and indexed shader/material loading have selected Linux CPU/backend
and native pixel acceptance. Only main text/image materials migrated; both UI
adapters retain [manual builders](../rendering/pipelines-and-materials.md#manual-material-construction).
The reusable [graphics procedure](../development/architecture-validation.md#sampling-and-positioned-text)
preserves coverage limits. Completed implementation plans are retired after their
contracts and unresolved acceptance have moved to the subject guides/testing queue.

NUMA discovery and memory placement remain [indefinitely deferred](long-term/README.md#other-engine-extensions).
General packet batching and reorder-safe sorting require separate selection under
the [mid-term scope](mid-term/README.md#general-render-batching).
[Release publication](long-term/README.md#other-engine-extensions)
remains a future decision: current SDK outputs are workflow artifacts and contain
prebuilt development libraries/headers, not applications or a demo package.

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
- [x] Compile independent Engine/OpenGL Startup first-include probes through the
  installed SDK consumer assembly.
- [ ] Add dedicated Startup parser/factory, callback/batch routing and placeholder
  hit-testing regressions in separately authorized
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
