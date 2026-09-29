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

`TickInput` combines zero or more completed polls since the preceding simulation tick. It keeps the previous tick's last poll as its baseline. Button queries return the final held state and whether any consumed poll pressed or released the action; axis queries return the baseline and final values. When no poll arrived, held and axis values persist without replaying edges or movement. The ordered poll handles remain available for game logic that needs transitions across polls. `AbstractGame::update(const TickContext&)` receives this read-only view and a delta from the simulation clock. Sequential runtime consumes one completed poll per tick; concurrent poll queuing and scheduling remain to be implemented.

`button(id)` returns `held()`, `pressed()`, and `released()`; `axis(id)` returns the current and previous values and `delta()`. A press and release between two commits sets both button edges even if the final held state is false. Unbinding a held action publishes one release or a zero axis sample; later queries return an inactive state. One `ActionSnapshot` represents one completed input poll. `TickInput` preserves the order of polls, but the current button flags do not preserve the number or order of multiple transitions inside one poll. The game owns sequence recognition and decides whether to consume actions in a controller, an ECS system, or its update loop. A physical chord only works when the device reports all its controls.

Input adapters report physical changes; bindings translate them into semantic action state. GLFW callbacks retain key and mouse-button edges plus scroll pulses, while cursor position is sampled once after the event pump. Game logic will receive complete tick input at the update boundary instead of running from device callbacks on the polling thread.

For code using the earlier API, replace `bind_button(control, callback)` or `bind_axis(control, callback)` with a semantic `ActionId` binding and read it in `update(const TickContext&)`. Custom adapters call `on_button(control, held)` or `on_axis(control, value)`; Gainput's old value is not needed by the binding layer.

Generic tools are independent of input: `StateTracker<T>` compares successive samples; `VersionedVariable<T>` synchronizes a value, revision, and change waiter; `ObservedVariable<T>` layers immediate callbacks on that storage; and `StateMachine<State, Trigger>` defines typed transition rules, guards, and hooks. `State` and `Trigger` are the types of whole domains, such as enums with several values. A game may compose multiple machines and let a guard inspect another machine. None of these tools assumes the game has controllers or runs its simulation on another thread.
