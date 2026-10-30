# Rendering

## Purpose

Rendering turns a completed snapshot of visual state into ordered backend draws
and presentation. The game selects what to show; the renderer consumes copied
values and retained resources without reading live simulation objects.

## Interface

Types below are in `CE::Rendering` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`FrameWriter <CE::Rendering::FrameWriter>`, {cpp:class}`PassWriter <CE::Rendering::PassWriter>`, {cpp:class}`Frame <CE::Rendering::Frame>`<br>Build ordered passes in a borrowed slot, then publish complete retained visual state. [Frame storage and writers](../rendering/pipelines-and-materials.md#frame-storage-and-writers); [handoff](../runtime/runtime-frame-boundary.md#frame-publication-and-recycling). |
| {cpp:struct}`DrawStyle2D <CE::Rendering::DrawStyle2D>`, {cpp:struct}`DrawPacket2D <CE::Rendering::DrawPacket2D>`<br>Resolve sprite/tile/graphic/text resources and copied placement into complete packets. [CPU submission](../rendering/pipelines-and-materials.md#cpu-submission). |
| {cpp:class}`Camera2D <CE::Rendering::Camera2D>`, {cpp:class}`Camera3D <CE::Rendering::Camera3D>`<br>Own camera state and provide matrices copied into a pass. [Camera observations](../rendering/pipelines-and-materials.md#camera-observations); [camera task](../getting-started/how-to/use-camera.md). |
| {cpp:class}`Resources::Pipeline <CE::Rendering::Resources::Pipeline>`, {cpp:class}`Resources::Material <CE::Rendering::Resources::Material>`<br>Retain executable/fixed state and resolve typed material parameters. [Parameters and ownership](../rendering/pipelines-and-materials.md#parameters-and-ownership); [manual construction](../rendering/pipelines-and-materials.md#manual-material-construction). |
| {cpp:class}`Renderer <CE::Rendering::Renderer>`, {cpp:class}`PresentationSurface <CE::Rendering::PresentationSurface>`<br>Consume completed frames and present through compatible graphics/platform adapters. [Adapter catalogue](../architecture/interfaces-adapters.md#runtime-adapters); [OpenGL owner](../modules/opengl.md). |

### Draw values and enums

| Type | Choices and details |
| --- | --- |
| {cpp:enum}`Materials::Builtin2D <CE::Rendering::Materials::Builtin2D>` | `Textured`/`TexturedStrip` supply ordinary asset defaults; `SolidColor`/`TexturedColor` supply colored UI triangles. [Default asset materials](../rendering/pipelines-and-materials.md#default-asset-materials); [default UI materials](../rendering/pipelines-and-materials.md#default-ui-materials). |
| {cpp:enum}`Pipelines::Blend <CE::Rendering::Pipelines::Blend>` | `Opaque`, `StraightAlpha`, `PremultipliedAlpha`, `Additive`; match source color/alpha convention. [Pipeline and material state](../rendering/pipelines-and-materials.md#parameters-and-ownership). |
| {cpp:enum}`Pipelines::Depth <CE::Rendering::Pipelines::Depth>`, {cpp:enum}`Pipelines::Cull <CE::Rendering::Pipelines::Cull>` | Depth comparison and face culling; camera dimensionality does not select these policies. [Fixed state and pass constraints](../rendering/pipelines-and-materials.md#parameters-and-ownership). |
| {cpp:enum}`Geometry::Layout2D <CE::Rendering::Geometry::Layout2D>`, {cpp:enum}`Geometry::Topology <CE::Rendering::Geometry::Topology>` | Match position/UV or colored vertices and triangles/triangle strips to the pipeline. [Geometry and independent binding](../rendering/pipelines-and-materials.md#independent-binding). |
| {cpp:enum}`Parameters::Type <CE::Rendering::Parameters::Type>`, {cpp:enum}`Parameters::Semantic <CE::Rendering::Parameters::Semantic>` | Declare parameter value types and custom/pass/draw semantics. [Typed parameters](../rendering/pipelines-and-materials.md#parameters-and-ownership). |
| {cpp:struct}`ClipRegion2D <CE::Rendering::ClipRegion2D>` | Logical rectangular clip retained with ordered draw state. [Rectangular clipping](../rendering/pipelines-and-materials.md#rectangular-clipping). |

## Usage

Prepare draws from stable simulation state, then let Runtime publish and play
the completed frame. The ordinary sprite path needs a retained asset, copied
draw style and pass values; it needs no native graphics calls in the game hook.

```{engine-diagram} rendering-focus-sprite
```

The [frame playback diagram](diagram-explorer.md#rendering-focus-playback)
follows the completed frame through Runtime to the renderer. The
[application map](diagram-explorer.md#rendering-application) adds cameras,
materials and other visual inputs.

### Update, prepare and play

| Stage | Application/engine work | Values crossing the boundary |
| --- | --- | --- |
| Game update | Advance authoritative positions, cameras and animation on simulation. | Stable current state for the next preparation call. |
| `prepare_render_frame()` | Borrow `FrameWriter`, begin passes, resolve complete packets and add them in draw order. | Copied matrices/styles/parameters and retained resource handles. No game-object pointer is needed by playback. |
| Runtime playback and recycling | Graphics consumes a completed `Frame`, presents it and releases recycled frame owners. | Earlier generations stay alive while any frame, scene or application handle retains them. |

Preparation may be skipped under concurrent backpressure. Advance essential
state during update so skipped visual preparation cannot stop animation or
change gameplay. See [frame publication](../runtime/runtime-frame-boundary.md#frame-publication-and-recycling).

### Uses

| Consumer | Dependency | Integration boundary |
| --- | --- | --- |
| `Submission::resolve_sprite()`, `resolve_tile()` and `resolve_graphic()` | Retained [Assets](assets.md#usage) and {cpp:struct}`Rendering::DrawStyle2D <CE::Rendering::DrawStyle2D>` | Resolve geometry ranges, image bindings, material and copied transform without uploading or advancing playback. |
| `Submission::resolve_text()` | {cpp:class}`Text::Rendering::Generation <CE::Text::Rendering::Generation>` from [Text](text.md#usage) | Resolve already uploaded glyphs; no font discovery, shaping or rasterization is repeated. |
| {cpp:struct}`Rendering::DrawPacket2D <CE::Rendering::DrawPacket2D>` | [Resources](resources.md#usage): geometry, material and resolved image/sampler parameters | Retain exact immutable generations, including when the original game object is gone. |
| Renderer and presentation adapters | {cpp:class}`Rendering::PresentationSurface <CE::Rendering::PresentationSurface>` and [Display](display.md) | Native drawing/presentation stays on the graphics/platform owner with compatible live contexts. |

### Used by

| Consumer | Entry point | Responsibility |
| --- | --- | --- |
| Application game implementing {cpp:struct}`Game::AbstractGame <CE::Game::AbstractGame>` | `prepare_render_frame()` and its borrowed {cpp:class}`Rendering::FrameWriter <CE::Rendering::FrameWriter>` | Select visible objects, pass order and copied camera state. Do not retain frame/pass writers. |
| [GUI](gui.md#usage) `UI::TGUI::Scene` and `UI::RmlUi::Scene` | `Scene::write(frame)` | Append an uploaded UI pass at the application's chosen position. Rendering playback does not query live widgets or documents. |
| {cpp:class}`Runtime::GameRuntime <CE::Runtime::GameRuntime>` | Complete frame publication, renderer playback and recycling | Coordinate execution and resource release across the simulation/graphics boundary. |

### Choose a submission path

| Visual | Normal path | Application choice |
| --- | --- | --- |
| Sprite or atlas tile | `resolve_sprite()` / `resolve_tile()` | Current grid cell and per-object style; an empty material uses the asset's retained default. |
| Whole image | `resolve_graphic()` | Retain `Assets::Graphics::Asset`; use its independent-triangle geometry. |
| Unicode label | `resolve_text()` | Retain the uploaded text generation. [Positioned text runs](../assets/text-layout.md#positioned-text-runs) are an explicit alternative with a compatible shader contract. |
| Toolkit UI | `Scene::write(frame)` | Adopt a complete scene before preparation; choose its order relative to world/HUD passes. |

[CPU submission](../rendering/pipelines-and-materials.md#cpu-submission) defines
image insertion and parameter precedence. Camera dimensionality does not select
blend, depth or culling policy; those belong to the material's pipeline and pass
constraints.

## Contracts

Frame/pass writers borrow a runtime-owned slot for one preparation call. Do not
retain them or issue native graphics operations from preparation. Draw packets
resolve resources, transforms and parameters before publication; playback does
not need the source game object or animation cursor. Pipelines supply complete
validated fixed state and material parameter bindings must match their contracts.

Camera matrices are borrowed references until copied into a pass. Camera mutation
is unsynchronized. Frame preparation can be skipped under concurrent backpressure;
advance gameplay/animation during update rather than while drawing.

## Lifecycle

Create/upload resources during platform-owned initialization or dispatched
upload work. Update camera and authoritative state on simulation. Prepare,
publish, consume and recycle frames through runtime. Recycling drops retained
packet generations on graphics; game cleanup precedes backend closure.

## Execution

Update and frame preparation share the selected simulation owner. Native draw,
presentation and retirement maintenance run on graphics. Concurrent mode
transfers complete slots; it does not grant the renderer access to mutable
simulation members. The newest completed frame may supersede an older waiting
frame without skipping authoritative game updates.

## Integration

A preparation hook begins a pass with copied camera values:

```cpp
auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
const CE::Rendering::Submission::Context2D context{
    pass.semantics(), pass.parameters(), pass.constraints()
};
pass.add(CE::Rendering::Submission::resolve_sprite(*sprite_, cell_, style_, context));
```

The application supplies the retained sprite and valid cell. `Context2D` inserts
the ordinary `image` binding on texture unit zero by default; use an
[explicit image override or manual binding](../rendering/pipelines-and-materials.md#cpu-submission)
only when the material needs it. An empty
`style_.material` uses the compatible material retained during loading; no
application shader setup is needed for ordinary drawing. Set a material and
custom parameters in the style when an object needs a different appearance; see
[per-object materials](../rendering/pipelines-and-materials.md#give-one-object-a-custom-material).
See [First Game](../getting-started/first-game.md#prepare-a-frame-publish-the-actors-current-appearance)
for complete initialization, transform and cleanup wiring.

## Limits and further reading

Cameras do not choose a scene policy or synchronize game state. World/entity
organization belongs to the application. Read
[pipelines, materials and submission](../rendering/pipelines-and-materials.md),
[frame ownership](../runtime/runtime-frame-boundary.md),
[resource retirement](../resources/resource-residency.md),
[camera task guide](../getting-started/how-to/use-camera.md) and
[Frame interfaces](../../../projects/engine/include/cheryl/core/rendering/render-frame.h).
