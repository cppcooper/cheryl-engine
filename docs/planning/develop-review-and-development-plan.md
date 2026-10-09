# Development roadmap

The [planning catalogue](README.md) groups work by horizon. Linux/GLFW/X11/OpenGL
is the active native platform; deferred platform coverage does not gate Linux work.
Current architecture and API contracts live in the [subject guides](../README.md).

## Short term

Close acceptance for [Unicode](short-term/unicode-text.md),
[demo assets](short-term/demo-assets.md) and the startup/UI checklist below.
These tasks retain their own progress until
accepted. Their observations can proceed independently with matching builds;
refresh the toolkit-free demo once for Unicode/assets, and reuse the accepted
UI assembly for TR14. The
[testing queue](../testing-requests.md) owns launch commands and prerequisites.
Existing regressions do not close the dedicated-case gaps listed in the plans.

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
- [ ] Accept native startup/field/focus/list observations through
  [TR14](../testing-requests.md#tr14-qa-startup-and-ui-interaction).
- [ ] Obtain a corrected supplied RmlUi target/package and consuming host, then
  accept declaration/rejection behavior independently of Cheryl-owned source.

The [upstream investigation](../external-work/rmlui-placeholder-issue.md) retains
the unmodified reproduction, sanitizer and submission prerequisites. Downstream
demo success does not close that work. Dynamic backend selection remains outside
the current programmatic startup contract and has no selected implementation phase.

### Deferred HID integration

Gainput backend work remains deferred at the `d94c60f` code baseline. The
[submodule handoff](../../extern/gainput/TODO.md#ordered-implementation) owns its
sequence. Enabling HID requires compiler-definition, report identity/retained-state
and source-selection corrections together, preserving one initialized native owner
and one report source per controller.

[TR6](../testing-requests.md#tr6-qa-hid-lifecycle-and-notification-observations)
remains blocked on backend work and an observation harness. It needs initialization,
enumeration/open, source and actual HID report/lifecycle evidence; the demo trace
and successful Gainput `Init` do not provide those observations. The dependency
discards HID initialization's return code, so explicit startup-failure reporting
requires a contract change if selected.

Linux joystick QA uses HID disabled. Windows ownership/notification observations
belong to the [platform plan](long-term/platform-acceptance.md). Broader native
ownership requires a consumer case. Battery and touch/motion exposure remain separate
from the handoff's initial input/adaptive-trigger scope; use the
[native capability limits](../../projects/modules/platform/native-glfw/README.md#hid-capability-and-platform-scope)
when selecting extensions.

## Later work

Use the [horizon catalogue](README.md) to select a follow-on. Color emoji waits for
Unicode acceptance; optimization waits for workload evidence and compatibility/order
decisions. Platform, Steam and optional-module work requires a named consumer and
resolved prerequisites. The unscheduled Debug terminal needs transport/platform
decisions before implementation.
