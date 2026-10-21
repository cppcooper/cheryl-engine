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

Use the selected window directly during platform initialization or an explicit
platform-dispatch callback. During updates, use the runtime's copied dimensions;
that route works in both sequential and concurrent mode.

```{engine-diagram} display-application
```

[Explore the engine-side display relationships](diagram-explorer.md#display-internal).
Arrows point from a consumer to the type it uses.

### Choose a window observation

| Need | Use | What the value means |
| --- | --- | --- |
| Size gameplay or a camera for this update | {cpp:struct}`Game::TickContext <CE::Game::TickContext>` | The logical/pixel size pair selected for this simulation tick. |
| Apply required UI sizing before dependent updates | `engine.simulation_view().watch(...)` | A seeded, simulation-owned selection; retain its {cpp:class}`Runtime::ViewAttachment <CE::Runtime::ViewAttachment>`. |
| Inspect current copied platform state from another thread | `engine.window_state().snapshot()` | A synchronized {cpp:struct}`Runtime::WindowState <CE::Runtime::WindowState>`, including optional native focus. No native window is retained. |
| Change mode, size or cursor visibility; query a live window | `engine.window()` on platform | A borrowed {cpp:class}`Platform::Window <CE::Platform::Window>` with native owner requirements. Dispatch explicit requests from other owners. |
| Observe a raw native framebuffer resize | `Platform::window_resized_event` | Copied pixels plus a borrowed window pointer. Native GLFW publishes on the default `Events::EventSystem` bus. |
| Observe the runtime's sampled main-window close intent | `Platform::window_close_requested_event` on `engine.events()` | An owned intent fact; negotiation belongs to Runtime's shutdown policy. |

Prefer the selected runtime view for game/UI layout. The raw resize channel and
Context bus are distinct routes. A raw callback may be followed by another native
change before runtime selects its view. See
[committed views](../runtime/display-and-window-contract.md#committed-window-and-simulation-views)
and [native close intent](../runtime/display-and-window-contract.md#native-close-intent).

### Uses

| Dependency | Display's use |
| --- | --- |
| {cpp:struct}`Platform::Monitor <CE::Platform::Monitor>` | Identify entries from this display's monitor inventory when creating windows or querying scale. |
| {cpp:struct}`Platform::Dimensions <CE::Platform::Dimensions>`, {cpp:struct}`Platform::FramebufferSize <CE::Platform::FramebufferSize>` | Keep logical coordinates and drawable pixels distinct. Zero pixel extent can describe a minimized drawable. |
| {cpp:struct}`Events::EventSystem <CE::Events::EventSystem>` | Native GLFW dispatches legacy and typed framebuffer-resize observations through the default bus. [Events](events.md#engine-events). |

Native-library state belongs to the selected backend; the portable display
interface does not choose or own a renderer.

### Used by

| Consumer | Relationship to the window |
| --- | --- |
| {cpp:class}`Runtime::Context <CE::Runtime::Context>`, {cpp:class}`Runtime::GameRuntime <CE::Runtime::GameRuntime>` | Context owns the display; Runtime samples its active window and coordinates dependent teardown. [Runtime](runtime.md). |
| {cpp:class}`Input::System <CE::Input::System>` | Borrow the live window for native callbacks; detach while it is still alive. [Input](input.md). |
| {cpp:class}`Rendering::OpenGL::GLFWContext <CE::Rendering::OpenGL::GLFWContext>` | Use the compatible native window for presentation/context operations. [OpenGL integration](../modules/opengl.md). |
| {cpp:class}`Rendering::Camera2D <CE::Rendering::Camera2D>` and application layout | Consume copied drawable dimensions; a camera does not own a native window. [Rendering](rendering.md). |
| {cpp:class}`UI::TGUI::Session <CE::UI::TGUI::Session>`, {cpp:class}`UI::RmlUi::Session <CE::UI::RmlUi::Session>` | Follow copied simulation dimensions through the supported session integration. [GUI](gui.md). |

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
