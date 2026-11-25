(developer-console-tutorial)=
# Add a developer console

Add an in-game console with editable commands, completion, history, and retained
results. Register a setting and an action against application-owned state, open
the console with the physical GraveAccent key, and exercise the same handlers
through typed calls and a bounded script.

This is an advanced extension of [First Game](../first-game.md). Start with the
copied `my_game` project from [Project setup](../project-setup.md), including its
`src/first-game.h`, `src/first-game.cpp`, and existing assets. Keep its startup,
sprite, text, and graphics initialization. The lesson adds one RmlUi session
dedicated to the console; it does not require the TGUI HUD lessons.

The console's sample host owns a small simulation counter. Advancing it performs
the configured number of substeps; zero pauses that counter. This gives commands
an observable effect without depending on the demo's Contact or Magic systems.
It does not change First Game's actor or pause the surrounding game.

```{contents} In this tutorial
:local:
:depth: 1
```

## Select Console

In the source project's existing `block(SCOPE_FOR VARIABLES)`, change both local
selections before `add_subdirectory()`:

```cmake
set(CHERYL_BUILD_UI_RMLUI ON)
set(CHERYL_BUILD_CONSOLE ON)
```

These local selections take precedence over same-name command-line cache options.
Prepare RmlUi through the existing [dependency setup](../project-setup.md#dependencies);
selecting a module does not download its dependencies. For an installed SDK,
retain `find_package(Cheryl 4 CONFIG REQUIRED)` and select an SDK containing Console.

Add Console to the executable's existing links, using your actual target name:

```cmake
target_link_libraries(my_game PRIVATE Cheryl::Console)
```

Keep the existing startup/backend links. Console supplies Engine and RmlUi usage
requirements transitively; there are no separate core, runtime, or view libraries
to link. The normal SDK includes Console by default. See the
[Console component contract](../../modules/console.md#select-the-surface) for
deliberately reduced builds and runtime deployment.

In `src/main.cpp`, give the embedded console room by replacing the existing
`make_startup()` call with:

```cpp
auto startup = CE::Application::make_startup(
    "Cheryl console tutorial",
    {.width = 1000, .height = 800, .title = "Developer console: GraveAccent opens/closes"});
```

Keep the application's asset option and remaining startup sequence.

## Give commands an application owner

Copy the maintained [independent host header](../../../../.github/ci/sdk-consumer/console/host.h)
into your game's `src/console-host.h`, unchanged. It keeps its `Independent`
namespace. This header also supplies the isolated fixture used later in the lesson.
Its implementation is included here from the maintained SDK consumer:

```{literalinclude} ../../../../.github/ci/sdk-consumer/console/host.h
:language: cpp
```

Read this host as four responsibilities:

| Part | What the application supplies |
| --- | --- |
| `Simulation` | Actual owner state: the substep limit, completed substeps, and a write count. Its `advance()` uses the current limit. |
| `Authority`, issued `Context`s, and the Registry gate | A gameplay context with read/write rights and a distinct diagnostic context with read rights. The gate recognizes those issued scopes. |
| `UnsignedSetting` and retained `Registration`s | A bounded setting, an advancing Action, and a read-only Query. |
| `Presentation::Connection` and `Runtime::Fixture` | Access to the same Registry through an interactive Session, or through fresh gameplay/diagnostic Sessions for an isolated script. |

The `1` and `2` rights bits are this host's read and write policy. Checking rights
alone would not recognize whether a context belongs to this host, so the gate
also checks `same_scope()`. Keep the gate pure and non-reentrant. All Registry,
Session, and owner-state operations in this example run on the simulation owner.

`UnsignedSetting` registers `simulation.step_limit.read` and
`simulation.step_limit.set`. The allowed unsigned range is 0 through 16. The
declared default of 4 describes the owner's policy; `Simulation::step_limit = 4`
performs the actual initialization. Reads report `default`, `effective`,
`minimum`, `maximum`, and `timing`. The callbacks use the real owner rather than
maintaining a second settings value.

The custom `simulation.advance` Action shows the command lifecycle. Its descriptor
declares its name, help, required rights, kind, and unsigned result. The registered
callback prepares a `CounterWork` that retains the simulation. Mutation happens
in `CounterWork::step()`, after the Session has retained the request. Binding a
command must not perform its effect. The `simulation.steps` Query uses the same
Work implementation with advancement disabled.

Keep each move-only `Registration` for as long as its command should be available.
Retire registrations before destroying state their callbacks borrow. Work that
can remain admitted must retain everything it still needs; this example captures
a shared simulation owner. `close()` cancels unfinished work without waiting and
does not undo an already completed effect. See
[host authority and retention](../../modules/console.md#own-the-host-and-its-authority).

## Connect the model, view, and input

The application connects the command host to the graphical surface:

```text
Console input -> RmlUi View -> Model -> Connection -> Registry and Session
                                                         |
                                                     owned Work
                                                         |
                                                   Simulation state
```

`Model` retains command lines and typed results. `View` owns the console document
and its native listeners, borrowing the Model and RmlUi session. `Input` selects
one View, supplies the default toggle, and decides whether gameplay may consume
the current input batch. Linking Console alone installs no shortcut. A command
line is limited to 4096 UTF-8 bytes; history retains at most 32 lines / 32768 bytes,
and displayed output is bounded to 65536 bytes.

Add these includes to `first-game.h`:

```cpp
#include "console-host.h"

#include <cheryl/tools/console/input.h>
#include <cheryl/tools/console/view.h>
#include <cheryl/ui/rmlui/scene.h>
#include <cheryl/ui/rmlui/session.h>

#include <future>
```

The copied header supplies the Model include. After `engine_`, add the following
game members in this order. Existing game members can follow them:

```cpp
CE::Runtime::PlatformDispatcher::Submission console_platform_{
    engine_.platform_dispatcher().submission()};
std::optional<CE::UI::RmlUi::SceneUploader> console_uploader_;
CE::UI::RmlUi::Scene console_scene_;
std::future<CE::UI::RmlUi::Scene> console_pending_;
std::unique_ptr<CE::UI::RmlUi::Session> console_ui_;
std::shared_ptr<Independent::Host> console_host_;
std::unique_ptr<CE::Tools::Console::Presentation::Model> console_model_;
std::unique_ptr<CE::Tools::Console::Presentation::View> console_view_;
std::unique_ptr<CE::Tools::Console::Presentation::Input> console_input_;
```

Add `void init_simulation() override;` and `void deinit_simulation() override;`
to the class's public hook declarations. At the end of the existing
`Game::init_platform()`, construct the uploader on the graphics owner:

```cpp
console_uploader_.emplace(engine_.resources());
```

Define the new initialization hook in `first-game.cpp`, inside `namespace FirstGame`:

```cpp
void Game::init_simulation() {
    namespace Presentation = CE::Tools::Console::Presentation;
    constexpr CE::Input::Routing::TargetId console_target = 7;
    console_ui_ = std::make_unique<CE::UI::RmlUi::Session>(
        engine_, console_target,
        CE::UI::RmlUi::SessionOptions{.automatic_view = true});
    console_host_ = std::make_shared<Independent::Host>();
    console_model_ = std::make_unique<Presentation::Model>(console_host_);
    console_view_ = std::make_unique<Presentation::View>(*console_ui_, *console_model_);
    console_input_ = std::make_unique<Presentation::Input>(*console_ui_, *console_view_);
}
```

The RmlUi session, command host, Model, View, and Input share the simulation owner.
Concurrent mode creates them on its simulation thread; sequential mode uses the
calling thread. The uploader remains platform-owned in either mode.

The nonzero `console_target` identifies this session for keyboard/text routing.
Choose an unused target if your application has other recipients. The automatic
view option applies the initial size before View construction and later changes
before update, including minimization. Time remains manual below.

The View starts hidden and uses an embedded RML/RCSS document. No console asset
files or application fonts are required: the session imports the Engine's font
default, including its safety fallback, and the uploader supplies default
materials. RmlUi owns process globals and permits one live Cheryl RmlUi session.
If you already followed [the RmlUi GUI guide](../how-to/create-gui.md), reuse that
session and uploader instead; see [composition with other interfaces](#compose-with-other-interfaces)
before combining input policies.

## Route input before gameplay

At the start of `Game::update()`, after updating `camera_` and before First Game's
quit check, insert:

```cpp
const bool drawable = tick.logical_size.width > 0 && tick.logical_size.height > 0
    && tick.framebuffer_size.width > 0 && tick.framebuffer_size.height > 0;
const bool gameplay_allowed = console_input_->handle_input(tick.input.records(), drawable);
console_model_->pump();
console_view_->update();
console_ui_->update_time(tick.delta_seconds);
```

The input helper drains the complete ordered batch into RmlUi exactly once.
Do not also call `console_ui_->handle_input()` for that batch. Its return value
applies to the whole batch, including actions queued before an opening or closing
toggle. Decide admission before any action changes gameplay or requests quit.

Replace First Game's quit condition, direction expression, and cell-change
condition respectively with:

```cpp
if (gameplay_allowed && tick.input.button(quit_).pressed()) {
    tick.request_stop();
    return;
}

const float direction = gameplay_allowed
    ? (tick.input.button(right_).held() ? 1.0f : 0.0f)
        - (tick.input.button(left_).held() ? 1.0f : 0.0f)
    : 0.0f;

// Keep the existing position/clamping calculations between these fragments.
if (gameplay_allowed && tick.input.button(next_cell_).pressed())
    actor_->cell = (actor_->cell + 1) % actor_->sprite->definition().grid.cell_count();
```

Keep all other update calculations. A larger game must also discard queued
gameplay/menu effects when admission is false. This example gates input effects
while continuing simulation, command progression, and UI publication. Opening
the console does not pause the runtime.

A physical GraveAccent **Press** opens and focuses the console; the next Press
closes it. Shift is allowed, while Repeat and Release never toggle. Held controls
remain quarantined until release so closing the console does not turn an old
gesture into a new gameplay action. Focus acquisition is deferred until the
batch finishes. A non-drawable batch closes the console. Text attributed to the
toggle is filtered; ordinary pasted backticks and tildes remain editable.

For a different key, replace the Input construction with:

```cpp
console_input_ = std::make_unique<CE::Tools::Console::Presentation::Input>(
    *console_ui_, *console_view_,
    CE::Tools::Console::Presentation::InputOptions{CE::Input::KeyboardKey::F10});
```

`InputOptions{std::nullopt}` disables the binding; explicit `console_view_->open()`
still works. See [input ownership](../../modules/console.md#input-ownership) for
text association limits and the advanced drain contract.

## Publish the console scene

At the end of `Game::update()`, after the existing actor updates, add:

```cpp
static_cast<void>(CE::UI::RmlUi::adopt_scene(console_pending_, console_scene_));
if (!console_pending_.valid())
    console_pending_ = console_uploader_->submit(console_platform_, console_ui_->record());
```

At the end of `prepare_render_frame()`, after the existing sprite and score draws:

```cpp
console_scene_.write(frame);
```

Model progression executes retained commands; `View::update()` copies current
presentation state into its document; toolkit time updates layout; recording and
upload produce the retained scene that is appended to the frame. Keep these
steps running while the View is hidden so admitted work can still finish.

Retain the previous complete scene while at most one replacement is pending.
`adopt_scene()` does not wait; an upload failure propagates through the runtime's
normal failure cleanup in this example. The first scene may be empty until the
platform services the upload. See [RmlUi publication](../../modules/rmlui.md#recording-and-publication)
for recording and rendering limits.

## Retire on the owner, then close the toolkit

Define the new cleanup hook alongside the other game hooks:

```cpp
void Game::deinit_simulation() {
    if (console_input_)
        console_input_->retire();
    if (console_model_)
        console_model_->close();
}
```

Retirement must run while the simulation/UI owner is still alive, before its
thread joins. It is idempotent and releases the input policy's ownership. Model
closure stops admission and closes the host Sessions without waiting. Completed
results remain readable; hiding or reopening a View does not resubmit commands.

At the start of the existing `deinit_platform()`, after the runtime has joined
simulation and settled accepted work, add:

```cpp
console_input_.reset();
console_view_.reset();
console_model_.reset();
console_host_.reset();
if (console_ui_) {
    console_ui_->close_after_quiescence();
    console_ui_.reset();
}
console_scene_ = CE::UI::RmlUi::Scene{};
console_uploader_.reset();
```

Keep First Game's other cleanup. View destruction detaches native listeners and
closes its document while the RmlUi session still exists. The guards also handle
partial initialization. Final toolkit closure follows quiescence; merely joining
simulation is not a substitute for settling accepted dependencies. See the
[runtime lifecycle](../../runtime/runtime-frame-boundary.md) for these hooks.

## Try the interactive path

Reconfigure and rebuild the copied game using [its existing build commands](../project-setup.md#build-your-game-with-a-source-checkout),
then launch it normally. Expect First Game's sprite and score, with the console
initially hidden. Open it with GraveAccent and enter these lines separately:

```text
simulation.step_limit.read
simulation.step_limit.set 6
simulation.advance
simulation.steps
simulation.step_limit.set 0
simulation.advance
```

The first read should report effective/default 4, minimum 0, maximum 16, and
`OwnerImmediate` timing. The first advance returns 6 completed substeps; the Query
returns the same count. After setting zero, the next advance still returns 6.
Setting 17 should report a range failure and preserve the effective value.

The setter also accepts `simulation.step_limit.set value=6`. Arguments can be
positional or named; positional arguments must precede named ones, and each
required argument appears exactly once. Double quotes preserve spaces in text
arguments. Inside quotes, only `\"` and `\\` are escape sequences. Each line is
one command. These rules apply to text submitted through the View or Model.

Try Tab after `simulation.step_limit.r` and Up/Down for command history. A host
can call `console_model_->help("simulation.step_limit.set")` on the simulation
owner to add the discovered command contract to its output. Close and reopen the
console: output should persist and the counter should remain unchanged. Try
opening while holding an actor movement key, release it, close, and make a fresh
movement gesture. Input used by the console must not move the actor, change its
cell, or quit the game. Resize/minimize the window and repeat with `--concurrent`
to exercise the other runtime ownership mode.

## Call the same handler from C++

A typed caller binds against the same descriptor and submits through a Session.
For an isolated experiment, run this fragment on its owning thread, with a fresh
host rather than the one whose Session the Model uses:

```cpp
namespace Console = CE::Tools::Console;
auto host = std::make_shared<Independent::Host>();
auto request = host->registry().bind(
    "simulation.step_limit.set", {Console::Value::unsigned_integer(6)}, host->context());
if (!request)
    throw std::runtime_error("Cannot bind the setting");
if (!host->session().submit(1, *request))
    throw std::runtime_error("Cannot retain the setting");
// The actual owner still has step_limit == 4 here.
if (!host->session().pump())
    throw std::runtime_error("Cannot progress the setting");
auto result = host->session().lookup(1);
if (!result || (*result)->stage != Console::Stage::Succeeded || host->simulation().step_limit != 6)
    throw std::runtime_error("Setting did not reach its owner");

// The same ID and admitted body retrieve the retained operation.
if (!host->session().submit(1, *request) || !host->session().pump())
    throw std::runtime_error("Cannot retry the setting");
if (host->simulation().writes != 1)
    throw std::runtime_error("Retry repeated the effect");
host->close();
```

`bind_text(registry, context, line)` provides the equivalent textual binding;
Model performs that binding and manages its own request IDs. A new nonzero ID
means a new operation. Reusing an ID with a different request conflicts. Session
results are retained rather than evicted; its default capacity is 32 requests,
so a long-lived host needs a deliberate Session renewal policy at capacity.
The sample Host has fixed Sessions: starting over uses a fresh Host and Model
and resets the sample simulation. A production host can retain its game state
while replacing the interactive Session and Model on their owner.
Reacquire borrowed results from `lookup()` after progression. Use
[the Session declaration](../../../../projects/modules/tools/console/include/cheryl/tools/console/session.h)
for its exact retention and capacity rules.

## Exercise the commands with a bounded script

Scripts validate commands and assertions against the same Registry. They use
fresh, dedicated gameplay and diagnostic Sessions and must not borrow the
interactive Model's used Session. For this experiment, use a separate
`Independent::Host` and the following maintained script:

```{literalinclude} ../../../../.github/ci/sdk-consumer/console/fixtures/success.console
:language: text
```

`fixture.load independent-v1` names the host fixture. Ordinary commands use the
gameplay channel. `observe` selects its separate Query-only diagnostic channel;
it cannot perform settings or actions. `-> name` retains a result, `$name.field`
addresses a typed field, and `retry` repeats the admitted request's identity rather
than its effect. The zero-limit assertions check the owner's pause behavior.

For an in-memory check, put this fragment in a separate test function. Define
`std::string_view source` containing the script above before the fragment; it
does not perform file I/O or create a durable report:

```cpp
namespace Scenario = CE::Tools::Console::Scenario;
Independent::Host fixture;
auto plan = Scenario::prepare(fixture.registry(), fixture.channels(), source);
if (!plan)
    throw std::runtime_error("Invalid console script");
Scenario::Run run(*plan, fixture.registry());
while (run.step() == Scenario::Disposition::Running) {}
if (run.disposition() != Scenario::Disposition::Passed)
    throw std::runtime_error("Console assertions did not pass");
```

Here the caller has already constructed the matching fixture; `prepare()` does
not invoke a factory. The whole file is checked before any handler progresses.
A Plan starts once, including its copies; create a new host and Plan for another
run. The Run borrows the Registry and Sessions, so it is destroyed first. It
closes its dedicated Sessions at completion. This short loop is for an isolated
synchronous check; inside a running game, retain the Run and call `step()` over
owner updates.

Version 1 has no loops, shell execution, or nested `test.run`. Bounds include
64 KiB source, 256 lines, 128 statements, 32 names, 256 Session pumps, and a
ten-second default wall deadline. Missing `finish`, no assertions, unknown
commands, and invalid diagnostic writes cannot pass. For negative checks,
change an assertion to a wrong value and expect `Failed`; add
`observe simulation.step_limit.set 6 -> forbidden` before `finish` and expect
preparation to reject the file without changing the owner.

## Add file jobs and reports when needed

`Scenario::Run` supplies in-memory assertions. Use `Runtime::Job` when the host
also needs bounded physical input, isolated fixture creation, and finalized
`report.json`, `transcript.txt`, and `complete` artifacts. The copied host already
implements `Runtime::Fixture`, and `Independent::fixture` recognizes
`independent-v1` without an asset.

The maintained [SDK consumer main](../../../../.github/ci/sdk-consumer/console/main.cpp)
shows a complete standalone owner for this layer: it creates a Pool and I/O Group,
retains `Runtime::IoService`, sets `Config::factory = Independent::fixture`, and
steps each Job. Supply approved absolute script/output roots that already exist,
a script name relative to the script root, a fresh single-component `run_id`,
and a nonempty build identity. An empty `asset` needs no asset I/O.

The fixture factory runs on the Job step caller, so it must create isolated state
rather than mutate the live game. A finished Job is successful only when its
disposition and report finalization agree: use `batch_exit() == 0` for batch
acceptance. `durable()` alone can also describe a finalized failing report.
Optional `Runtime::Commands` registers `test.run script run` in the interactive
Registry; a successful command Stage still requires checking its returned
`disposition` and `durable` fields.

Shutdown closes I/O admission, closes/destroys Jobs and their handles, settles the
I/O service on the shutdown owner, and then shuts down its Pool. Do not wait for
I/O from a UI callback or pool worker. Follow the full
[file-job and report contract](../../modules/console.md#run-an-isolated-test-file)
before adding this layer to the game. This synchronous SDK example demonstrates
the ownership sequence; the graphical integration above does not start file jobs.

## Compose with other interfaces

Keep one RmlUi session and add documents to it. Custom console styling uses
`Presentation::ViewOptions::document` with the required control IDs listed in
[graphical composition](../../modules/console.md#graphical-composition). Prepared
buttons populate the command field; Enter or Submit still sends that text through
the Model. Application output must be escaped before insertion as RML.

The ordinary Input drain above is for a console-only interface. Other editable
documents or a TGUI HUD/menu need a shared focus, pointer, and effect policy.
Use Input's synchronous custom drain to feed each participating session exactly
once, call `route_toggle(record)` before per-record toolkit delivery, and cancel
pending Console focus when another control selects its owner. Preserve held-key
and pointer quarantine in that path; the custom drain replaces the ordinary
helper's generic quarantine. `transitioned()` allows queued UI effects to be
cancelled before delivery.

The maintained [demo routing adapter](../../../../projects/apps/demo/src/developer-console-routing.cpp)
shows that mixed-toolkit policy. Start from the ordinary helper unless you need
those choices. For production commands, replace the sample simulation with your
game's retained owner and keep the same authority, registration, Work, and
cleanup boundaries. Continue with the
[Console reference](../../modules/console.md) for exact contracts and
[SDK integration](../../development/consuming-engine.md#console-consumers) for
installed consumers.
