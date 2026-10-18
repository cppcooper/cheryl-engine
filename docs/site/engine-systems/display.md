# Display

## Purpose

Display manages native windows, monitor inventory, modes and dimensions. It
supplies the window used by input and presentation; rendering is a separate
responsibility.

## Interface

Types below are in `CE::Platform` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`DisplaySystem <CE::Platform::DisplaySystem>`<br>Enumerate monitors, create/select display-owned windows and query monitor scale. [Inventory and selection](../runtime/display-and-window-contract.md#native-glfw-inventory-and-selection); [adapter catalogue](../architecture/interfaces-adapters.md#runtime-adapters). |
| {cpp:class}`Window <CE::Platform::Window>`<br>Read size/mode/close state, request resize/mode/cursor changes and check deferred failures. [Sizes, modes and observation](../runtime/display-and-window-contract.md#sizes-modes-and-observation). |
| {cpp:struct}`Monitor <CE::Platform::Monitor>`<br>Inspect a borrowed monitor inventory entry; copy values before deferred use. [Monitor identity and freshness](../runtime/display-and-window-contract.md#native-glfw-inventory-and-selection). |
| {cpp:struct}`WindowResized <CE::Platform::WindowResized>`, `CE::Platform::window_resized_event`<br>Receive copied drawable pixels and a display-borrowed window pointer. [Resize observations](../runtime/display-and-window-contract.md#sizes-modes-and-observation); [typed channels](../runtime/event-delivery.md#typed-channels). |

[Native GLFW](../modules/native-glfw.md) implements these desktop contracts.

### Dimensions and modes

| Type or choice | Purpose and details |
| --- | --- |
| {cpp:struct}`Dimensions <CE::Platform::Dimensions>` | Logical window coordinates; creation/resize requests need positive dimensions. [Size units](../runtime/display-and-window-contract.md#sizes-modes-and-observation). |
| {cpp:struct}`FramebufferSize <CE::Platform::FramebufferSize>` | Drawable pixels, which may be zero; runtime copies them into ticks. [Framebuffer observations](../runtime/display-and-window-contract.md#sizes-modes-and-observation). |
| {cpp:enum}`WindowMode <CE::Platform::WindowMode>` | `NORMAL`, `BORDERLESS`, `FULLSCREEN`; backend policy determines supported transitions. [Window modes](../runtime/display-and-window-contract.md#sizes-modes-and-observation). |
| Monitor content scale | X/Y scale for a recognized monitor snapshot; separate from per-window notification and explicit UI font scale. [Scale and UI capabilities](../runtime/display-and-window-contract.md#scale-and-ui-capabilities). |

## Usage

### Uses

The selected platform implementation owns native library/window state and
publishes resize observations through Events.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Events::EventChannel`<br>{cpp:struct}`CE::Platform::Monitor`<br>{cpp:struct}`CE::Platform::Dimensions`<br>{cpp:struct}`CE::Platform::FramebufferSize` | [Events](events.md) |

### Used by

Runtime composes display with input and presentation. Cameras and UI consume
copied dimensions; live window operations remain on the platform owner.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Runtime::Context`<br>{cpp:class}`CE::Input::System`<br>{cpp:class}`CE::Rendering::OpenGL::GLFWContext`<br>{cpp:class}`CE::Rendering::Camera2D`<br>{cpp:class}`CE::UI::TGUI::Session`<br>{cpp:class}`CE::UI::RmlUi::Session` | [Runtime](runtime.md)<br>[Input](input.md)<br>[Rendering](rendering.md)<br>[GUI](gui.md) |

## Contracts

Window dimensions use logical window coordinates; framebuffer dimensions use
drawable pixels and may be zero. Creation/resize requests require positive
logical dimensions. Monitor inventory references are borrowed and expire on
backend refresh or destruction. Copy values before deferred use.
The [current Native GLFW inventory](../runtime/display-and-window-contract.md#native-glfw-inventory-and-selection)
is captured once at construction and has no hotplug refresh.

Monitor content scale is not a per-window scale notification contract. Window
activation alone does not switch a renderer's graphics-resource domain. A
resize payload's window pointer neither extends its lifetime nor grants access
from another thread.

## Lifecycle

The display owns created windows; pointers returned to callers are borrowed.
Construct compatible input/presentation dependents while the display is live.
Detach callbacks and close graphics dependents before destroying their window.
Runtime coordinates the normal session cleanup; application-supplied services
must preserve the same order during partial initialization and failure.

## Execution

Native display/window operations run on the platform owner. Queries do not
synchronize access from simulation. Resize callbacks remain on platform; pass
their copied observations through runtime input/frame handoff rather than
mutating simulation state directly.

## Integration

Update a simulation-owned camera from the runtime's copied observation:

```cpp
void update(const CE::Game::TickContext& tick) override {
    camera_.set_framebuffer_size(tick.framebuffer_size);
    // Update the game without querying engine_.window() here.
}
```

Game initialization can read the live window on platform. Later updates use the
tick in either runtime mode, so the same game remains safe when concurrent.

## Limits and further reading

Read [display/window contracts](../runtime/display-and-window-contract.md) for
monitor freshness, mode transitions, scale and native failure limits. The
[Window](../../../projects/engine/include/cheryl/core/display/window-interface.h)
and [DisplaySystem](../../../projects/engine/include/cheryl/core/display/display-system-interface.h)
headers define the portable interface; module details stay in the owner guide.
