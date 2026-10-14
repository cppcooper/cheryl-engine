# Input actions and state

## Bindings

`CE::Input` defines the input protocols shared by all backends, games and other
consumers. `Actions` maps physical controls into semantic state; `Events`
contains ordered physical and committed-text observations. `Capture` requests
those record channels, while `Routing` selects keyboard/text recipients. Capture
and routing serve gameplay, tools and UI without requiring a widget toolkit. The
native implementation is `CE::Platform::GLFW::InputSystem`; optional toolkit
adapters live separately under `CE::UI`.

Cheryl collects physical input on the platform thread. `CE::Input::Actions::Bindings` can map one control or a simultaneous `CE::Input::Actions::Chord` to a game-defined `CE::Input::Actions::Id`. Several button bindings for the same action combine with OR; controls inside one chord combine with AND. Axis bindings add their scaled values and can be gated by a chord. Negative scale inverts an axis, and an optional dead zone rescales the remaining input range. An action cannot be both a button and an axis while bound. A returned `CE::Input::Actions::BindingId` can remove one mapping; `unbind_action` removes all of its mappings. A physical `CE::Input::Control` stores the device ID in `device` and that device's control token in `control`.

This binding example uses Gainput control tokens supplied by the GLFW backend.
The `context` is the runtime context available during game initialization.

```cpp
constexpr CE::Input::Actions::Id Jump{1};
constexpr CE::Input::Actions::Id Save{2};

auto& input = context.input();
auto& bindings = input.bindings();
const auto keyboard = input.keyboard_id(); // The adapter is initialized before game init.
(void)bindings.bind_button({keyboard, gainput::KeySpace}, Jump);
(void)bindings.bind_button(
    CE::Input::Actions::Chord{{{keyboard, gainput::KeyCtrlL}, {keyboard, gainput::KeyS}}}, Save
);
```

## Poll snapshots

Each adapter processes its physical changes and publishes one complete poll. The GLFW adapter feeds State from key/mouse callbacks in their shared observation order, then updates Gainput and publishes the State/record pair through `publish_input()`. Gainput notifications for those externally mapped devices are ignored to prevent replay and cross-device reordering. Gainput-owned devices still use the mapper; the adapter also reconciles the gamepad's current state for reattachment/disconnection when no delta was delivered. Readers receive a `std::shared_ptr<const CE::Input::Actions::Snapshot>` through `CE::Input::System::action_snapshot()`. The published pointer is replaced atomically; an earlier handle keeps its complete sample while later polls proceed.

## Tick input

`CE::Game::AbstractGame::update(const CE::Game::TickContext&)` runs for each selected simulation step, including steps in a bounded recovery batch. At its start the runtime consumes the entire accumulated polling backlog, leaving an empty backlog for new polls. Input never invokes updates or divides the simulation timestep. A press observed at 20 ms and a release at 80 ms of a 100 ms observation interval report 60 ms of raw down-time even when the selected fixed delta is 20 ms. `CE::Game::TickContext::delta_seconds` belongs to the simulation policy; `observed_seconds()` and raw input durations remain on the observation clock. The named `button_simulation_seconds()` helper maps observed down-time proportion onto the selected delta. [Simulation timing](simulation-timing.md) explains variable/fixed stepping, caps, and dropped time.

`CE::Input::Tick::button(id)` reports current/previous state, `pressed()` and `released()`, `press_count`, `release_count`, `held_duration`, `down_duration`, and `completed_holds`. All durations are `std::chrono::duration<double>` (seconds). `held_duration` is the age of the still-active hold, zero after release. `down_duration` is time observed down since the previous consumption. `completed_holds` contains the full duration of every hold released in this batch, including holds started in earlier cycles. Consumption clears activity, counts, completed holds, and relative movement; current buttons, active hold starts, and absolute axes persist.

State timestamps are poll observation times. Multiple semantic transitions inside one poll retain counts, but share that poll's timestamp; a same-poll tap has zero measured duration rather than an invented hardware duration. There is no threshold that erases taps. Repeated unchanged polls do not repeat edges. If a poll crosses a consumption boundary before publication, its State activity arrives in the next batch and down-time is clamped to that batch's interval; ordered records keep their original observation timestamps. `CE::Input::Accumulator::consume()` retains the last State baseline and consumption boundary; it has no simulation callback.

`CE::Input::Actions::AxisOptions::kind` defaults to `CE::Input::AxisKind::Absolute`: current position/value persists and `delta()` compares the final value with the previous consumption's value. `CE::Input::AxisKind::Relative` uses `on_delta()` and sums movement across the batch, then resets to zero. Absolute cursor coordinates and controller axes use the first contract; wheel and relative pointer movement use the second. Bindings for one action cannot mix kinds. Unbinding a held action publishes one release or zero absolute-axis sample. `CE::Input::Tick::polls()` exposes retained sample handles, but does not promise a physical event stream or reconstruct event order within a poll.

Input adapters report physical changes; bindings translate them into semantic action state. GLFW callbacks retain key and mouse-button edges plus scroll pulses, while cursor position is sampled once after the event pump. Game logic receives complete tick input at the update boundary instead of running from device callbacks on the polling thread.

Bind controls to semantic `CE::Input::Actions::Id` values and read them in `update(const CE::Game::TickContext&)`. Custom adapters call `on_button(control, held)` or `on_axis(control, value)`; Gainput's old value is not needed by the binding layer.

## Ownership and validation

`CE::Input::Actions::Bindings` configuration, physical notifications, gates and publication share
one platform owner. `action_snapshot()` atomically copies a retained immutable
handle from any reader thread. Retrieve `poll_snapshot()` after the complete poll
on the platform owner, then share that handle; reading a State and complete-poll
pointer separately during publication is not a combined snapshot operation.
The initialized window is borrowed until callback detachment. Request handles keep
only capture/routing state, rather than extending adapter or window lifetime.
Independent handles can be requested/released across threads, with exclusive access
to each individual move-only handle.

Button chords must be nonempty. Axis scales must be finite, dead zones finite in
`[0, 1)`, and physical values/deltas finite. Values are not clamped to `[-1, 1]`;
dead-zone rescaling can apply to logical movement as well as controller values.
An empty axis modifier chord is unconditional. Publication requires nondecreasing
observation time and preserves the earlier published handle on failure; poll IDs
can skip. Mutable collection/configuration staging has no general rollback/retry
guarantee after allocation or arithmetic failure. Adapters report such failures
through their platform failure boundary rather than publishing a partial sample.

Manual `CE::Input::Tick`/accumulator consumers must provide a nonnull baseline, strictly
newer poll IDs with nondecreasing observation times, and a consumption interval
starting at/after the baseline and ending at/after its start. Samples cannot be later
than that end. These checks precede advancing the accumulator baseline. `CE::Input::Tick`
owns its handles and copied record storage; spans and filtered record views borrow
that value and must not survive its move/destruction. Missing action queries return
inactive/zero state copies. An axis kind change within one batch fails when queried;
consumption boundaries separate supported kind changes.

`CE::Input::PollingBacklog` rejects unknown policies, negative spacing and zero finite capacity.
`complete()` requires eligible capacity/spacing, a newer nonnull State and observation
time no later than completion. Its mutable staging has no allocation-failure rollback
guarantee. A full batch reports no next eligible poll; consuming it restores capacity
while retaining the existing spacing deadline.

## Polling backlog and scheduling

`CE::Runtime::GameRuntime` accepts `CE::Input::PollingOptions` independently of `CE::Runtime::RunMode`. The default `Lockstep` policy permits one completed poll between simulation consumptions. `Finite` permits `capacity` completed polls; `Unlimited` removes that limit. Every completed poll counts, even if no action changed. This implementation keeps all completed poll handles in its transport batch; State derives the useful activity from them. Capacity therefore describes observations, not the number of button transitions or retained changed samples.

`spacing` is a configurable minimum delay from the end of one poll to the beginning of the next (default 1 ms; zero permits polling as fast as the platform loop can run). Consumption reopens capacity without resetting that delay. When a finite/lockstep batch fills, the platform stops pumping input until consumption. It continues rendering and recycling frames. Pending OS events remain with the backend; one later `glfwPollEvents()` processes all waiting events. Cheryl can retain the callbacks that GLFW delivers, but cannot reconstruct transitions or hardware timestamps the backend never exposed.

The worker transfers the entire backlog under the scheduler lock before each actual update and immediately leaves an empty backlog. Both runtime modes use independently configurable variable/fixed timing; the default variable interval is approximately 16.667 ms. A cycle with no update leaves input pending. Every recovery update consumes fresh polls or persistent State without replaying edges, relative motion, Events, or Text. Sequential mode uses the same eligibility rules and may collect multiple eligible polls before a paced update, within its selected capacity; it cannot poll while update or presentation blocks its thread. `stop()` and worker completion wake the scheduler even when capacity is full.

```cpp
CE::Input::PollingOptions polling;
polling.policy = CE::Input::PollingPolicy::Finite;
polling.capacity = 8;
polling.spacing = std::chrono::milliseconds(2);
CE::Runtime::GameRuntime runtime(context, game, CE::Runtime::RunMode::Concurrent, polling);
```

## Ordered Events and OS Text

State is always collected. `CE::Input::System::capture(CE::Input::Capture::Channel::Events)` and `capture(CE::Input::Capture::Channel::Text)` return move-only `CE::Input::Capture::Lease` handles. Requests are reference-counted per channel and may be made or released from simulation; activation is latched at the next platform poll. A lease controls capture, not a private queue. Releasing one leaves other requests active and never removes records already published. Adapters declare `supports(mode)`; requesting an unsupported channel fails explicitly. A State request is harmless and does not disable other channels.

Each `CE::Input::PollSnapshot` contains its `CE::Input::Actions::Snapshot` and an independent ordered vector of `CE::Input::Events::Record`. The runtime transfers these complete polls with the same whole-batch/backpressure policy. `CE::Input::Tick::records()` exposes the concatenated read-only stream for that update. Multiple game/UI consumers may read it without consuming each other's data. The next update has only newly captured records. State flags, deduplication, or aggregation never replace the stream's only copy.

A `CE::Input::Events::Record` has a shared sequence number, observation timestamp, device ID/kind, and a variant payload: `CE::Input::Events::Button`, `CE::Input::Events::Axis`, `CE::Input::Events::Pointer`, `CE::Input::Events::Scroll`, or `CE::Input::Events::Text`. Sequence spans physical and text records, preserving their relative order. Buttons retain press/release/repeat, modifier flags, and backend key/scancode information where available. Pointer records use logical coordinates and wheel records preserve fractional X/Y offsets. GLFW callbacks are recorded before Gainput mapping; unknown key tokens still retain native/scancode information. Gamepad events describe changes between actual samples, in sample-processing order; they cannot recover intermediate physical transitions the gamepad backend did not report. Callback/sample timestamps are observation times, not hardware timestamps.

`CE::Input::Events::Button::key` and `mouse_button` identify known keyboard and mouse controls
through the neutral `CE::Input::KeyboardKey` and `CE::Input::MouseButton` enums. UI/tool consumers can
recognize editing keys, shortcuts and pointer buttons without interpreting GLFW or
Gainput constants. Letter/punctuation names refer to US keyboard positions, not
localized characters. `CE::Input::Events::Text` remains the sole committed-character source.
`button` remains an opaque per-device State binding ID; portable identity does not
change existing bindings or imply that a binding token exists. For example, GLFW
F20 has portable identity even though the current Gainput mapping has no token.
Adapters leave unrecognized/inapplicable identities `Unknown` and retain available
native/scancode diagnostics. Older aggregate construction defaults the added fields
to `Unknown`; numeric enum values are not a persisted or native-code interchange.

Mouse button and scroll records carry optional observation-time logical `position`.
Native GLFW captures it in the corresponding callback, so a first click after
capture activation and a click followed by movement retain the correct location.
Sources without that information leave it absent. Consumers requiring a click/wheel
position must reject that unsupported input or arrange an explicit source contract;
they must not substitute a later cursor sample or assume a move preceded the event.

GLFW's Unicode character callback supplies `CE::Input::Events::Text::codepoint` (one valid Unicode scalar). Characters follow OS keyboard layout and include whatever repeated committed characters the OS delivers. They are never reconstructed from physical key codes. Physical repeats remain `CE::Input::Events::ButtonPhase::Repeat` without creating repeated State presses. Text and editing controls share stream order: Backspace, Delete, arrows, Home/End, and shortcuts arrive as physical control events, not inferred characters. This backend provides committed text only; it does not expose composition/preedit, candidate selection, grapheme segmentation, clipboard editing, or a complete editor/IME contract.

The GLFW adapter also reports relative State axes under `CE::Input::MouseControl::DeltaX`, `DeltaY`, `ScrollX`, and `ScrollY`. Bind these with `CE::Input::AxisKind::Relative` and read `delta()` once per update. Relative movement is scaled and gated by the modifiers active when it arrives, then accumulated until publication; releasing a modifier or unbinding afterwards does not erase already mapped movement. A newly installed relative binding receives subsequent movement. Absolute axes still use the final sampled value and modifier state. Changing an action's axis kind requires a consumption boundary; mixing kinds in one State batch is rejected explicitly. Existing wheel-button pulse bindings remain available, while fractional/repeated wheel movement uses the relative axis or ordered scroll records.

Custom adapters use `begin_input_poll()` before collection and `publish_input()` once afterwards to publish the State/record pair. Existing State-only adapters can still publish actions directly. Adapters advertising Events/Text must publish the complete poll; a mismatched State-only publication fails rather than silently omitting captured data. `supports_focus()` separately declares routing support; an unsupported routing request fails explicitly. Focus-capable adapters call `begin_input_poll()` even when only State capture is used. A host managing GLFW pumping explicitly calls `CE::Platform::GLFW::InputSystem::begin_poll()`, pumps events, then calls `update()`. Polling, bindings, collection, and publication remain platform-thread operations; capture handles and immutable delivered data cross the thread boundary. Deinitialization detaches all callbacks and discards pending records without invalidating already delivered handles.

## Focus, routing, and editing controls

`CE::Input::Routing::Manager` is a separate focus-request abstraction accessed through `CE::Input::System::routing()`. `focus(nonzero_target_id, policy)` returns a move-only `CE::Input::Routing::Lease`. The latest published request owns keyboard focus; atomic publication decides overlapping requests. Releasing an older lease cannot clear a newer owner, even when the same target ID is reused. Releasing the current lease clears focus without restoring older requests. `CE::Input::Routing::Lease::epoch()` identifies that particular request, and `owns_focus()` is an observation rather than a lock on ownership. Capture requests and focus leases may be held by UI objects on the simulation thread; all platform input work remains on the platform thread.

The platform latches focus at the beginning of each poll, together with capture activation. This one focus snapshot controls both keyboard State gating and record routing. Requests made during collection take effect at the next poll. `CE::Input::Events::Record::target` and `focus_epoch` retain the owner at collection time; pending input is never retroactively assigned to a new focus target. Target zero means no focused recipient. `CE::Input::Tick::records_for(target[, epoch])` exposes the ordered Text and editing-control records for a consumer, while `gameplay_events()` filters physical records allowed through to gameplay. These are non-destructive views, not platform-thread widget callbacks; the game/UI dispatches or reads them during its update. A reused ID can filter by epoch when it represents a new consumer lifetime.

`CE::Input::Routing::KeyboardPolicy::Exclusive` (default) withholds keyboard-derived semantic actions and keyboard records from gameplay while leaving mouse/controller input available. Other-device alternatives for the same action still work. `PassThrough` assigns the focused target and also permits keyboard gameplay input. `CE::Input::Actions::Bindings` implements only the general device gate, not textbox policy: it continues tracking physical keys while disabled. When focus ends, the next poll resumes currently held keys as semantic presses; physical Events do not fabricate matching hardware presses. A focus transition can therefore change State independently of a physical event.

Focusing does not activate capture. A textbox separately holds Events and Text leases, then reads its routed records in shared sequence order. `CE::Input::Events::Text` inserts committed Unicode scalars. `CE::Input::Events::Button` carries press/repeat/release and modifiers for Backspace, Delete, arrows, Home/End, and shortcuts. The widget decides editing semantics, selection, clipboard behavior, and whether a repeated control applies. Capture supplies the records without prescribing a complete text editor or calling UI objects from GLFW callbacks.

```cpp
auto events = input.capture(CE::Input::Capture::Channel::Events);
auto text = input.capture(CE::Input::Capture::Channel::Text);
auto focus = input.routing().focus(textbox_id); // Retain these handles while active.
// During update: read tick.input.records_for(textbox_id) in order.
// On blur: focus.reset(); text.reset(); events.reset();
```

The toolkit-free demo probe requests Events for F2 and Text while focused. It edits
logical Unicode scalars with Backspace/Delete, arrows and Home/End, releasing focus
with Enter/Escape. WASD is gated while controller/scroll input remains available.
Its preview uses the [Unicode text service](../assets/text-layout.md); capture and
editing remain independent of font coverage. Grapheme/bidi caret behavior and IME
need separate editing contracts.

Generic [state utilities](utility-contracts.md) are independent of input. They
define sampled state, synchronized value/revision storage, immediate observation
and transition tables, including callback and failure boundaries. `State` and
`Trigger` describe whole domains, such as enums with several values. A game can
compose machines and let a guard inspect another machine without prescribing
controllers or simulation threads.

GLFW callback work catches its first failure and reports it from `update()` instead of unwinding through the C callback stack. Runtime cleanup detaches callbacks, clears focus and pending capture, and tears down rendering after a poll/worker failure. Already delivered immutable handles remain valid. Capacity limits completed polls, not records inside one pump; an OS pump can deliver many records, all retained when their channels were active. At shutdown the runtime discards pending input after stopping/joining simulation; it does not perform an extra final update.

Validation procedures and coverage limits are in
[architecture-validation.md](../development/architecture-validation.md).
