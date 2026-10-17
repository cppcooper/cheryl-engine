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
| {cpp:struct}`Actions::Id <CE::Input::Actions::Id>`, {cpp:type}`Actions::BindingId <CE::Input::Actions::BindingId>`, {cpp:struct}`Control <CE::Input::Control>`<br>Distinguish game meaning, one registration and a physical device/control pair. [Control identities and tokens](../runtime/input-state-model.md#control-identities-and-binding-tokens). |
| {cpp:class}`Tick <CE::Input::Tick>`<br>Read button/axis state, observed duration and ordered record views during update. [Tick input](../runtime/input-state-model.md#tick-input); [input and simulation time](../runtime/simulation-timing.md#input-and-tick-time). |
| {cpp:struct}`PollSnapshot <CE::Input::PollSnapshot>`, {cpp:class}`Actions::Snapshot <CE::Input::Actions::Snapshot>`<br>Retain complete immutable poll results across later polling. [Poll snapshots](../runtime/input-state-model.md#poll-snapshots); [ordered records](../runtime/input-state-model.md#ordered-events-and-os-text). |
| {cpp:class}`Capture::Lease <CE::Input::Capture::Lease>`, {cpp:class}`Routing::Lease <CE::Input::Routing::Lease>`<br>Keep a record channel requested or a particular keyboard-focus request alive. [Capture](../runtime/input-state-model.md#ordered-events-and-os-text); [routing](../runtime/input-state-model.md#focus-routing-and-editing-controls). |

### Controls for bindings

Pass these Engine enums directly with the corresponding device ID when binding
buttons, chords or axes.

| Enum or constants | Purpose and details |
| --- | --- |
| {cpp:enum}`KeyboardKey <CE::Input::KeyboardKey>` | Letter/digit positions, editing/navigation keys, `F1`–`F25`, keypad and left/right modifiers; pair with `keyboard_id()`. [Identities, headers and binding examples](../runtime/input-state-model.md#control-identities-and-binding-tokens). |
| {cpp:enum}`MouseButton <CE::Input::MouseButton>` | `Left`, `Right`, `Middle`, `Extra1`–`Extra5`; pair with `mouse_id()`. [Mouse binding path](../runtime/input-state-model.md#control-identities-and-binding-tokens). |
| {cpp:enum}`GamepadControl <CE::Input::GamepadControl>` | Named buttons, D-pad, sticks, triggers and optional sensor axes; pair with `gamepad_id()`. `A`/`B`/`X`/`Y` describe Xbox-style positions. Availability depends on the backend/device. [Names and limits](../runtime/input-state-model.md#control-identities-and-binding-tokens). |
| `MouseControl` | Absolute `X`/`Y`, one-poll `WheelUp`/`WheelDown` button pulses, and relative `DeltaX`, `DeltaY`, `ScrollX`, `ScrollY`, paired with `mouse_id()`. [Axis behavior](../runtime/input-state-model.md#ordered-events-and-os-text); [constant declarations](../../projects/engine/include/cheryl/core/controls/input-types.h). |
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

### Uses

A concrete adapter borrows a live window and translates device observations into
complete input polls.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Platform::Window`<br>{cpp:class}`CE::Input::Actions::Bindings`<br>{cpp:class}`CE::Input::Capture::Collector`<br>{cpp:class}`CE::Input::Routing::Manager` | [Display](display.md) |

### Used by

Runtime transfers polls into immutable tick views. Game updates read actions;
toolkit sessions read ordered records and request keyboard/text focus. Input
owns neither a simulation scheduler nor a widget tree.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Runtime::GameRuntime`<br>{cpp:struct}`CE::Game::TickContext`<br>{cpp:class}`CE::UI::TGUI::Session`<br>{cpp:class}`CE::UI::RmlUi::Session`<br>[`Tutorial::Player`](../getting-started/create-a/player.md) | [Runtime](runtime.md)<br>[GUI](gui.md) |

## Contracts

Bind and unbind controls on the platform owner. Button edges/counts and observed
hold durations describe the consumed polling interval; they do not schedule
updates or replace `tick.delta_seconds`. Relative axes accumulate changes,
whereas absolute axes retain their final value. `button_simulation_seconds()`
maps observed down-time proportion onto the selected simulation delta when that
is the desired movement policy.

Records preserve collection order and committed text comes from the backend's
text input, not inferred key names. Readers may inspect the same record batch.
Capture activation and focus are latched at poll start; requests cannot
retroactively reroute already captured records. Exclusive keyboard focus gates
keyboard gameplay input while leaving other devices available. A superseded
focus lease cannot clear a newer request.

## Lifecycle

The context may own input or explicitly borrow it. Borrowed input must survive
runtime cleanup/context lifetime and detach from a still-live window. Register
bindings during game initialization, retain their IDs and remove them during
cleanup, including partial initialization. Keep capture/focus leases only while
their consumer needs them. Lease state and delivered immutable snapshots can
survive adapter destruction; they do not retain a native window.

Shutdown discards pending, unpublished input after simulation stops. It does not
perform an extra game update to consume those records. Already delivered handles
remain valid.

## Execution

Polling, native callback collection, binding changes and publication run on the
platform owner. Runtime transfers complete batches to simulation. Update reads
the tick's borrowed view during that hook; copy owned data for longer use.
Capture and routing requests support concurrent callers, while delivered records
are read-only. GUI widgets are called on their UI/simulation owner, never from a
native callback.

## Integration

After binding application actions during `init()`, an update can select direction
and speed without referring to physical keys:

```cpp
const float direction = (tick.input.button(move_right).held() ? 1.0f : 0.0f)
                      - (tick.input.button(move_left).held() ? 1.0f : 0.0f);
position.x += direction * speed * static_cast<float>(tick.delta_seconds);
```

Here `position`, `speed` and the action IDs belong to the game. For a menu or
editor, route ordered records through a toolkit session instead of deriving text
from these actions. See [binding actions](../getting-started/how-to/bind-input.md)
and [GUI construction](../gui/index.md).

## Limits and further reading

Backend capabilities determine whether Events, committed Text and focus routing
are available. The GLFW backend does not promise composition/preedit, clipboard
editing or a complete IME/editor protocol. Read
[actions, snapshots, polling, records and focus](../runtime/input-state-model.md),
[timing](../runtime/simulation-timing.md) and
[the input interface](../../projects/engine/include/cheryl/core/controls/input-interface.h).
