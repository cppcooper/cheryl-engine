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

Each adapter processes its physical changes and then calls `publish_actions()` once per poll. `InputSystem` already does so after Gainput's `Update()`; it also checks the gamepad's current state to handle reattachment and disconnection when Gainput does not deliver a new delta. Readers receive a `std::shared_ptr<const ActionSnapshot>` through `iInputSystem::action_snapshot()`. The published pointer is replaced atomically; an earlier handle keeps its complete sample while later polls proceed.

`AbstractGame::update(const TickContext&)` runs once per independently scheduled simulation cycle. At its start the runtime consumes the entire accumulated polling batch, leaving an empty batch for new polls. Input never invokes updates or divides the simulation timestep. A press observed at 20 ms and a release at 80 ms of a 100 ms period therefore produce one 100 ms update whose State reports 60 ms of observed down-time. `TickContext::delta_seconds` remains simulation elapsed time; game code chooses whether to use that time, current state, or the input durations.

`TickInput::button(id)` reports current/previous state, `pressed()` and `released()`, `press_count`, `release_count`, `held_duration`, `down_duration`, and `completed_holds`. All durations are `std::chrono::duration<double>` (seconds). `held_duration` is the age of the still-active hold, zero after release. `down_duration` is time observed down since the previous consumption. `completed_holds` contains the full duration of every hold released in this batch, including holds started in earlier cycles. Consumption clears activity, counts, completed holds, and relative movement; current buttons, active hold starts, and absolute axes persist.

State timestamps are poll observation times. Multiple semantic transitions inside one poll retain counts, but share that poll's timestamp; a same-poll tap has zero measured duration rather than an invented hardware duration. There is no threshold that erases taps. Repeated unchanged polls do not repeat edges. `InputAccumulator::consume()` retains the last State baseline and consumption boundary; it has no simulation callback.

`AxisOptions::kind` defaults to `AxisKind::Absolute`: current position/value persists and `delta()` compares the final value with the previous consumption's value. `AxisKind::Relative` uses `on_delta()` and sums movement across the batch, then resets to zero. Absolute cursor coordinates and controller axes use the first contract; wheel and relative pointer movement use the second. Bindings for one action cannot mix kinds. Unbinding a held action publishes one release or zero absolute-axis sample. `TickInput::polls()` exposes retained sample handles, but does not promise a physical event stream or reconstruct event order within a poll.

Input adapters report physical changes; bindings translate them into semantic action state. GLFW callbacks retain key and mouse-button edges plus scroll pulses, while cursor position is sampled once after the event pump. Game logic will receive complete tick input at the update boundary instead of running from device callbacks on the polling thread.

For code using the earlier API, replace `bind_button(control, callback)` or `bind_axis(control, callback)` with a semantic `ActionId` binding and read it in `update(const TickContext&)`. Custom adapters call `on_button(control, held)` or `on_axis(control, value)`; Gainput's old value is not needed by the binding layer.

## Capture channels and textbox focus (planned)

`InputMode` names three ways game code may need to observe input. These are capture channels, not an exclusive switch for the entire engine:

- `State` is the implemented path: coherent action state through `ActionSnapshot` and `TickInput`. It answers whether an action is held, pressed, released, or has an axis value. Repeated observations of an unchanged state need no history.
- `Events` is a future ordered, timestamped stream of transitions delivered by the input backend. It matters when each individual transition counts, as in a rhythm game. Converting that stream into state can lose distinct transitions that occurred inside one poll, so it must be retained before the state mapper condenses it. A backend that only samples a device cannot reconstruct transitions the device never reported.
- `Text` is a future ordered stream of text produced by the operating system, respecting keyboard layout and repeat. A textbox also needs editing controls such as Backspace, arrows, and shortcuts; those are control events, not characters inferred from action bindings. Text input must preserve meaningful units in order and must not be flattened into held-key state. Composition/IME support needs an explicit backend contract when this channel is implemented.

Focusing a textbox is a routing decision separate from which channels are captured. The focused textbox should receive text and relevant editing controls while keyboard actions intended for gameplay are withheld from the game; other state, such as a gamepad, may remain available. Capturing State, Events, and Text can be concurrent where a game needs them. The focus API and the exact event/text handoff types remain to be designed; no `iInputSystem` method selects these channels yet. Today the GLFW adapter publishes State only, ignores key-repeat callbacks, and does not install a character callback. An implementation must carry ordered events and text through the runtime's input handoff without silently collapsing them into action snapshots.

Generic tools are independent of input: `StateTracker<T>` compares successive samples; `VersionedVariable<T>` synchronizes a value, revision, and change waiter; `ObservedVariable<T>` layers immediate callbacks on that storage; and `StateMachine<State, Trigger>` defines typed transition rules, guards, and hooks. `State` and `Trigger` are the types of whole domains, such as enums with several values. A game may compose multiple machines and let a guard inspect another machine. None of these tools assumes the game has controllers or runs its simulation on another thread.
