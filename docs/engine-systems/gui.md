# GUI

## Purpose

GUI integrations connect toolkit-native widgets/documents to Engine input and
retained rendering. The application controls session lifetime, UI ownership,
focus, pointer selection and integration with game state.

## Interface

Types below are in `CE::UI` unless another namespace is shown.

The [TGUI](../modules/tgui.md) and [RmlUi](../modules/rmlui.md) modules each own
their session, native authoring API and recording/publication types.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`TGUI::Session <CE::UI::TGUI::Session>`, {cpp:class}`RmlUi::Session <CE::UI::RmlUi::Session>`<br>Own toolkit lifetime; supply copied dimensions, time and input, and manage explicit keyboard focus. [Session construction](../gui/index.md#construct-widgets-on-the-ui-owner); [routing](../gui/index.md#route-input-and-focus-explicitly). |
| {cpp:class}`TGUI::RecordedScene <CE::UI::TGUI::RecordedScene>`, {cpp:class}`RmlUi::RecordedScene <CE::UI::RmlUi::RecordedScene>`<br>Retain owned CPU recordings without live toolkit objects crossing to graphics. [Update and publication](../development/ui-adapters.md#update-publication-and-shutdown). |
| {cpp:class}`TGUI::SceneUploader <CE::UI::TGUI::SceneUploader>`, {cpp:class}`RmlUi::SceneUploader <CE::UI::RmlUi::SceneUploader>`<br>Transfer owned recordings to the provider owner and publish complete scenes. [Upload and cleanup](../gui/index.md#publish-and-clean-up). |
| {cpp:class}`TGUI::Scene <CE::UI::TGUI::Scene>`, {cpp:class}`RmlUi::Scene <CE::UI::RmlUi::Scene>`<br>Append retained UI draw packets during frame preparation. [Retained publication](../development/ui-adapters.md#update-publication-and-shutdown); [GUI task](../getting-started/how-to/create-gui.md). |

### Configuration and capabilities

| Type or choice | Purpose and details |
| --- | --- |
| {cpp:struct}`TGUI::SessionOptions <CE::UI::TGUI::SessionOptions>`, {cpp:struct}`RmlUi::SessionOptions <CE::UI::RmlUi::SessionOptions>` | Set maximum texture size; TGUI also supplies explicit font scale. RmlUi uses native document styling. [Adapter ownership](../development/ui-adapters.md#module-and-application-ownership); [TGUI](../modules/tgui.md), [RmlUi](../modules/rmlui.md). |
| {cpp:struct}`TGUI::Capabilities <CE::UI::TGUI::Capabilities>`, {cpp:struct}`RmlUi::Capabilities <CE::UI::RmlUi::Capabilities>` | Report keyboard focus, committed text, clipboard, cursor and IME availability; RmlUi also reports clipping/effects support. [Unavailable services](../development/ui-adapters.md#routing-and-unavailable-services). |
| {cpp:enum}`CE::Input::Routing::KeyboardPolicy` and pointer selection | `Exclusive`/`PassThrough` controls keyboard gameplay. The application separately selects pointer recipients. [GUI routing](../gui/index.md#route-input-and-focus-explicitly). |
| {cpp:struct}`TGUI::Materials <CE::UI::TGUI::Materials>`, {cpp:struct}`RmlUi::Materials <CE::UI::RmlUi::Materials>` | Optionally override automatic defaults with compatible loaded material handles. [Custom UI materials](../rendering/pipelines-and-materials.md#custom-ui-materials). |
| {cpp:struct}`TGUI::WidgetLayout <CE::UI::TGUI::WidgetLayout>` | Typed placement/resizing for native TGUI widgets; RmlUi uses native RML/RCSS layout. [Choose an interface](../gui/index.md#choose-an-interface); [TGUI layout declarations](../../projects/modules/ui/tgui/include/cheryl/ui/tgui/layout.h). |

## Usage

### Uses

Sessions consume copied input, dimensions and time. Adapters share Engine
font/image operations while retaining toolkit authoring, atlas and effect
behavior; Runtime supplies lifecycle and frame boundaries.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Input::System`<br>{cpp:struct}`CE::Input::Events::Record`<br>{cpp:struct}`CE::Rendering::Resources::Provider`<br>{cpp:class}`CE::Rendering::Resources::Material`<br>{cpp:class}`CE::Text::FontRasterizer`<br>{cpp:class}`CE::Runtime::PlatformDispatcher::Submission`<br>{cpp:class}`CE::Rendering::PassWriter`<br>{cpp:struct}`CE::Platform::FramebufferSize` | [Input](input.md)<br>[Resources](resources.md)<br>[Text](text.md)<br>[Dispatch](dispatch.md)<br>[Rendering](rendering.md)<br>[Display](display.md)<br>[Runtime](runtime.md) |

### Used by

Menus, editors and interactive HUDs return intents to game state. Frame
preparation appends uploaded scenes without exposing live toolkit objects to
playback.

| Types | Systems |
| --- | --- |
| [`DemoGame`](../../projects/apps/demo/src/demo-game.h)<br>[TGUI HUD](../getting-started/create-a/hud.md)<br>[Application-owned UI views](../gui/index.md#construct-widgets-on-the-ui-owner) | [Rendering](rendering.md) |

## Contracts

Create the session before native toolkit objects and call them only on the UI
owner. Input must outlive its session; external widgets/fonts/textures/documents
must not outlive toolkit shutdown. Toolkit global state imposes session
coexistence limits described by each owner.

Feed the complete ordered input batch once so modifier releases remain visible.
Keyboard focus and pointer selection are separate choices. Copied tick
dimensions determine layout; monitor scale or a live window query must not be
invented on simulation. Recording/upload must preserve earlier immutable
generations already retained by frames.

## Lifecycle

Prepare the scene uploader during platform-owned initialization. It creates
default materials lazily through the selected resource provider. Create
the toolkit session on its UI/simulation owner, then native objects. Update input,
time and layout there, record CPU state, dispatch upload and retain the latest
complete scene. During shutdown, stop UI calls and release application-held
toolkit references before `close_after_quiescence()` and session destruction.
Keep accepted upload dependencies alive until runtime settles them.

## Execution

In concurrent runtime mode, UI mutation/recording belongs on simulation and
resource upload belongs on graphics. Sessions permit final platform teardown
after simulation has joined. Toolkit callbacks must update UI/simulation-owned
state or submit owned work, not directly perform native graphics operations.

## Integration

An application creates a native button on the TGUI session owner:

```cpp
auto button = tgui::Button::create("Apply");
button->onPress([this] { apply_requested_ = true; });
session_->gui().add(button);
session_->request_keyboard_focus();
```

The application retains `session_` and the callback target. It must also select
pointer delivery, feed records and publish the recorded scene. The
[GUI construction guide](../gui/index.md) explains those surrounding steps;
[the HUD tutorial](../getting-started/create-a/hud.md) connects TGUI widgets to a
game, and [the focused GUI task](../getting-started/how-to/create-gui.md) shows
the RmlUi document approach.

## Limits and further reading

There is no universal Engine widget API or context-owned UI manager. Reported
capabilities define clipboard, cursor, text/focus and IME availability. Read the
[module owner guides](../modules/index.md),
[UI adapter contracts](../development/ui-adapters.md) and
[the maintained toolkit demo](../../projects/apps/demo/src/tgui-demo.cpp).
