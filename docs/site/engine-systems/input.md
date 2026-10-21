# Input

## Purpose

Input turns device observations into semantic actions and, when requested,
ordered physical/text records. Games consume an immutable tick view without
polling native devices or taking ownership of another consumer's records.

## Interface

Types below are in `CE::Input` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`System <CE::Input::System>`<br>Device IDs, bindings, capture/focus support and complete poll publication. [Adapter publication](../runtime/input-state-model.md#adapter-publication); [adapter catalogue](../architecture/interfaces-adapters.md#runtime-adapters). |
| {cpp:class}`Actions::Bindings <CE::Input::Actions::Bindings>`, {cpp:struct}`Actions::Chord <CE::Input::Actions::Chord>`, {cpp:struct}`Actions::AxisOptions <CE::Input::Actions::AxisOptions>`<br>Register buttons, simultaneous chords and scaled/gated axes on the platform owner. [Bindings](../runtime/input-state-model.md#bindings); [binding task](../getting-started/how-to/bind-input.md). |
| {cpp:struct}`Actions::ContextDefinition <CE::Input::Actions::ContextDefinition>`, {cpp:class}`Actions::ContextSubmission <CE::Input::Actions::ContextSubmission>`, {cpp:class}`Actions::Context <CE::Input::Actions::Context>`<br>Attach reusable bindings for independent consumers from any producer, retaining one move-only lease per consumer. [Action contexts](../runtime/input-state-model.md#action-contexts); [Controller tutorial](../getting-started/create-a/controller.md). |
| {cpp:struct}`Actions::Id <CE::Input::Actions::Id>`, {cpp:type}`Actions::BindingId <CE::Input::Actions::BindingId>`, {cpp:struct}`Control <CE::Input::Control>`<br>Distinguish game meaning, one registration and a physical device/control pair. [Control identities and tokens](../runtime/input-state-model.md#control-identities-and-binding-tokens). |
| {cpp:class}`Tick <CE::Input::Tick>`, {cpp:class}`Actions::View <CE::Input::Actions::View>`<br>Read direct or context-local button/axis state, observed duration and ordered records during update. [Tick input](../runtime/input-state-model.md#tick-input); [context views](../runtime/input-state-model.md#views-and-shutdown). |
| {cpp:struct}`PollSnapshot <CE::Input::PollSnapshot>`, {cpp:class}`Actions::Snapshot <CE::Input::Actions::Snapshot>`<br>Retain complete immutable poll results across later polling. [Poll snapshots](../runtime/input-state-model.md#poll-snapshots); [ordered records](../runtime/input-state-model.md#ordered-events-and-os-text). |
| {cpp:struct}`GamepadAvailability <CE::Input::GamepadAvailability>`, {cpp:struct}`GamepadAvailabilityChanged <CE::Input::GamepadAvailabilityChanged>`<br>Read the current convenience-slot state and subscribe to changes after complete polls. [Gamepad availability and seed](../runtime/input-state-model.md#gamepad-availability). |
| {cpp:class}`Capture::Lease <CE::Input::Capture::Lease>`, {cpp:class}`Routing::Lease <CE::Input::Routing::Lease>`<br>Keep a record channel requested or a particular keyboard-focus request alive. [Capture](../runtime/input-state-model.md#ordered-events-and-os-text); [routing](../runtime/input-state-model.md#focus-routing-and-editing-controls). |

### Controls for bindings

Pass these Engine enums directly with the corresponding device ID when binding
buttons, chords or axes.

| Enum or constants | Purpose and details |
| --- | --- |
| {cpp:enum}`KeyboardKey <CE::Input::KeyboardKey>` | Letter/digit positions, editing/navigation keys, `F1`–`F25`, keypad and left/right modifiers; pair with `keyboard_id()`. [Identities, headers and binding examples](../runtime/input-state-model.md#control-identities-and-binding-tokens). |
| {cpp:enum}`MouseButton <CE::Input::MouseButton>` | `Left`, `Right`, `Middle`, `Extra1`–`Extra5`; pair with `mouse_id()`. [Mouse binding path](../runtime/input-state-model.md#control-identities-and-binding-tokens). |
| {cpp:enum}`GamepadControl <CE::Input::GamepadControl>` | Named buttons, D-pad, sticks, triggers and optional sensor axes; pair with `gamepad_id()`. `A`/`B`/`X`/`Y` describe Xbox-style positions. Availability depends on the backend/device. [Names and limits](../runtime/input-state-model.md#control-identities-and-binding-tokens). |
| `MouseControl` | Absolute `X`/`Y`, one-poll `WheelUp`/`WheelDown` button pulses, and relative `DeltaX`, `DeltaY`, `ScrollX`, `ScrollY`, paired with `mouse_id()`. [Axis behavior](../runtime/input-state-model.md#ordered-events-and-os-text); [constant declarations](../../../projects/engine/include/cheryl/core/controls/input-types.h). |
| {cpp:enum}`AxisKind <CE::Input::AxisKind>` | `Absolute` retains the final value; `Relative` accumulates changes. Set through `Actions::AxisOptions`. [Axis mapping and observation](../runtime/input-state-model.md#ordered-events-and-os-text). |

Keyboard positions do not supply localized text. `Unknown` is not bindable;
standard enums are converted by `Control`, while adapters normalize native codes.
The [control guide](../runtime/input-state-model.md#control-identities-and-binding-tokens)
explains this distinction and lists the headers without repeating every key here.

### Capture, records and routing values

| Enum or value | Purpose and details |
| --- | --- |
| {cpp:enum}`Capture::Channel <CE::Input::Capture::Channel>` | `State` is always collected; `Events` requests physical records; `Text` requests committed Unicode scalars. [Channel activation and leases](../runtime/input-state-model.md#ordered-events-and-os-text). |
| {cpp:enum}`Routing::KeyboardPolicy <CE::Input::Routing::KeyboardPolicy>` | `Exclusive` gates keyboard gameplay; `PassThrough` also permits it. Focus does not activate capture. [Focus and routing](../runtime/input-state-model.md#focus-routing-and-editing-controls). |
| {cpp:enum}`DeviceKind <CE::Input::DeviceKind>`, {cpp:enum}`Events::ButtonPhase <CE::Input::Events::ButtonPhase>`, {cpp:enum}`Modifiers <CE::Input::Modifiers>` | Identify record source, press/release/repeat and modifier flags. [Record interpretation](../runtime/input-state-model.md#ordered-events-and-os-text). |
| {cpp:struct}`Events::Record <CE::Input::Events::Record>`, {cpp:type}`Events::Payload <CE::Input::Events::Payload>` | Ordered button, axis, pointer, scroll or committed-text payload with target/epoch metadata. [Record ownership](../runtime/input-state-model.md#ordered-events-and-os-text); [target filtering](../runtime/input-state-model.md#focus-routing-and-editing-controls). |
| {cpp:enum}`PollingPolicy <CE::Input::PollingPolicy>`, {cpp:struct}`PollingOptions <CE::Input::PollingOptions>` | `Lockstep`, `Finite`, `Unlimited` capacity and minimum spacing between completed polls. [Backlog and scheduling](../runtime/input-state-model.md#polling-backlog-and-scheduling). |

## Usage

Bind physical controls during platform initialization, then read semantic actions
through the update's immutable {cpp:class}`Input::Tick <CE::Input::Tick>`. A reusable
controller retains its own action context so local action IDs and bindings do
not collide with another controller's.

```{engine-diagram} input-focus-actions
```

[Explore the full application path](diagram-explorer.md#input-application),
[UI capture and focus](diagram-explorer.md#input-focus-capture), or
[engine input dependencies](diagram-explorer.md#input-internal).

### Choose what to consume

| Reader's task | API path | Setup and ownership |
| --- | --- | --- |
| Read a few game-wide actions | {cpp:class}`Input::Actions::Bindings <CE::Input::Actions::Bindings>` → `tick.input.button(id)` | Register direct bindings on platform; retain tokens and remove them during cleanup. [Bind input](../getting-started/how-to/bind-input.md). |
| Reuse a player/controller mapping | {cpp:struct}`Input::Actions::ContextDefinition <CE::Input::Actions::ContextDefinition>` → {cpp:class}`Input::Actions::Context <CE::Input::Actions::Context>` → `tick.input.actions(context)` | Acquire `action_contexts()` on platform once; attach from the consumer's owner and retain one lease per controller. [Controller tutorial](../getting-started/create-a/controller.md). |
| Feed a menu or text editor | {cpp:class}`Input::Capture::Lease <CE::Input::Capture::Lease>` and {cpp:class}`Input::Routing::Lease <CE::Input::Routing::Lease>` → tick records | Request Events/Text separately from keyboard focus. Read ordered records on the UI owner, preferably through a [toolkit session](gui.md). |
| Retain a complete native poll | {cpp:struct}`Input::PollSnapshot <CE::Input::PollSnapshot>` | Acquire after publication on platform; the immutable handle can cross threads and survive later polls. |
| React to convenience-gamepad availability | `Input::gamepad_availability_changed_event` plus `gamepad_availability()` | Subscribe and read the seed together on platform; select queued delivery for a simulation-owned target. [Availability contract](../runtime/input-state-model.md#gamepad-availability). |

### Actions, capture and focus are independent

Actions describe gameplay meaning. Capture requests activate ordered physical or
committed-text records; they do not consume a private queue. Focus chooses a
record recipient and may gate keyboard gameplay. Focusing alone does not request
Events or Text, and action-context lifetime does not establish UI focus.

Keep capture/focus leases while their UI consumer is active. Each poll latches
those requests once, so a mid-poll change takes effect on the next poll and
cannot reassign records already captured. Multiple consumers can inspect the
same immutable record batch. See [capture and routing](../runtime/input-state-model.md#focus-routing-and-editing-controls).

### Uses

| Dependency | Input's use |
| --- | --- |
| {cpp:class}`Platform::Window <CE::Platform::Window>` | A concrete adapter borrows a compatible live window while native callbacks are attached. [Display](display.md). |
| {cpp:class}`Input::Actions::Bindings <CE::Input::Actions::Bindings>` | Evaluate direct and context-local mappings into immutable action snapshots. |
| {cpp:class}`Input::Capture::Collector <CE::Input::Capture::Collector>`, {cpp:class}`Input::Routing::Manager <CE::Input::Routing::Manager>` | Collect supported records and latch focus/capture for one complete poll. |
| {cpp:class}`Events::EventBus::Publisher <CE::Events::EventBus::Publisher>` | Publish optional gamepad observations through a weak route. Runtime selects its Context bus; standalone input opts in. [Events](events.md#engine-events). |

### Used by

| Consumer | What it reads or requests |
| --- | --- |
| {cpp:class}`Runtime::GameRuntime <CE::Runtime::GameRuntime>` | Poll on platform and transfer complete polling intervals to immutable tick input. [Runtime](runtime.md). |
| Application controllers | Read direct or context-local actions from {cpp:struct}`Game::TickContext <CE::Game::TickContext>` without querying live devices. |
| {cpp:class}`UI::TGUI::Session <CE::UI::TGUI::Session>`, {cpp:class}`UI::RmlUi::Session <CE::UI::RmlUi::Session>` | Request supported record/focus capabilities and feed widgets during their update. [GUI](gui.md). |
| Standalone host or custom adapter | Drive complete polling/publication explicitly when Runtime is absent. [Adapter publication](../runtime/input-state-model.md#adapter-publication). |

## Contracts

Directly bind and unbind controls on the platform owner. For runtime consumers,
obtain `action_contexts()` there once and retain its endpoint; attach definitions
or reset context leases on the consumer's owner. Each context has independent
local action IDs, and accepted changes take effect at the next poll.
Button edges/counts and observed
hold durations describe the consumed polling interval; they do not schedule
updates or replace `tick.delta_seconds`. Relative axes accumulate changes,
whereas absolute axes retain their final value. For direct actions,
`button_simulation_seconds()` maps observed down-time proportion onto the
selected simulation delta when that is the desired movement policy.

Records preserve collection order and committed text comes from the backend's
text input, not inferred key names. Readers may inspect the same record batch.
Capture activation and focus are latched at poll start; requests cannot
retroactively reroute already captured records. Exclusive keyboard focus gates
keyboard gameplay input while leaving other devices available. A superseded
focus lease cannot clear a newer request.

## Lifecycle

`Runtime::Context` may own input or explicitly borrow it. Borrowed input must survive
runtime cleanup/context lifetime and detach from a still-live window. Register
bindings during game initialization, retain their IDs and remove them during
cleanup, including partial initialization. Context leases release their mappings
when their controller leaves; the endpoint remains safe after input destruction but
rejects new attachments. Keep capture/focus leases only while
their consumer needs them. Lease state and delivered immutable snapshots can
survive adapter destruction; they do not retain a native window.

Shutdown discards pending, unpublished input after simulation stops. It does not
perform an extra game update to consume those records. Already delivered handles
remain valid.

## Execution

Polling, native callback collection, direct binding changes and publication run on the
platform owner. Runtime transfers complete batches to simulation. Update reads
the tick's borrowed view during that hook; copy owned data for longer use.
Context attachment/removal, capture and routing requests support concurrent callers,
with exclusive access when moving/resetting each individual lease. Delivered records
are read-only. GUI widgets are called on their UI/simulation owner, never from a
native callback.

## Integration

After binding application actions during `init_platform()`, an update can select direction
and speed without referring to physical keys:

```cpp
const float direction = (tick.input.button(move_right).held() ? 1.0f : 0.0f)
                      - (tick.input.button(move_left).held() ? 1.0f : 0.0f);
position.x += direction * speed * static_cast<float>(tick.delta_seconds);
```

Here `position`, `speed` and the action IDs belong to the game. Held-state
movement is a policy that moves for the whole selected update when the button
ends held; a tap released before consumption contributes no movement in this
example. For proportional contribution from observed taps, use
[`tick.button_simulation_seconds(action)`](../runtime/simulation-timing.md#input-and-tick-time)
and multiply its result by speed instead of multiplying `held()` by delta.
For a menu or
editor, route ordered records through a toolkit session instead of deriving text
from these actions. See [binding actions](../getting-started/how-to/bind-input.md)
and [GUI construction](../gui/index.md).

A reusable controller instead reads `tick.input.actions(its_context).button(id)`.
The [Controller tutorial](../getting-started/create-a/controller.md) puts those
local IDs and the context lease into one class; [Character](../getting-started/create-a/character.md)
consumes its commands, including runtime player joins and departures.

## Limits and further reading

Backend capabilities determine whether Events, committed Text and focus routing
are available. The GLFW backend does not promise composition/preedit, clipboard
editing or a complete IME/editor protocol. Read
[actions, snapshots, polling, records and focus](../runtime/input-state-model.md),
[timing](../runtime/simulation-timing.md) and
[the input interface](../../../projects/engine/include/cheryl/core/controls/input-interface.h).
