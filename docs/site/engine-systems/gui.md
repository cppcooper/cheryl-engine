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
| {cpp:struct}`TGUI::WidgetLayout <CE::UI::TGUI::WidgetLayout>` | Typed placement/resizing for native TGUI widgets; RmlUi uses native RML/RCSS layout. [Choose an interface](../gui/index.md#choose-an-interface); [TGUI layout declarations](../../../projects/modules/ui/tgui/include/cheryl/ui/tgui/layout.h). |
| {cpp:class}`CE::Text::FontService` and {cpp:class}`CE::Text::Font` | Context-aware sessions inherit the Engine default and catalogue; per-widget/document overrides accept the same retained handle as direct text. [GUI fonts](../gui/index.md#select-fonts); [font service](../text/index.md). |

## Usage

Choose a toolkit, construct its session on the UI owner and retain native
widgets/documents there. Record owned CPU data, upload a complete scene on
graphics, then append that scene during frame preparation.

```{engine-diagram} gui-focus-tgui
```

The [RmlUi variant](diagram-explorer.md#gui-focus-rmlui) follows the same retained
publication boundary with its own native document API. The
[application map](diagram-explorer.md#gui-application) includes runtime, view and
focus connections.

### Choose an authoring interface

| Choice | Authoring and layout | Rendering/material rule |
| --- | --- | --- |
| [TGUI](../modules/tgui.md) | Native widgets through `Session::gui()`; optional typed `WidgetLayout` rules. | Colored triangles with straight alpha. The uploader supplies defaults unless explicit `Materials` overrides are passed. |
| [RmlUi](../modules/rmlui.md) | Native RML documents, RCSS styles and element/event APIs through `Session::context()`. | Colored triangles with premultiplied alpha; rectangular clipping is supported, effects are reported unavailable. |

Both use retained Engine fonts and CPU image decoding. Their session, recording,
scene and material types remain distinct. Each integration owns process-global
toolkit state with its own coexistence restrictions; consult its module owner
before composing sessions. Toolkit capabilities also define which input and
platform services are actually available.

### Uses

| Consumer | Dependency | Integration boundary |
| --- | --- | --- |
| Toolkit `Session` | [Input](input.md) {cpp:class}`Input::System <CE::Input::System>` and copied {cpp:struct}`Input::Events::Record <CE::Input::Events::Record>` values | Borrow input for the session lifetime; feed the entire ordered batch once. Keyboard focus leases and pointer selection are separate application decisions. |
| Font/image bridges | [Text](text.md) retained fonts and `FontRasterizer`; [Assets](assets.md) CPU image decoding | Share CPU data/operations without sharing live toolkit objects or using the direct text renderer as the toolkit's layout engine. |
| `SceneUploader` | [Resources](resources.md) provider and [Dispatch](dispatch.md) `PlatformDispatcher::Submission` | Upload on the original provider/owner. Submitted work owns its recording/material handles and contains no live widgets/documents. |
| Uploaded `Scene` | [Rendering](rendering.md) `FrameWriter` and retained draw packets | `write(frame)` appends a pass; rendering later needs no toolkit access. |
| Optional view/time attachments | [Runtime](runtime.md) `SimulationViewSource` and `RuntimeServices` | Apply copied dimensions or selected simulation-step time on the UI owner, not by reading a live window from simulation. |

### Used by

| Consumer | Entry point | Responsibility |
| --- | --- | --- |
| Application UI controller | `UI::TGUI::Session` or `UI::RmlUi::Session` | Own native authoring, focus policy, pointer selection and callbacks into simulation-owned application state. [GUI construction](../gui/index.md). |
| Menus, HUD components and editors | Native widgets/documents in that session | Share the enclosing session's routing identity and lifetime; release native references before toolkit closure. [HUD tutorial](../getting-started/create-a/hud.md). |
| Application frame preparation | `UI::TGUI::Scene::write()` or `UI::RmlUi::Scene::write()` | Select a complete uploaded scene and its pass order. `adopt_scene()` polls without waiting and preserves the current scene on failure. |

### Decide who supplies view and time

The Context constructor supplies input and the font catalogue, but automatic
view/time participation still requires the corresponding options. Enable them
on the initialized simulation owner, normally during `init_simulation()`;
the automatic view source is not ready during `init_platform()`.

| Mode | View and time work | Work still supplied by the application |
| --- | --- | --- |
| Automatic | Enable `SessionOptions::automatic_view` and/or `automatic_time` when constructing with Context, or explicitly call `follow_view()` / `follow_time()`. | Feed records, choose focus/pointer recipients, author UI, record/upload/adopt scenes and append them to frames. |
| Manual | Call `set_view()` with copied logical/framebuffer dimensions and `update_time()` with simulation elapsed seconds. | Initialize dimensions before input/recording and refresh changes; keep advancement on the UI owner. |

A valid `set_view()` switches sizing to manual. Call `detach_time()` before
manual time advancement when automatic time is attached. Automatic time starts
with future selected simulation steps; it does not replay the current step.
See the [TGUI](../modules/tgui.md#automatic-view-and-time) and
[RmlUi](../modules/rmlui.md#automatic-view-and-time) contracts for required
attachment failure and teardown behavior.

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

With `session_` constructed on its owner as shown in
[GUI construction](../gui/index.md#construct-widgets-on-the-ui-owner), create a native
TGUI button. A session constructed with `Context` already inherits the Engine
default font and configured catalogue; no widget font setup is needed for this case:

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
[the maintained toolkit demo](../../../projects/apps/demo/src/tgui-demo.cpp).
