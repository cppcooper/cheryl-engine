# Your first game

Build a small game that opens a window, moves a sprite with the arrow keys,
changes its image with Space, and displays `Score: 0`. Escape closes the window.
The score stays at zero while you establish the application and resource flow.

Start with setup and `main()`, then learn which parts of the engine your game
uses and who owns its state. The sprite and text walkthroughs apply those rules
from initialization through drawing and cleanup. The main walkthrough excerpts
come from the maintained [first-game application](../../../projects/apps/first-game),
with the enclosing function and thread named above each excerpt. The
screen-coordinate section extends that example; the linked text-update guide
adds a changing score.

Complete the sprite, static text and cleanup steps first. They form the starting
application for [Create a PlayerController](create-a/controller.md). Screen-space text
and [loading during play](#load-or-replace-resources-during-play) are optional
extensions when those tasks become relevant.

## Setup

Follow [Project setup](project-setup.md) for dependencies, a game CMake project,
source or SDK consumption, and the commands that produce libraries, an SDK, or
documentation. Building your game against a source checkout also builds the
libraries it needs; a separate SDK build is optional.

To run the maintained example in the checkout first, use this block from anywhere
inside that checkout. It selects the native graphics/input composition and the
first-game executable:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/first-game -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCHERYL_BUILD_FIRST_GAME=ON -DCHERYL_BUILD_DEMO=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_BUILD_OPENGL=ON \
    -DCHERYL_BUILD_STARTUP=ON -DCHERYL_STARTUP_OPENGL=ON -DCHERYL_BUILD_WORKERS=OFF \
    -DCHERYL_NATIVE_INPUT=ON -DCHERYL_NATIVE_NULL_PLATFORM=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
    -DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF -DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=OFF \
    -DCHERYL_BUILD_TESTS=OFF -DCHERYL_BUILD_CONSUMER_TESTS=OFF \
    -DCHERYL_BUILD_SDK=OFF -DCHERYL_BUILD_DOCS=OFF \
    -DGLFW_BUILD_WAYLAND=OFF -DGAINPUT_ENABLE_HID=OFF
  cmake --build build/first-game --target first-game --parallel
  ./build/first-game/first-game
)
```

The policy setting accommodates pinned legacy dependencies on CMake 4 hosts.
The GLFW setting chooses X11 on Linux; the Gainput setting avoids the optional
HID dependency download. See the [build reference](../development/building.md)
for other platform choices. Windows executables have an `.exe` suffix.

The example's PNG and manifests live in its own `assets` directory; the default
material shaders are embedded in the OpenGL library and the
embedded fallback font comes from the pinned assets submodule. No optional
artwork downloads are needed. Expect a 640 × 360 window with a cyan sprite and
the score label. Check movement, Space changing the sprite to amber, resizing,
and Escape on the desktop; compiling or displaying command-line help alone does
not establish those graphics and input observations.

## Main: configure and start the engine

### Startup options, including your own

{cpp:func}`CE::Application::make_startup` is a **factory function**:
it constructs and returns a {cpp:class}`CE::Application::Startup` object. The
example stores that object in the variable `startup`. It holds the command-line
parser and the configuration used to create the engine context.

With this build's configured Startup integration, the function selects GLFW for the window/input implementation and OpenGL for
drawing and graphics resource creation. These implementations are the
**backends**; their objects implement the engine's window, input, rendering, and
resource interfaces. Register your game's command-line options on `startup`
before calling `startup.initialize()`:

```{literalinclude} ../../../projects/apps/first-game/src/main.cpp
:language: cpp
:caption: main() — platform thread; owners live through runtime.run()
:start-after: // [startup]
:end-before: // [/startup]
:dedent: 8
```

In `make_startup("Cheryl first game", {...})`, the first argument is the text
shown in command-line help. The second argument, `{.width = 640, ...}`, is a
`CE::Application::ApplicationOptions` configuration value: it supplies defaults
such as window size and title.

The calls in `main()` act on different objects:

| Call in the example | Object and role |
| --- | --- |
| `startup.add_application_options(...)` | A `Startup` member function. It immediately calls the supplied lambda to register your game's options. |
| `options.add_option("--assets", asset_root, ...)` | A CLI11 member function. The lambda's `CLI::App& options` refers to the **Application option group** within `startup`'s parser. This call registers `--assets` and binds its value to `asset_root`. |
| `startup.initialize(argc, argv)` | Parses the command line, creates the engine context, and returns the `StartupResult` stored in `result`. |
| `result.make_runtime(game)` | Creates the `GameRuntime` stored in `runtime`, using the parsed settings and your `game` object. |
| `runtime.run()` | Runs the engine loop and calls your game's lifecycle functions. |

The callback's `options` parameter is a parser interface; the `Options` value
passed to `make_startup()` is configuration data. Cheryl also registers its own
engine/backend options: use `--help` to see those available in this build.
For example, `--window-width` overrides the initial width, `--worker-count`
controls CPU worker capacity, and `--concurrent` selects the separate simulation
thread.

When `startup.initialize()` parses `--assets`, it writes into the `asset_root`
string captured by the lambda. Keep that string alive through parsing. CMake
supplies its default as an absolute path, so launching from another directory
still finds the same assets. An explicit relative override resolves from the
process's working directory. Add other game-specific options in that callback
and keep their destination variables alive until `startup.initialize()` returns.

### Configure the context and run the game

The second argument to `make_startup()`, the
{cpp:struct}`CE::Application::ApplicationOptions` value,
selects the initial window dimensions, title, window mode, and swap interval.
Its `execution` member configures the context's CPU execution service, including
worker count and an optional shared pool of threads for background tasks.
Its `runtime` member is a {cpp:struct}`CE::Application::RuntimeOptions` value
supplying loop defaults such as execution mode and simulation timing. Parsed command-line options
override these defaults.
See [application startup](../development/consuming-engine.md#command-line-startup)
for the complete option and validation contract.

`result` is a {cpp:struct}`CE::Application::StartupResult`.
On successful initialization, its `result.context` member owns a
{cpp:class}`CE::Runtime::Context`, the object grouping the chosen engine services
and backend implementations. The GLFW window exists at this point. Help and
argument errors return without a context: check `result.should_start()` before
using it and return `result.exit_code`. Backend creation failures can throw; the
complete example wraps `main()`'s body in an exception handler.

Construct the `FirstGame::Game` instance named `game` with `*result.context`.
`result.make_runtime(game)` returns a {cpp:class}`CE::Runtime::GameRuntime`,
stored in `runtime`, with the parsed loop settings. `runtime.run()` starts the
rendering and input implementations, calls the game's lifecycle functions, runs
until stopped, and shuts rendering and input down. Uploading graphics resources
becomes available inside `Game::init_platform()`.

Declare `result`, `game`, and `runtime` in that order. Here, **owns** means being
responsible for an object's lifetime; **borrows** means using an object that
someone else must keep alive. The game borrows the context, and the runtime
borrows both. Reverse destruction order keeps those dependencies alive. The
context provides services; the runtime controls the loop that calls your game.

## Engine architecture: the game developer's view

### Game state and lifecycle

In a normal session, the hooks have this lifetime order. Updates and frame
preparation repeat within the active phase; rendering independently consumes
complete published frames.

```mermaid
flowchart TB
    accTitle: First Game hook lifetime
    accDescr: Initialize game state while adapters are live, run updates and frame preparation, stop and join simulation, quiesce producers, settle accepted work, then release game resources during deinit.
    init["Game init"] --> active["Updates and frame preparation"]
    active -->|stop and join simulation| quiesce["Game quiesce"]
    quiesce --> settle["Settle accepted work"]
    settle --> deinit["Game deinit"]
```

{cpp:struct}`CE::Game::AbstractGame` is the base interface for a game. Derive your
own class from it and implement its virtual member functions, called **hooks**
because the runtime calls them at defined points in the lifecycle. The example's
implementation is `FirstGame::Game`, the class instantiated as `game` in `main()`:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.h
:language: cpp
:caption: FirstGame::Game — initial state is prepared in init_platform(), then managed by simulation
:start-after: // [game]
:end-before: // [/game]
:dedent: 4
```

Its `engine_` member is a `CE::Runtime::Context&` referring to `*result.context`.
Thus calls such as `engine_.input()` use the context created in `main()`.
`camera_` stores the view, `actor_` stores the sprite actor, `score_` stores the
uploaded text label, and `images_` retains a material with nearest image sampling. The
sprite and text sections below introduce those resource types.

The **platform thread** is the thread calling `runtime.run()`. It polls native
events and owns the graphics context. The **simulation owner** runs your game
updates and frame preparation. By default, it is that same thread. With
`--concurrent`, it is one dedicated simulation thread; simulation initialization,
updates, frame preparation and simulation cleanup run on that owner.

| Stage, in lifecycle order | Owner | Your responsibility |
| --- | --- | --- |
| `init_platform()` | Platform/graphics thread, before simulation starts | Bind controls, acquire resources, and prepare initial game state. |
| `init_simulation()` | Simulation owner, with services and initial view ready | Construct simulation objects and attach services before Ready, timing and the first update. The default hook is enough for this example. |
| `update(tick)` | Simulation owner | Change game state using copied input, dimensions, and simulation time. Request a stop when appropriate. |
| `prepare_render_frame(frame) const` | Same simulation owner | Turn current state into a complete frame of draw values and retained resources. Keep gameplay progression in `update()`; frame preparation can be skipped when no frame slot is available. |
| `deinit_simulation()` | Simulation owner, after simulation dispatch closes and before it exits | Release state requiring this owner; keep dependencies of active producers or accepted work alive. The default hook is enough for this example. |
| `quiesce()` | Platform thread, after simulation stops and joins | Detach external listeners and stop producers that could submit more work. The default hook is enough for this example. |
| `deinit_platform()` | Platform/graphics thread, after submitted background work finishes and frames are released for reuse | Remove bindings and release game-owned resources while input and graphics still exist. |

This example prepares its initial state during `init_platform()`; work that needs
simulation ownership belongs in `init_simulation()`. Both initialization and
deinitialization hooks default to doing nothing. Use the constructor for setup
that needs neither initialized owner. During the loop, simulation owns changes
to game state. Rendering reads a
**published frame**: a complete set of drawing instructions for one window
image, containing copied camera/draw values and retained resources.
After your frame-preparation hook returns, the runtime publishes the completed
frame.

That boundary lets native events and graphics stay on their required thread
while simulation can run independently. Reading a live actor position from the
renderer while simulation changes it would be a data race. Instead, the renderer
can draw an earlier frame's copied position while the actor moves again.
The sprite/material **handles** in this example are `shared_ptr` values: holding
one keeps the referenced resource alive. That does not make concurrent mutation
safe.
Publish a new resource version when its contents change.

### Interfaces you use

`engine_` gives the game access to these interfaces. An **interface** describes
the operations an implementation provides; for example, `engine_.resources()`
returns a `CE::Rendering::Resources::Provider&`. That **resource provider** creates
the backend's graphics resources from prepared data. Later snippets store this
reference in the local variable `resources`.

| Interface | When and why your game uses it |
| --- | --- |
| `engine_.input()` | Bind actions in `init_platform()` and unbind them in `deinit_platform()`. Read gameplay input from `tick.input` during updates. |
| `engine_.window()` | Read native window state on the platform thread. During updates, use `tick.framebuffer_size`, the copied dimensions for simulation. |
| `engine_.resources()` | Obtain the resource provider for uploads on the graphics owner, initially in `init_platform()`. |
| `engine_.fonts()` | Obtain the shared font catalogue. Use its default font or configure named families once before creating text/UI consumers. |
| `tick`, the `TickContext` parameter of `update()` | Read the simulation delta/input/dimensions and call `tick.request_stop()`. Its borrowed data is valid for that update. |
| `frame`, the `FrameWriter` parameter of `prepare_render_frame()` | Build drawing instructions on the CPU. Do not retain the writer or passes after the hook returns. |
| `engine_.make_worker_group()` and `engine_.platform_dispatcher()` | Later, prepare expensive resources on background CPU threads and queue upload requests for the platform thread. |

Calling a context accessor does not move execution to another thread. In
particular, graphics uploads in concurrent `update()` would run on the wrong
thread. Your game supplies frame data; the runtime calls the renderer and
presents the window. The
[context assembly](../development/consuming-engine.md#context-assembly) and
[frame boundary](../runtime/runtime-frame-boundary.md) describe the wider design.

### Obtain, use, and stop using resources

Separate shared resource contents from the mutable state that uses them. A
sprite contains image/grid data; each actor owns its position and selected cell.
A text generation contains an uploaded label; the game chooses its position and
which generation to submit. Keep those choices with simulation.

```mermaid
flowchart LR
    accTitle: Copy state and retain appearance for drawing
    accDescr: Frame preparation copies actor or label placement and retains selected resource handles in draw packets. Platform rendering consumes those packets rather than reading mutable game state.
    state["Game position and cell"] -->|copy values| packets["Frame packets"]
    handles["Sprite or text handles"] -->|retain resources| packets
    packets -->|playback| renderer["Platform renderer"]
```

| Resource | Obtain | Use | Stop using |
| --- | --- | --- | --- |
| Sprite and material | Load graphics assets through the provider; the sprite retains a compatible default material. | Simulation chooses a cell/transform and optional material; frame preparation submits those values and retains the resources. | Stop submitting them and reset the game's handles during cleanup or replacement. |
| Font and rendered text | Ask the engine for its default font, lay out the message, prepare CPU images/triangles, then upload a text generation with its default material. | Retain the generation and resolve it into each frame that displays the label. | Replace or reset the generation; local font/layout variables need not remain in scope. |
| Sound effect or music | With the optional audio module, decode a clip, then play it through an application-owned audio system; or open a stream there. | Playback voices run on the audio system's own timeline; they do not pass through rendering frames. | Explicitly stop a voice when playback should end; dropping a voice handle alone does not stop it. Close the audio system after its producers stop. |

Releasing a game handle ends that owner's use. The shared asset caches and
published frames may still retain the resource, so this does not immediately
delete its GPU textures or buffers. Do not clear a shared cache just because an
actor disappears. Keep the backend that created the resources alive until their
references are released and backend cleanup finishes; runtime shutdown provides
that ordering for this game's graphics. See
[resource ownership](../resources/resource-residency.md) and
[audio playback](../runtime/audio.md) for their respective lifetime contracts.

## Prepare controls and a drawing pass

These are shared foundations for the sprite and text examples below.

### Bind controls during initialization

An **action** is a game-defined input name represented by a
`CE::Input::Actions::Id`; here, `left_`, `right_`, `next_cell_`, and `quit_` are
those IDs. A **binding** maps a device input to an action. In the following code,
`bindings` is the binding interface returned by `engine_.input().bindings()`.
Its `bind_button()` calls map Cheryl keyboard controls to the game's actions and return
binding IDs, stored in the `bindings_` member for cleanup:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:caption: Inside Game::init_platform() — platform thread
:start-after: // [bindings]
:end-before: // [/bindings]
:dedent: 8
```

Updates read the action IDs (`left_`, `right_`, `next_cell_`, `quit_`) rather than
native key codes. See the [input guide](../runtime/input-state-model.md) for other
actions, events, and focus handling.

### Understand what a draw needs

A sprite or uploaded label supplies geometry (vertices forming shapes) and image
pixels. A **material**, represented by `CE::Rendering::Resources::Material`,
supplies the drawing recipe: a shader program to run on the GPU, how vertices
form triangles (topology), how image pixels are sampled, and named parameters.
The provider supplies compatible defaults when loading sprites and uploading
text. This example requires no shader files or material manifest:

| Resource | Default material |
| --- | --- |
| Actor sprite | Four-vertex triangle strips, retaining the image's sampling defaults. This game chooses nearest sampling to keep its pixel art sharp. |
| Uploaded score | Six-vertex glyph quads as triangles, with linear sampling for the text's alpha atlas. |

The camera converts Y-up pixel coordinates into the renderer's view. Initialize
it from the actual framebuffer dimensions before placing the actor:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:caption: Inside Game::init_platform() — platform/graphics thread, before bindings and asset loading
:start-after:     void Game::init_platform() {
:end-before:         // [bindings]
:dedent: 8
```

Updates will keep `camera_`'s dimensions current using the tick's copied size.
In `prepare_render_frame(CE::Rendering::FrameWriter& frame)`, `frame` is the
runtime's borrowed **frame writer**, used to assemble this frame's instructions.
`frame.begin_pass(...)` returns the local `pass`, a
`CE::Rendering::PassWriter`. A **pass** groups draws that share camera and pass
settings. The local `context` is a `CE::Rendering::Submission::Context2D` value
containing those settings for the draw-resolution functions below:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:caption: Inside Game::prepare_render_frame() — simulation owner; common to both draws
:start-at:         auto pass = frame.begin_pass
:end-before:         CE::Rendering::DrawStyle2D image_style;
:dedent: 8
```

`frame.begin_pass()` copies `camera_`'s projection and view matrices, which map
game coordinates to the window. The draw-settings value `context` is separate
from the engine's `CE::Runtime::Context` referenced by `engine_`. By default,
resolution inserts each asset's image using the standard material parameter
`image` and texture binding slot zero.

```mermaid
flowchart LR
    accTitle: A frame contains passes and draw packets
    accDescr: A frame writer orders passes, and each pass writer orders draw packets sharing its camera and constraints.
    frame["Frame"] --> passes["Ordered passes"] --> draws["Ordered draw packets"]
```

A pass is a shared rendering boundary. It can require several GPU draw calls.
Draws in it can overlap; visibility depends on their order,
depth settings and blending. Separate world and HUD passes give each its own
camera and constraints; the game chooses their order.

For each draw, the local `image_style` or `text_style` is a
{cpp:struct}`CE::Rendering::DrawStyle2D` value. It supplies an optional material,
the model transform (position/orientation), scale, and alpha (opacity). Resolution
combines an asset, a style, and the pass settings into a
`CE::Rendering::DrawPacket2D`: an owned drawing instruction. The following two
walkthroughs add those packets to this same `pass` with `pass.add(...)`.

With no material in the style, resolution uses the resource's retained material.
For pixel art, `images_` keeps the sprite's default pipeline and changes only
sampling; the text draw uses its uploaded generation's default directly.
Objects can share a sprite while choosing different materials. For a per-object
color tint, follow the [custom sprite material recipe](../rendering/pipelines-and-materials.md#give-one-object-a-custom-material).
Scale and alpha have standard draw semantics; a custom `tint` parameter must
match the selected material's declared contract. Keep the sprite-cell triangle-strip
topology; a UI material expecting colored triangles is not interchangeable.
See [pipelines and materials](../rendering/pipelines-and-materials.md) for further
JSON recipes and sampling overrides.

## Put a sprite on screen

The complete path is: **load image/grid/material → retain a sprite in an actor →
update position/cell → submit a draw → release the actor and material**.

### Init: load assets and create the actor

Keep this game's resources under one controlled asset root:

```text
assets/
└── graphics/
    ├── graphics-manifests.json
    ├── definitions/sprites.json
    └── textures/actor.png
```

`graphics-manifests.json` is an **index**: it lists the JSON definition files
the loader should read. Each listed **manifest** describes named assets or
drawing materials:

```{literalinclude} ../../../projects/apps/first-game/assets/graphics/graphics-manifests.json
:language: json
:caption: graphics-manifests.json — index for this asset root
```

Only files named `graphics-manifests.json` select definitions automatically;
unlisted JSON files are ignored. Paths in those definitions resolve from their
enclosing `graphics` directory. `$schema` URLs help the editor validate JSON;
runtime loading uses the local files.

The sprite definition divides a 32 × 16 image into two 16 × 16 cells:

```{literalinclude} ../../../projects/apps/first-game/assets/graphics/definitions/sprites.json
:language: json
:caption: Sprite definition — two selectable cells with a centered pivot
```

An actor retains that shared sprite while keeping its position and current cell
as game-owned values:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.h
:language: cpp
:caption: Actor — resource handle plus per-actor simulation state
:start-after: // [actor]
:end-before: // [/actor]
:dedent: 4
```

The local `loader` is a `CE::Assets::Loading::Loader` that reads this asset root.
`resources` refers to the resource provider from `engine_.resources()`.
`Sprites::Manager::get()` accesses the shared sprite cache. Load the root,
check the required sprite, select nearest sampling and construct `actor_`:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:caption: Inside Game::init_platform() — platform/graphics thread
:start-after: // [assets]
:end-before: // [/assets]
:dedent: 8
```

`load_assets()` prepares CPU data and uploads through the active resource
provider. It loads indexed graphics definitions and standalone PNGs under the
root; fonts and audio have separate paths. Keep the root limited to this game's
assets, so loading does not also select unrelated or optional artwork.
`register_files()` alone only registers paths; it does not perform this loading.
An upload failure can leave already completed cache entries; the
[asset-loading guide](../assets/asset-loading.md) explains validation and retry.

The sprite manifest's `"namespace": "first-game"` groups asset names; its
`"sprites"` object contains the local name `"actor"`. Together they identify
`first-game:actor`. This asset namespace is a naming group within the loaded
resources. `get_asset("first-game", "actor")` returns its shared sprite handle
or null. `get_assets("first-game")` returns that group keyed by local name when a
game system needs several sprites. Resolve names once and
pass handles into the owning actors/managers; collection iteration order is not
gameplay identity. The local loader can go out of scope after initialization.
The actor's retained handle remains valid within its backend's lifetime.

### Update: move the actor and choose a cell

```{literalinclude} ../../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:caption: Game::update() — simulation owner, once per simulation step
:start-after: // [update]
:end-before: // [/update]
:dedent: 4
```

Movement uses `tick.delta_seconds`. The camera follows `tick.framebuffer_size`
without reading the live native window. Escape requests normal runtime shutdown.
Space advances the actor's selected image cell; the shared sprite stays unchanged.

This asset has no timed animation clip. For authored clips, keep a separate
{cpp:class}`CE::Assets::Sprites::Animation` object per actor to hold its playback
position, and advance it with simulation time. See
[asset values and playback](../assets/asset-values-and-playback.md).

### Prepare a frame: publish the actor's current appearance

Use the common pass above to submit the current cell and transform:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:caption: Inside Game::prepare_render_frame() — simulation owner
:start-at:         CE::Rendering::DrawStyle2D image_style;
:end-before:         CE::Rendering::DrawStyle2D text_style;
:dedent: 8
```

{cpp:func}`CE::Rendering::Submission::resolve_sprite` builds CPU draw packets.
`pass.add()` copies the draw values and retains the geometry, material, and
selected image in the frame. After publication, the platform renderer draws
those packets and presents the window. The actor can move again without changing
the submitted frame.

### Deinit: release this game's use

On the platform thread, `actor_.reset()` releases the actor and its sprite handle;
`images_.reset()` releases the game's image material handle. They appear in the
[complete cleanup hook](#finish-the-game-and-release-resources) below, together
with input and text cleanup. Earlier frames and cache ownership follow the
resource lifetime rules already described.

## Put text on screen

Start with a UTF-8 `std::string`, such as `std::string message = "Score: 0"`.
The complete path is: **choose fonts → choose/position glyphs → build their
images and triangles → upload → submit a draw → release the uploaded label**.
The string literal in this example follows that same path.

### Init: turn the message into a drawable label

`engine_.fonts().default_font()` returns a retained `CE::Text::Font` from the
engine catalogue. Without configuration it uses the embedded safety font.
The calls below produce these successive values:

| Stage | What it produces and why |
| --- | --- |
| `engine_.fonts().default_font()` | `font`, a `CE::Text::Font` retaining the selected family and its glyph fallbacks. Configure the catalogue once to change the default for Engine text and both GUI integrations. |
| {cpp:func}`CE::Text::layout_text` | `layout`, a `CE::Text::ShapedText` describing **which glyphs to draw and where to put them**, at the requested font size. A glyph is a font's drawing for a letter, symbol, or group of characters. Choosing glyphs and adjusting their spacing is called **shaping**; letters can join or share a glyph. Layout also handles direction and line breaks. It produces no pixels. |
| {cpp:func}`CE::Text::Rendering::prepare` | A `CE::Text::Rendering::Prepared` value: CPU images of the needed glyphs packed into sheets called **atlas pages**, plus triangles selecting each glyph's image. This turns font outlines into drawable pixels. |
| {cpp:func}`CE::Text::Rendering::upload` | A {cpp:class}`CE::Text::Rendering::Generation`, stored in the game's `score_` member, holding the layout and graphics resources. In this application, images and triangles become OpenGL textures and buffers. |
| `resolve_text()` and `pass.add()` below | `DrawPacket2D` drawing instructions combining that uploaded label with a material and position, added to `pass`. The runtime's renderer executes the published frame to display the text. |

The initialization code follows those stages. `resources` is the provider
reference obtained in the asset-loading excerpt:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:caption: Inside Game::init_platform() — platform/graphics thread
:start-after: // [score]
:end-before: // [/score]
:dedent: 8
```

These stages let you reuse prepared text while changing its placement. Layout
and preparation are CPU work; upload must run on the graphics owner, as it does
here during `init_platform()`. Keep the resulting `score_` in the game for simulation to
select and publish in later frames. Local font/layout variables can leave scope;
the generation retains the data and resources it needs.

To use application or installed fonts, configure `engine_.fonts()` before
creating text or UI consumers. Give families persistent names and select a
default once; individual books, labels or widgets can still select another font.
See the [font catalogue](../assets/text-layout.md) for configuration, random
group selection, saving a chosen reference and fallback behavior.

### Update: retain or replace the label

This example's message is static, so `update()` keeps the same `score_`. Moving
the label changes only its draw transform. Changing the message, font or layout
requires a new prepared and uploaded generation; changing the original string
alone does not update an uploaded label.

For text that changes during play, use `CE::Text::Rendering::Updater`.
[Display and update text](how-to/update-text.md) shows the complete member,
initialization, update, submission and cleanup changes for a changing score.
The updater handles background preparation and platform upload while retaining
the last complete label. That guide extends this direct text path; the
[TGUI HUD tutorial](create-a/hud.md) teaches toolkit widgets.

### Prepare a frame: place and publish the label

Add the uploaded label to the same camera/pass used for the sprite:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:caption: Inside Game::prepare_render_frame() — simulation owner
:start-at:         CE::Rendering::DrawStyle2D text_style;
:end-at:         pass.add(CE::Rendering::Submission::resolve_text
:dedent: 8
```

The model transform places the label's origin 24 pixels from the left and 48 below
the framebuffer's top in the Y-up camera. The generation's default material draws its
glyph triangles with the uploaded atlas images.
{cpp:func}`CE::Rendering::Submission::resolve_text <std::vector<DrawPacket2D> CE::Rendering::Submission::resolve_text(const CE::Text::Rendering::Generation&, const DrawStyle2D&, const Context2D&)>`
performs CPU packet construction; it does not upload or draw. `pass.add()`
retains the selected resources and copies parameters into the frame. After the
hook returns, the runtime publishes the complete frame for the platform renderer
to draw and present.

### Keep text in screen coordinates

The example's camera has an identity view, so the sprite and text can share one
pass. If a later game pans the world view, give direct screen text a separate
`CE::Rendering::Camera2D hud_camera_` member and keep its identity view.
Initialize its size during platform-owned `init_platform()`:

```cpp
hud_camera_.set_framebuffer_size(engine_.window().framebuffer_size());
```

Keep the copied size current during `update()`:

```cpp
hud_camera_.set_framebuffer_size(tick.framebuffer_size);
```

Replace the existing direct text draw with a separate pass after the world draw
in `prepare_render_frame()`:

```cpp
auto text_pass = frame.begin_pass(hud_camera_.projection_matrix(), hud_camera_.view_matrix());
const CE::Rendering::Submission::Context2D text_context{
    text_pass.semantics(), text_pass.parameters(), text_pass.constraints()
};
CE::Rendering::DrawStyle2D text_style;
text_style.model_matrix =
    glm::translate(glm::mat4(1.0f), glm::vec3(24.0f, static_cast<float>(hud_camera_.framebuffer_size().height) - 48.0f, 0.0f));
text_pass.add(CE::Rendering::Submission::resolve_text(*score_, text_style, text_context));
```

The label remains 24 pixels from the left and 48 below the framebuffer's top
while the world camera moves. Resize observations update the projection and
placement. The separate camera changes where text is drawn; text content still
uses [generation replacement](how-to/update-text.md). Toolkit scenes provide
their own screen projection, as shown in [Create a HUD with TGUI](create-a/hud.md) and
[Create an interactive GUI with RmlUi](how-to/create-gui.md).

### Deinit: release the uploaded label

`score_.reset()` releases this game's generation, including its material,
on the platform thread during the cleanup hook below. Frames and caches
can still retain their resources, as described in the ownership section. See the
[Unicode text guide](../assets/text-layout.md) for wrapping and more advanced
replacement behavior.

## Finish the game and release resources

The full cleanup hook removes the bindings and releases both examples' handles:

```{literalinclude} ../../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:caption: Game::deinit_platform() — platform/graphics thread, after simulation and accepted work finish
:start-after: // [cleanup]
:end-before: // [/cleanup]
:dedent: 4
```

Cleanup also follows attempted initialization, so tolerate resources that were
never created. The runtime stops and joins simulation, calls `quiesce()`, settles
accepted work while servicing required platform dispatch, recycles frames, and
calls `deinit_platform()` before input and graphics shutdown.

This game uses the default `quiesce()` because it has no external producers.
When adding listeners or background services, detach/stop those producers there
while their dependencies still exist. Do not wait in `quiesce()` for work that
needs platform dispatch; settling accepted work follows that hook. The
[frame-boundary guide](../runtime/runtime-frame-boundary.md) defines the complete
shutdown and failure ordering.

## Continue from the working game

The introductory game is complete when arrows move the actor, Space changes its
cell, `Score: 0` remains visible through resizing, and Escape closes cleanly.
Try the same controls with `--concurrent` to exercise the separate simulation owner.
Continue with [Actor controllers and commands](create-a/actor-controller.md),
then [Create a PlayerController](create-a/controller.md) and a Character
and TGUI HUD, to organize this state into reusable application classes.

The section below extends resource loading during play. Use
[the text updater](how-to/update-text.md) for a changing direct-text score,
[a camera](how-to/use-camera.md) for a scrolling view, or
[audio playback](../runtime/audio.md) for sounds when your game needs them.

## Load or replace resources during play

The same resource path can span threads when preparation is expensive.
`engine_.make_worker_group()` returns a `CE::Workers::Group`, which tracks jobs
submitted to background CPU threads. `engine_.platform_dispatcher()` returns a
`CE::Runtime::PlatformDispatcher&`, the queue for work that must run on the
platform thread. Its submitted callback receives a `CE::Runtime::Context&` to
use on that thread; a worker-group callback takes no engine argument. Either
object's `submit(...)` returns a `std::future<T>`: a way to retrieve the eventual
result of that job.

1. **Simulation:** request a load through an Engine worker group and keep the
   current resource while the request is pending.
2. **CPU worker:** prepare owned data, such as `Loader::prepare()` results. Keep
   the loader and request inputs alive through their work.
3. **Platform/graphics thread:** receive the owned prepared value through
   `engine_.platform_dispatcher().submit(...)`, upload it, and return retained
   resources through that request's future.
4. **Simulation, during `update()`:** check readiness and adopt the completed
   result. Later frames select the new version; earlier frames retain the old one.

Pass owned request data across that boundary. A platform upload callback should
return a result rather than mutate live simulation state through a captured
`this`. Avoid blocking the thread needed to complete the upload. Tracked work and
the runtime shutdown sequence keep the graphics owner available while accepted
requests settle.

Follow the working sequences in [asset loading](../assets/asset-loading.md),
[worker execution](../runtime/worker-execution.md), and
[thread dispatch](../runtime/thread-dispatch.md) when adding this flow. For changing
labels, the [text updater](how-to/update-text.md) already manages this handoff.
