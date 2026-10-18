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
[build/CI guide](../development/ci.md).

### Selected planned work

The following work is selected for investigation and design. Dependency reviews
remain three independent tasks; each feature needs its own settled scope and
acceptance before implementation. This selection does not promote general packet
batching or the other deferred integrations.

| Work | Owning scope and next checkpoint |
| --- | --- |
| GoogleTest submodule review | [Dependency review](#dependency-reviews): compare upstream history with the current pin, assess test/CMake/toolchain changes and recommend retaining the pin or selecting an update. |
| nlohmann/json submodule review | [Dependency review](#dependency-reviews): assess parsing, serialization, configuration and source/package compatibility against upstream candidates. |
| spdlog submodule review | [Dependency review](#dependency-reviews): assess logger/sink behavior, formatting dependencies, exported targets and source/package/toolchain compatibility. |
| RmlUi rendering effects | [UI extension scope](long-term/README.md#ui-adapters): settle masks, transforms, layers, filters, custom shaders and repeating coordinates, including required neutral-contract and graphics-backend support. |
| Reorder-safe render sorting | [Sorting scope](mid-term/README.md#reorder-safe-render-sorting): identify reorderable regions and measure a policy that preserves authored order where required, clipping and retained generations. |
| Color emoji glyphs | [Emoji scope](mid-term/README.md#color-emoji): select color-font formats and sequences, then settle RGBA pages, materials, fallback and CPU/native acceptance. |
| Alternative audio SDK | [Audio scope](long-term/README.md#other-engine-extensions): select an SDK, assess licensing/deployment and preserve the ordinary playback contract; any richer authoring model needs explicit scope. |
| In-game developer console | [Console scope](long-term/README.md#other-engine-extensions): settle graphical output, commands, scrolling/history and input routing independently of the native terminal. |

The current RmlUi document/font/input/clipping and retained-upload integration
remains sufficient for the selected Linux root composition. Its planned rendering
effects include any required backend work; the
[startup/UI follow-up](#startup-and-ui-follow-up) keeps regression and supplied-Core
acceptance separate from extending that integration.

### Dependency reviews

For each dependency, compare its pinned commit with upstream history and candidate
revisions, including changed ancestry or tag/commit availability. Review relevant
release notes, integration/API changes, CMake targets/options, supported compilers
and license changes. Check both Cheryl-owned source and supplied target/package
paths where applicable. Recommend an exact reproducible candidate or retention of
the existing pin; reviewing history does not automatically authorize an update.

If an update is selected, keep each submodule change a separate development unit
and design focused acceptance for the affected owners, independent consumers and
Linux/Windows CI configurations. Preserve unrelated pins and local dependency work.

### Remaining acceptance

Remaining acceptance has independent prerequisites:

| Work | Next phase and checkpoint |
| --- | --- |
| Main-push SDK collection | Hosted Ubuntu/Windows builds and relocated SDK consumers are accepted. Verify the first successful main-push collection gate in [TR20](../testing-requests.md#tr20-automated-main-push-sdk-collection). PR/develop/manual runs intentionally skip collection. |
| Hosted Windows skipped cases | Supply a CFF font fixture and a temporary directory on the checkout drive, then accept the affected font-allocation and logging cases on hosted Windows. The [CI guide](../development/ci.md#hosted-windows-coverage-limits) retains their scope and prerequisites. |
| Linux Debug terminal | Complete real-desktop [TR18](../testing-requests.md#tr18-qa-linux-debug-terminal-on-a-real-desktop) using matching binaries from the accepted configuration matrix. Observe actual emulator windows and desktop/IDE application launch. |
| Demo material/artwork QA | Complete shader-pair failure/recovery and available artwork observations in [TR19](../testing-requests.md#tr19-qa-indexed-demo-materials-and-recovered-graphics). Missing images/showcase fixtures remain explicit blockers. |

Sampling/TGUI and indexed shader/material loading have selected Linux CPU/backend
and native pixel acceptance. Main text/image materials use indexed recipes;
UI adapters use [provider-owned defaults](../rendering/pipelines-and-materials.md#default-ui-materials)
and accept optional custom material handles.
The reusable [graphics procedure](../development/architecture-validation.md#sampling-and-positioned-text)
preserves coverage limits. Completed implementation plans are retired after their
contracts and unresolved acceptance have moved to the subject guides/testing queue.

NUMA discovery and memory placement remain [indefinitely deferred](long-term/README.md#other-engine-extensions).
General packet batching remains deferred under the
[mid-term scope](mid-term/README.md#general-render-batching). Reorder-safe sorting
is selected separately; positioned text runs do not implement either policy.
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
Toolkit OS services remain [deferred UI extensions](long-term/README.md#ui-adapters).

### Deferred HID integration

The [long-term HID section](long-term/README.md#deferred-hid-integration) owns the
backend deferral, Gainput handoff and observation prerequisites.
[TR6](long-term/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications)
is deferred there, outside the active testing queue.

## Later work

Use the [horizon catalogue](README.md) for work beyond the nearest sequence.
Color emoji and reorder-safe sorting are selected above, with their design and
workload prerequisites retained. General batching and text/timing optimization
remain deferred. RmlUi effects, alternative audio and the graphical console are
also selected; platform, Steam and other optional modules retain their existing
consumer and prerequisite requirements. Deferred phases in the
[status inventory](../STATUS.md#3-deferred) describe review order within that section.
