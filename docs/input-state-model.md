# Input actions and state

Cheryl collects physical input on the platform thread. `InputBindings` can map one control or a simultaneous `InputChord` to a game-defined `ActionId`. Several button bindings for the same action combine with OR; controls inside one chord combine with AND. Axis bindings add their scaled values and can be gated by a chord. Negative scale inverts an axis, and an optional dead zone rescales the remaining input range. An action cannot be both a button and an axis while bound. A returned `BindingId` can remove one mapping; `unbind_action` removes all of its mappings.

```cpp
constexpr CE::Input::ActionId Jump{1};
constexpr CE::Input::ActionId Save{2};

auto& bindings = engine.input().bindings();
(void)bindings.bind_button({keyboard, gainput::KeySpace}, Jump);
(void)bindings.bind_button(CE::Input::InputChord{{{keyboard, gainput::KeyCtrlL},
                                                  {keyboard, gainput::KeyS}}}, Save);
```

Each adapter processes its physical changes and publishes one complete poll. The GLFW adapter feeds State from key/mouse callbacks in their shared observation order, then updates Gainput and publishes the State/record pair through `publish_input()`. Gainput notifications for those externally mapped devices are ignored to prevent replay and cross-device reordering. Gainput-owned devices still use the mapper; the adapter also reconciles the gamepad's current state for reattachment/disconnection when no delta was delivered. Readers receive a `std::shared_ptr<const ActionSnapshot>` through `iInputSystem::action_snapshot()`. The published pointer is replaced atomically; an earlier handle keeps its complete sample while later polls proceed.

`AbstractGame::update(const TickContext&)` runs for each selected simulation step, including steps in a bounded recovery batch. At its start the runtime consumes the entire accumulated polling backlog, leaving an empty backlog for new polls. Input never invokes updates or divides the simulation timestep. A press observed at 20 ms and a release at 80 ms of a 100 ms observation interval report 60 ms of raw down-time even when the selected fixed delta is 20 ms. `TickContext::delta_seconds` belongs to the simulation policy; `observed_seconds()` and raw input durations remain on the observation clock. The named `button_simulation_seconds()` helper maps observed down-time proportion onto the selected delta. [Simulation timing](SIMULATION-TIMING.md) explains variable/fixed stepping, caps, and dropped time.

`TickInput::button(id)` reports current/previous state, `pressed()` and `released()`, `press_count`, `release_count`, `held_duration`, `down_duration`, and `completed_holds`. All durations are `std::chrono::duration<double>` (seconds). `held_duration` is the age of the still-active hold, zero after release. `down_duration` is time observed down since the previous consumption. `completed_holds` contains the full duration of every hold released in this batch, including holds started in earlier cycles. Consumption clears activity, counts, completed holds, and relative movement; current buttons, active hold starts, and absolute axes persist.

State timestamps are poll observation times. Multiple semantic transitions inside one poll retain counts, but share that poll's timestamp; a same-poll tap has zero measured duration rather than an invented hardware duration. There is no threshold that erases taps. Repeated unchanged polls do not repeat edges. If a poll crosses a consumption boundary before publication, its State activity arrives in the next batch and down-time is clamped to that batch's interval; ordered records keep their original observation timestamps. `InputAccumulator::consume()` retains the last State baseline and consumption boundary; it has no simulation callback.

`AxisOptions::kind` defaults to `AxisKind::Absolute`: current position/value persists and `delta()` compares the final value with the previous consumption's value. `AxisKind::Relative` uses `on_delta()` and sums movement across the batch, then resets to zero. Absolute cursor coordinates and controller axes use the first contract; wheel and relative pointer movement use the second. Bindings for one action cannot mix kinds. Unbinding a held action publishes one release or zero absolute-axis sample. `TickInput::polls()` exposes retained sample handles, but does not promise a physical event stream or reconstruct event order within a poll.

Input adapters report physical changes; bindings translate them into semantic action state. GLFW callbacks retain key and mouse-button edges plus scroll pulses, while cursor position is sampled once after the event pump. Game logic will receive complete tick input at the update boundary instead of running from device callbacks on the polling thread.

For code using the earlier API, replace `bind_button(control, callback)` or `bind_axis(control, callback)` with a semantic `ActionId` binding and read it in `update(const TickContext&)`. Custom adapters call `on_button(control, held)` or `on_axis(control, value)`; Gainput's old value is not needed by the binding layer.

## Polling backlog and scheduling

`GameRuntime` accepts `PollingOptions` independently of `RunMode`. The default `Lockstep` policy permits one completed poll between simulation consumptions. `Finite` permits `capacity` completed polls; `Unlimited` removes that limit. Every completed poll counts, even if no action changed. This implementation keeps all completed poll handles in its transport batch; State derives the useful activity from them. Capacity therefore describes observations, not the number of button transitions or retained changed samples.

`spacing` is a configurable minimum delay from the end of one poll to the beginning of the next (default 1 ms; zero permits polling as fast as the platform loop can run). Consumption reopens capacity without resetting that delay. When a finite/lockstep batch fills, the platform stops pumping input until consumption. It continues rendering and recycling frames. Pending OS events remain with the backend; one later `glfwPollEvents()` processes all waiting events. Cheryl can retain the callbacks that GLFW delivers, but cannot reconstruct transitions or hardware timestamps the backend never exposed.

The worker transfers the entire backlog under the scheduler lock before each actual update and immediately leaves an empty backlog. Both runtime modes use independently configurable variable/fixed timing; the default variable interval is approximately 16.667 ms. A cycle with no update leaves input pending. Every recovery update consumes fresh polls or persistent State without replaying edges, relative motion, Events, or Text. Sequential mode uses the same eligibility rules and may collect multiple eligible polls before a paced update, within its selected capacity; it cannot poll while update or presentation blocks its thread. `stop()` and worker completion wake the scheduler even when capacity is full.

```cpp
CE::Input::PollingOptions polling;
polling.policy = CE::Input::PollingPolicy::Finite;
polling.capacity = 8;
polling.spacing = std::chrono::milliseconds(2);
CE::GFramework::GameRuntime runtime(engine, game, CE::GFramework::RunMode::Concurrent, polling);
```

## Ordered Events and OS Text

State is always collected. `iInputSystem::capture(InputMode::Events)` and `capture(InputMode::Text)` return move-only `CaptureLease` handles. Requests are reference-counted per channel and may be made or released from simulation; activation is latched at the next platform poll. A lease controls capture, not a private queue. Releasing one leaves other requests active and never removes records already published. Adapters declare `supports(mode)`; requesting an unsupported channel fails explicitly. A State request is harmless and does not disable other channels.

Each `PollSnapshot` contains its `ActionSnapshot` and an independent ordered vector of `InputRecord`. The runtime transfers these complete polls with the same whole-batch/backpressure policy. `TickInput::records()` exposes the concatenated read-only stream for that update. Multiple game/UI consumers may read it without consuming each other's data. The next update has only newly captured records. State flags, deduplication, or aggregation never replace the stream's only copy.

An `InputRecord` has a shared sequence number, observation timestamp, device ID/kind, and a variant payload: `ButtonEvent`, `AxisEvent`, `PointerEvent`, `ScrollEvent`, or `TextEvent`. Sequence spans physical and text records, preserving their relative order. Buttons retain press/release/repeat, modifier flags, and backend key/scancode information where available. Pointer records use logical coordinates and wheel records preserve fractional X/Y offsets. GLFW callbacks are recorded before Gainput mapping; unknown key tokens still retain native/scancode information. Gamepad events describe changes between actual samples, in sample-processing order; they cannot recover intermediate physical transitions the gamepad backend did not report. Callback/sample timestamps are observation times, not hardware timestamps.

GLFW's Unicode character callback supplies `TextEvent::codepoint` (one valid Unicode scalar). Characters follow OS keyboard layout and include whatever repeated committed characters the OS delivers. They are never reconstructed from physical key codes. Physical repeats remain `ButtonPhase::Repeat` without creating repeated State presses. Text and editing controls share stream order: Backspace, Delete, arrows, Home/End, and shortcuts arrive as physical control events, not inferred characters. This backend provides committed text only; it does not expose composition/preedit, candidate selection, grapheme segmentation, clipboard editing, or a complete editor/IME contract.

The GLFW adapter also reports relative State axes under `MouseControl::DeltaX`, `DeltaY`, `ScrollX`, and `ScrollY`. Bind these with `AxisKind::Relative` and read `delta()` once per update. Relative movement is scaled and gated by the modifiers active when it arrives, then accumulated until publication; releasing a modifier or unbinding afterwards does not erase already mapped movement. A newly installed relative binding receives subsequent movement. Absolute axes still use the final sampled value and modifier state. Changing an action's axis kind requires a consumption boundary; mixing kinds in one State batch is rejected explicitly. Existing wheel-button pulse bindings remain available, while fractional/repeated wheel movement uses the relative axis or ordered scroll records.

Custom adapters use `begin_input_poll()` before collection and `publish_input()` once afterwards to publish the State/record pair. Existing State-only adapters can still publish actions directly. Adapters advertising Events/Text must publish the complete poll; a mismatched State-only publication fails rather than silently omitting captured data. `supports_focus()` separately declares routing support; an unsupported routing request fails explicitly. Focus-capable adapters call `begin_input_poll()` even when only State capture is used. A host managing GLFW pumping explicitly calls `InputSystem::begin_poll()`, pumps events, then calls `update()`. Polling, bindings, collection, and publication remain platform-thread operations; capture handles and immutable delivered data cross the thread boundary. Deinitialization detaches all callbacks and discards pending records without invalidating already delivered handles.

## Focus, routing, and editing controls

`InputRouting` is a separate focus-request abstraction accessed through `iInputSystem::routing()`. `focus(nonzero_target_id, policy)` returns a move-only `FocusLease`. The latest request owns keyboard focus; releasing an older lease cannot clear a newer owner, even when the same target ID is reused. `FocusLease::epoch()` identifies that particular request. Capture requests and focus leases may be held by UI objects on the simulation thread; all platform input work remains on the platform thread.

The platform latches focus at the beginning of each poll, together with capture activation. This one focus snapshot controls both keyboard State gating and record routing. Requests made during collection take effect at the next poll. `InputRecord::target` and `focus_epoch` retain the owner at collection time; pending input is never retroactively assigned to a new focus target. Target zero means no UI owner. `TickInput::records_for(target[, epoch])` exposes the ordered Text and editing-control records for a consumer, while `gameplay_events()` filters physical records allowed through to gameplay. These are non-destructive views, not platform-thread widget callbacks; the game/UI dispatches or reads them during its update. A reused ID can filter by epoch when it represents a new consumer lifetime.

`KeyboardRouting::Exclusive` (default) withholds keyboard-derived semantic actions and keyboard records from gameplay while leaving mouse/controller input available. Other-device alternatives for the same action still work. `PassThrough` assigns the focused target and also permits keyboard gameplay input. InputBindings implements only the general device gate, not textbox policy: it continues tracking physical keys while disabled. When focus ends, the next poll resumes currently held keys as semantic presses; physical Events do not fabricate matching hardware presses. A focus transition can therefore change State independently of a physical event.

Focusing does not activate capture. A textbox separately holds Events and Text leases, then reads its routed records in shared sequence order. `TextEvent` inserts committed Unicode scalars. `ButtonEvent` carries press/repeat/release and modifiers for Backspace, Delete, arrows, Home/End, and shortcuts. The widget decides editing semantics, selection, clipboard behavior, and whether a repeated control applies. Capture supplies the records without prescribing a complete text editor or calling UI objects from GLFW callbacks.

```cpp
auto events = input.capture(CE::Input::InputMode::Events);
auto text = input.capture(CE::Input::InputMode::Text);
auto focus = input.routing().focus(textbox_id); // Retain these handles while active.
// During update: read tick.input.records_for(textbox_id) in order.
// On blur: focus.reset(); text.reset(); events.reset();
```

The demo requests Events for its F2 focus toggle and requests Text while its textbox is focused. It edits Unicode scalar values with Backspace/Delete, arrows, Home/End, and exits focus with Enter/Escape. WASD gameplay is gated while typing; gamepad input and fractional scroll remain available. Its existing font atlas is ASCII, so the preview displays one `?` for each unsupported scalar. Text capture and storage retain the actual Unicode scalar; font shaping and grapheme-aware editing remain separate work.

Generic tools are independent of input: `StateTracker<T>` compares successive samples; `VersionedVariable<T>` synchronizes a value, revision, and change waiter; `ObservedVariable<T>` layers immediate callbacks on that storage; and `StateMachine<State, Trigger>` defines typed transition rules, guards, and hooks. `State` and `Trigger` are the types of whole domains, such as enums with several values. A game may compose multiple machines and let a guard inspect another machine. None of these tools assumes the game has controllers or runs its simulation on another thread.

GLFW callback work catches its first failure and reports it from `update()` instead of unwinding through the C callback stack. Runtime cleanup detaches callbacks, clears focus and pending capture, and tears down rendering after a poll/worker failure. Already delivered immutable handles remain valid. Capacity limits completed polls, not records inside one pump; an OS pump can deliver many records, all retained when their channels were active. At shutdown the runtime discards pending input after stopping/joining simulation; it does not perform an extra final update.

Implementation and validation status are recorded in [INPUT-IMPLEMENTATION-STATUS.md](INPUT-IMPLEMENTATION-STATUS.md).
