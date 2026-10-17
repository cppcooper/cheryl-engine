# Bind input actions

Bind physical controls once during initialization, then let game updates read
semantic actions. This task uses the Native GLFW input implementation selected
by [First Game](../first-game.md). The result is a movement/quit mapping whose
game logic does not depend on upstream device-library constants.

## Choose action and registration storage

Add these game members, with `<array>` and the input headers:

```cpp
static constexpr CE::Input::Actions::Id quit_{1};
static constexpr CE::Input::Actions::Id left_{2};
static constexpr CE::Input::Actions::Id right_{3};
std::array<CE::Input::Actions::BindingId, 3> bindings_{};
```

An action ID identifies game meaning. A binding ID identifies one registration
for later removal; they are not interchangeable. Keep the action IDs distinct
within the input system shared by your game.

Find physical controls in the [control guide](../../runtime/input-state-model.md#control-identities-and-binding-tokens):
{cpp:enum}`CE::Input::KeyboardKey`, {cpp:enum}`CE::Input::MouseButton` and
{cpp:enum}`CE::Input::GamepadControl` are Engine enums accepted directly by
bindings. Pointer axes, scroll axes and wheel-button pulses live under
`CE::Input::MouseControl`. The guide lists their headers and how to pair each
family with its device ID.

## Register on the platform owner

Include `<cheryl/core/controls/input-interface.h>` and
`<cheryl/core/controls/button-codes.h>`, and link the selected
[Native GLFW target](../../modules/native-glfw.md) with native input enabled.
During `init()`, after runtime initializes the input adapter:

```cpp
auto& input = engine_.input();
auto& bindings = input.bindings();
const auto keyboard = input.keyboard_id();
bindings_[0] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Escape}, quit_);
bindings_[1] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Left}, left_);
bindings_[2] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Right}, right_);
```

`CE::Input::Control` converts the enum when the binding is registered. The
adapter normalizes native observations to the same Engine control IDs. Obtain
device IDs from `input`; the standard bindings require no numeric casts or
adapter-specific translation. `Unknown` is not bindable, and a named gamepad
control is reported only when the selected backend/device supports it.

## Read the tick during update

```cpp
if (tick.input.button(quit_).pressed()) {
    tick.request_stop();
    return;
}
const float direction = (tick.input.button(right_).held() ? 1.0f : 0.0f)
                      - (tick.input.button(left_).held() ? 1.0f : 0.0f);
position_.x += direction * 160.0f * static_cast<float>(tick.delta_seconds);
```

`pressed()` reports an edge in the consumed observations; `held()` reports the
current semantic state. Runtime chooses the simulation delta independently of
poll count. For movement proportional to measured down-time within an interval,
use the documented `tick.button_simulation_seconds(action)` policy instead.

Bindings remain platform-owned even in concurrent runtime mode. Do not install
or remove them from `update()`; submit an owned request to platform if live
rebinding is required. The tick view is borrowed for that hook.

## Remove registrations during cleanup

In `deinit()`, tolerate a partially completed initialization:

```cpp
for (auto& binding : bindings_) {
    if (binding != 0) {
        static_cast<void>(engine_.input().bindings().unbind(binding));
        binding = 0;
    }
}
```

Only remove your own registrations. Runtime keeps input/window dependencies
alive through game cleanup, then detaches callbacks before destroying the window.

Left/right should move the actor and Escape should request ordinary runtime
shutdown. With both directions held the example cancels movement. While an
exclusive keyboard-focused editor is active, keyboard gameplay actions are
intentionally gated; mouse/controller alternatives are a separate policy.

Read [input contracts](../../runtime/input-state-model.md),
[capture and focus](../../runtime/input-state-model.md#focus-routing-and-editing-controls)
and [GUI routing](create-gui.md). Text input must use committed text records;
do not reconstruct characters from bound key actions.
