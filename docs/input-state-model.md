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

`AbstractGame::update(const TickContext&)` receives one input state and the seconds to advance using that state. A completed poll records when its state was observed. If several changes arrive while a concurrent update is busy, the runtime calls `update()` for the intervals before, between, and after those changes, then prepares one render frame from the resulting simulation state. For example, a press observed at 20 ms and a release at 80 ms of a 100 ms period produce 20 ms unpressed, 60 ms held, and 20 ms released. Sequential mode uses the same rule. An unchanged poll is not retained; changed polls remain in publication order. A poll time is an observation time, not the precise hardware event time.

`TickInput` keeps the preceding state as its baseline. Button queries give held, pressed, and released; axis queries give previous and current values. No new change means held and axis values persist without replaying edges. A press and release in one poll remains a tap: both edges are true, but the poll cannot tell how long it was held. There is no default threshold that erases short taps. Preserving an arbitrary number of actual changes while simulation is stalled indefinitely can still grow the pending history; limiting that requires an explicit policy about which events to lose or merge.

`button(id)` returns `held()`, `pressed()`, and `released()`; `axis(id)` returns the current and previous values and `delta()`. Unbinding a held action publishes one release or a zero axis sample; later queries return an inactive state. One `ActionSnapshot` represents one completed input poll. `TickInput::polls()` still exposes individual poll handles for game logic that needs them, but ordinary code need only inspect the current update's input. The flags cannot recover the order or duration of multiple transitions inside one poll. The game owns sequence recognition and decides whether to consume actions in a controller, an ECS system, or its update loop. A physical chord only works when the device reports all its controls.

Input adapters report physical changes; bindings translate them into semantic action state. GLFW callbacks retain key and mouse-button edges plus scroll pulses, while cursor position is sampled once after the event pump. Game logic will receive complete tick input at the update boundary instead of running from device callbacks on the polling thread.

For code using the earlier API, replace `bind_button(control, callback)` or `bind_axis(control, callback)` with a semantic `ActionId` binding and read it in `update(const TickContext&)`. Custom adapters call `on_button(control, held)` or `on_axis(control, value)`; Gainput's old value is not needed by the binding layer.

## Capture channels and textbox focus (planned)

`InputMode` names three ways game code may need to observe input. These are capture channels, not an exclusive switch for the entire engine:

- `State` is the implemented path: coherent action state through `ActionSnapshot` and `TickInput`. It answers whether an action is held, pressed, released, or has an axis value. Repeated observations of an unchanged state need no history.
- `Events` is a future ordered, timestamped stream of transitions delivered by the input backend. It matters when each individual transition counts, as in a rhythm game. Converting that stream into state can lose distinct transitions that occurred inside one poll, so it must be retained before the state mapper condenses it. A backend that only samples a device cannot reconstruct transitions the device never reported.
- `Text` is a future ordered stream of text produced by the operating system, respecting keyboard layout and repeat. A textbox also needs editing controls such as Backspace, arrows, and shortcuts; those are control events, not characters inferred from action bindings. Text input must preserve meaningful units in order and must not be flattened into held-key state. Composition/IME support needs an explicit backend contract when this channel is implemented.

Focusing a textbox is a routing decision separate from which channels are captured. The focused textbox should receive text and relevant editing controls while keyboard actions intended for gameplay are withheld from the game; other state, such as a gamepad, may remain available. Capturing State, Events, and Text can be concurrent where a game needs them. The focus API and the exact event/text handoff types remain to be designed; no `iInputSystem` method selects these channels yet. Today the GLFW adapter publishes State only, ignores key-repeat callbacks, and does not install a character callback. An implementation must carry ordered events and text through the runtime's input handoff without silently collapsing them into action snapshots.

Generic tools are independent of input: `StateTracker<T>` compares successive samples; `VersionedVariable<T>` synchronizes a value, revision, and change waiter; `ObservedVariable<T>` layers immediate callbacks on that storage; and `StateMachine<State, Trigger>` defines typed transition rules, guards, and hooks. `State` and `Trigger` are the types of whole domains, such as enums with several values. A game may compose multiple machines and let a guard inspect another machine. None of these tools assumes the game has controllers or runs its simulation on another thread.
