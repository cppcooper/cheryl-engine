(console)=
# Developer console and bounded scripts

Register typed commands once, then call the same handlers from C++, text, a
graphical console, or a bounded test file. The core, graphical model, and script
runtime have no LivingWorld, Family, or Magic dependency. Contact remains an
optional demo adapter, not the host model applications must implement.

## Select the surface

Link `Cheryl::Console` once for the complete console: typed commands, settings,
history, help, completion, graphical presentation, script jobs, and reports.
The normal SDK includes it by default (`CHERYL_BUILD_CONSOLE=ON`), independently
of the demo and `CHERYL_BUILD_TESTS`. It is one compiled library, shared by default
under `CHERYL_BUILD_SHARED_LIBS`, with public Engine and RmlUi dependencies supplied
transitively. An explicitly selected static Cheryl build uses the same target.

```cmake
find_package(Cheryl 4 CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE Cheryl::Console)
```

| Public entry point under `cheryl/tools/console/` | Responsibility in `Cheryl::Console` |
| --- | --- |
| `registry.h`, `session.h`, `parser.h`, `scenario.h`, `setting.h` | Typed authority, retention, text binding, strict plans, and owner-backed settings. |
| `model.h` | History, completion, help, and retained output. |
| `view.h` | Real RmlUi document and listeners over Model. |
| `runtime/runner.h`, `runtime/command.h`, `runtime/diagnostics.h` | Owned isolated jobs, physical I/O/reporting, and safe Engine observations. |

Existing caches retain explicit choices: enable `CHERYL_BUILD_CONSOLE=ON` and
`CHERYL_BUILD_UI_RMLUI=ON` if either was cached OFF. The former runtime/view
split switches and targets no longer select separate products. No cache deletion
or FORCE is needed. Console requires RmlUi; intentionally omitting RmlUi also
requires `CHERYL_BUILD_CONSOLE=OFF`. No graphical-versus-headless Console variant
is distributed. Only the optional Contact demo requires its domain fixtures and
test selection. The SDK installs the whole Console header tree and advertises
one `CHERYL_HAS_CONSOLE` feature and a `console` component version.
Use the [SDK guide](../development/consuming-engine.md) for producer, runtime-library,
and relocation requirements; do not substitute private source include roots.

## Own the host and its authority

An application owns its Authority, Registry, registrations, and Sessions. Registry,
Context, Session, Model, and View operations are serialized on that host's owner
thread. A Registry gate is pure and non-reentrant: it recognizes actual issued
contexts rather than accepting every context that happens to have enough rights.
Keep gameplay mutation and diagnostic Query channels separate. A script's
`observe` route cannot turn a Setting or Action into a Query.

Register with `Registry::add<Args...>` and retain its move-only Registration.
The descriptor declares arguments, result schema, rights, kind, and timing.
The callback prepares owned Work; effects occur only in `Work::step()` after
Session retention. Binding does not mutate the host. Close/cancel Work without
waiting, retain result data, and do not fabricate rollback of completed effects.
Reset registrations before destroying borrowed host state. Retained Work must
own or otherwise safely retain every object it will still access.

Typed `Registry::bind` and `bind_text` produce BoundRequests for the same
registration. Submit with a nonzero ID, then pump the owner Session. Retrying
the exact admitted request under that ID returns its retained operation; a
different body conflicts. A new ID is a new operation, not a retry.

### A meaningful setting

`UnsignedSetting` registers `<name>.read` (Query) and `<name>.set` (Setting).
It calls the real owner; there is no mutable settings mirror or polling worker.
For example, an application's substeps-per-tick limit can use:

```cpp
namespace Console = CE::Tools::Console;
Console::UnsignedSetting limit(registry, "simulation.step_limit",
    "Substeps per owner tick; zero pauses simulation", 4, 16,
    {[simulation] { return simulation->step_limit; },
     [simulation](std::uint64_t value) { return simulation->set_limit(value); }},
    read_right, write_right);
```

Here `simulation` is a retained application owner whose actual update loop uses
that limit. `set_limit(false)` must preserve its value. Range checking precedes
admission; callbacks execute during owner progression. Discovery exposes an
unsigned argument with maximum 16 and `OwnerImmediate` timing. Results expose
`default`, `effective`, `minimum` (zero), `maximum`, and `timing`. The declared
default describes the owner's policy; it does not initialize the owner. Queries
read its actual current value. Exceptions fail the operation without promising
rollback. Counts are unitless unsigned schemas; describe their meaning in help.

The independent SDK example owns a simulation with default 4, changes it to 6
through typed and text calls, advances exactly 6 substeps, then sets zero and
demonstrates that advancement pauses. Contact's actual Magic price setting
continues through its existing owner/handlers, not this example's state.

## Graphical composition

Implement `Presentation::Connection` to retain Registry, issued Context, and its
Session, supply a scope label, and close admission without blocking. Construct
`Presentation::Model` with a shared Connection. Construct `Presentation::View`
with a borrowed RmlUi Session and Model on their common owner thread. Destroy
View first, then Model/Connection, and finally the UI Session and host owners.
Model close preserves copied typed results. Sharing one Connection between
Models also shares its Session and closure; prefer a dedicated Connection.

The default View document is host-neutral. Custom documents supply a native text
input `console-command`, output `console-output`, suggestions `console-suggestions`,
scope `console-scope`, and buttons `console-submit`, `console-previous`,
`console-next`, `console-complete`, `console-help`, `console-latest`, and
`console-close`. Optional prepared-command pairs identify additional button IDs
and their text lines. All commands still use Model binding. Host labels and
retained output are escaped before insertion as RML; do not inject raw results.

The real demo now wraps these production Model/View classes with a Contact
Connection, its existing custom document, and optional Magic buttons. Its
same-batch focus handoff, keyboard quarantine, WASD controls, and TGUI coexistence
remain application-owned. The reusable View does not silently steal global input.

## Run an isolated test file

Implement `Runtime::Fixture` with its own Registry and fresh gameplay/diagnostic
Sessions matching their issued Contexts. `Config::factory(name, asset)` recognizes
supported fixture identities and constructs isolated state. Job owns Fixture
before its borrowing strict Run and destroys Run first. The factory executes
on the Job step caller, never on the I/O worker. Do not mutate shared game state
from the factory. A null/refusing factory fails honestly.

Set host-approved absolute `script_root` and `output_root`, script relative name,
fresh single-component `run_id`, nonempty `build_id`, factory, and shared IoService.
Optional `asset` is a relative name read from `asset_root`; empty means no asset
I/O. No S1 asset or Contact fixture is required. The host-only `report_fields()`
extension is a trusted JSON member fragment; escape strings with `Runtime::quote`,
and never emit duplicate/reserved report keys or interpolate untrusted text.

Version 1 uses the existing strict parser, not eval or a general scripting language:

```text
script.version 1
fixture.load independent-v1
simulation.step_limit.set 6 -> setting
expect $setting.effective == 6
retry $setting -> retained
expect $retained.effective == 6
simulation.advance -> tick
expect $tick == 6
finish
```

`observe`, expected `refuse`/`error`, named results, field assertions, and bounded
`await` use the existing Scenario contract. The entire file is validated before
commands progress. Unknown commands, malformed input, out-of-range arguments,
wrong channel use, missing finish, and zero assertions cannot pass. Bounds remain
64 KiB input, 256 lines, 128 statements, 32 names, 256 steps, and a ten-second
default wall deadline. No loops, shell, or nested `test.run` are supported.

`Runtime::Commands` optionally registers `test.run script run` with host-selected
run rights (default bit 4 for Contact compatibility). A successful command result
means the job returned a disposition: inspect `disposition` and `durable`, not
only the command's Stage. Batch callers use `Job::batch_exit()`: zero additionally
requires a durable Passed job and every planned assertion executed successfully.

### Physical I/O and honest reports

Create a dedicated external Engine Pool and Group, then IoService. One accepted
lane callback owns file operations and cleanup. Interactive Job/Commands/View
retirement never waits for physical I/O. Close admission, retire jobs and command
registrations, call `IoService::shutdown()`, and call `settle()` from the external
shutdown owner before shutting down/destroying the Pool. Never settle from the
I/O lane or an interactive destructor. A stalled OS call can stall settlement.

POSIX descriptor-relative no-follow reads/writes require existing approved roots
without symlink components. Each run creates a new directory; do not reuse an
existing run ID. Windows uses handle-checked absolute drive paths, retaining
ancestor handles without write/delete sharing until cleanup. It rejects UNC and
device roots, reparse traversal, DOS device names, trailing dots, alternate data
streams, multiply linked inputs, and concurrent input writers. Reads remain
bounded and UTF-8 checked; outputs use exclusive fresh files. Other platforms
explicitly refuse report I/O. Roots are host-approved, not a security sandbox
against a hostile process renaming their ancestors.

Finalized `report.json` and `transcript.txt` are synchronized before exclusive
`complete` publication. Missing input/output, sink failure, cancellation, failed
assertions, and skipped assertions cannot produce batch exit zero. A finalized
negative report can be durable but not successful. `completed_report` validates
successful disposition and marker/content identities; it returns false for
durable negative reports. FNV-1a identities detect mismatch, not malicious forgery.
On Windows, checked `FlushFileBuffers` calls precede completion publication, and
artifact writers close before the marker is created. This is a finalization
guarantee, not a promise of directory-entry durability across power loss. No
writes follow `finish()`. Cleanup failures remain failures, not successful runs.

## Development-branch integration

The development branch combines reusable typed/text commands, Model/RmlUi View,
host-supplied scripts and reports, real Contact/demo adoption, and a non-Magic
SDK-consumer example. Existing frame pacing, audio scheduling, and GLFW
diagnostics are preserved. `VERSION_PROJECT_CHERYL` selects 4.1.0; component versions retain
their existing declarations. This does not promise mixed-version binary ABI
compatibility, publish a package, or qualify any platform.

The released assets revision `768f741b48bcf990285ad1d54d548596343f165b` is selected
to retain its menu documents and named Invaders clips. The console does not
require the separate world-generation, language-practice, or offline Story
applications. Console's graphical surface requires RmlUi; omitting that toolkit
also omits Console rather than creating a separate console product.
Development-branch availability is not a fully qualified release: graphical
interaction, Windows, and installed/relocated SDK use retain their own checks.

## Curated Engine observations

`Runtime::EngineObservations` registers `<prefix>.group`, and, only when passed a
retained LogHandle, `<prefix>.logging_backend` and `<prefix>.logging_queue`.
These are Query-only observations sampled on retained owner progression. Group
status is a synchronized logical-group snapshot, not a completion barrier.
Backend cumulative failures are independently sampled, not a coherent health
verdict; logging queue counts cover the shared pool across all categories, not
one logger. Help text records these scopes. The adapter neither creates a logger
nor changes logging destinations, runtime settings, or `GameRuntime::diagnostics()`.
The demo registers its actual dedicated I/O Group as `console.io.group`.

## Verification targets

Use the [testing guide](../development/testing.md) for these focused checks.
Select the relevant console options before building the named targets; a test
declaration alone is not a passing result for a particular checkout or platform.

- Foundation: Console-off, Console with Runtime-off, and Console+Runtime variants;
  preserve plain Engine/toolkit-off composition and static/shared SDK closure.
- Core target `tests-console-core`, CTest `console_core`, filter
  `ConsoleCore.*:ConsoleLifetime.*` includes setting effects, exact retry,
  range/observer refusal, and retirement. Existing `console_scenarios` retains
  `ConsoleScenario.*:ConsoleScenarioAuthority.*` authority/whole-file coverage.
- Physical I/O target `tests-console-runtime`, CTest `console_runtime_io`, filter
  `ConsoleRuntimeIo.*` declares confinement, hardlink/reparse rejection,
  finalization identity, and output-failure regressions; Windows additionally
  covers reserved aliases and concurrent writers.
- Contact: `living_scenarios`, `living_scenarios_allocation`, and existing
  `living_scenarios_batch_{heal,refused,wrong-assertion,authoring}` declarations.
  Preserve actual price, edit/inspect/quote/deliberate cast, and recovery.
- Installed/relocated SDK: independently configure `.github/ci/sdk-consumer/console`
  against only an installed prefix, build `sdk-console`, and select CTest
  `^sdk-console$`. It owns non-Magic host state and checks typed/text equivalence,
  real advancement/pause, retained retry, positive/failing/empty/invalid scripts,
  diagnostic mutation refusal, missing input, and missing output root.
  It also calls the real RmlUi Session and Console Model/View with routed text
  input, retained result presentation, open/hide/reopen, and ordered teardown.
  This is CPU-side toolkit interaction without a window or graphics context,
  not live graphical rendering or physical-input acceptance.
- Graphical: `tests-demo-console-controller`, CTest `demo_console` with
  `DemoConsole.*:DemoConsoleInput.*`, plus `demo_console_coexistence` when TGUI
  exists. Observe real display/input, partial construction, both runtime modes,
  concurrent shutdown, and owner-thread listener retirement after integration.

Retain the tested revision and configuration with results. Headless checks do
not establish graphical interaction, Windows filesystem behavior, or an installed
SDK consumer. Those checks remain distinct from making the source available on
the development branch.
