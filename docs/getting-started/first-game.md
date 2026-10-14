# Your first game

Start with a small application that opens a window, loads its own sprite and
shaders, and draws the sprite and a score label. The Left and Right arrow keys
move the sprite, Space selects its other image cell, and Escape closes the
application. The score stays at zero while you establish the application and
resource flow.

The maintained source lives in
[projects/apps/first-game](../../projects/apps/first-game/). The excerpts below
come directly from that example, so you can inspect the complete application as
you work through each step.

## 1. Set up the checkout and build the application

Install Git, CMake 3.28 or newer, Ninja, and a C++23 compiler and standard library
with `std::format`. Follow the
[system dependency table](../development/building.md#system-dependencies) for
FreeType, HarfBuzz, ICU, and the native GLFW/OpenGL requirements. On Linux, this
example uses X11 and needs a desktop driver supporting an OpenGL 3.3 core context.
Python and Jinja2 are required when GLAD is generated during the source build.

Choose a checkout location and a revision matching the documentation you are
reading. Use `main` for stable documentation, `develop` for latest documentation,
or the corresponding release tag for a versioned manual. Replace
`/path/to/cheryl-engine` with your desired checkout location:

```sh
(
  set -e
  cheryl_version=main
  git clone --branch "$cheryl_version" https://github.com/cppcooper/cheryl-engine.git /path/to/cheryl-engine
  cd /path/to/cheryl-engine
  git submodule update --init --recursive
)
```

The example's PNG, shader sources, and manifests are checked in under its own
`assets` directory. The Engine's embedded fallback font comes from the pinned
assets submodule. This walkthrough needs no optional artwork downloads or asset
initialization script.

Run the following block from anywhere inside the checkout. It selects only the
native graphics/input composition and this application:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/first-game -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCHERYL_BUILD_FIRST_GAME=ON -DCHERYL_BUILD_DEMO=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_BUILD_OPENGL=ON \
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

`CMAKE_POLICY_VERSION_MINIMUM=3.5` accommodates the pinned legacy dependency
projects on CMake 4 hosts. `GLFW_BUILD_WAYLAND=OFF` selects the Linux X11 route;
`GAINPUT_ENABLE_HID=OFF` avoids the separate HID dependency download. See the
[build reference](../development/building.md) for other platform and toolchain
choices. Windows executables have an `.exe` suffix.

The expected result is a 640 × 360 window containing a cyan sprite and
"Score: 0". Space changes the sprite to amber. Check movement, cell selection,
resizing, and Escape on the desktop; compilation and command-line help alone do
not establish those graphics and input observations.

## 2. Start the engine and keep its owners alive

The application's entry point creates
{cpp:class}`CE::Engine::Startup` through
{cpp:func}`CE::Engine::make_glfw_opengl_startup`. That factory selects the native
GLFW window/input adapter and the OpenGL renderer and resource provider.

```{literalinclude} ../../projects/apps/first-game/src/main.cpp
:language: cpp
:start-after: // [startup]
:end-before: // [/startup]
:dedent: 8
```

Register your own options before calling `initialize()`. Here, `--assets` binds a
string that stays alive until parsing completes. Its default is an absolute path
supplied by the example's CMake target, so launching the executable from another
directory still selects the same assets. An explicit relative override resolves
from the application's working directory.

Help and argument errors return a {cpp:struct}`CE::Engine::StartupResult` with no
engine; return its exit code when `should_start()` is false. On success, construct
your game from the owned context and use `make_runtime(game)` to apply the parsed
execution, input polling, and timing options together.

Declare the result, game, and runtime in that order. The game borrows the
{cpp:class}`CE::Engine::EngineContext`, and the runtime borrows both. Their
declaration order keeps those owners alive through runtime destruction. Add
`--concurrent` when launching to place simulation on a separate thread; the same
game hooks and resource ownership rules apply.

## 3. Give the game its hooks and controls

Implement {cpp:struct}`CE::GFramework::AbstractGame` and keep the state your game
needs in that object. The example owns its camera, bindings, resources, and actor:

```{literalinclude} ../../projects/apps/first-game/src/first-game.h
:language: cpp
:start-after: // [game]
:end-before: // [/game]
:dedent: 4
```

| Hook | What belongs here |
| --- | --- |
| `init()` | Bind controls and create initial resources after the platform and graphics adapters start. |
| `update(tick)` | Advance game state using the supplied input and simulation delta. |
| `prepare_render_frame(frame) const` | Resolve that state into complete render submissions. |
| `quiesce()` | Stop external producers before final cleanup when your game adds them. |
| `deinit()` | Release game-owned resources and registrations after simulation and accepted work settle. |

`init()` and `deinit()` run on the platform/graphics owner. In concurrent mode,
`update()` and frame preparation run on the simulation thread. Bindings belong to
the platform owner, so define them during initialization:

```{literalinclude} ../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:start-after: // [bindings]
:end-before: // [/bindings]
:dedent: 8
```

The native key codes appear at this boundary. Gameplay reads the game's semantic
{cpp:struct}`CE::Input::ActionId` values instead. Retaining each returned binding
ID lets the game remove its own mappings during cleanup. The
[input guide](../runtime/input-state-model.md) covers additional actions, events,
and focus handling.

## 4. Load a controlled game asset directory

Put the resources required by this game under one root. The example uses:

```text
assets/
└── graphics/
    ├── graphics-manifests.json
    ├── definitions/sprites.json
    ├── shaders/materials.json
    ├── shaders/first-game.vert
    ├── shaders/first-game.frag
    └── textures/actor.png
```

The index selects the sprite and shader/material definitions:

```{literalinclude} ../../projects/apps/first-game/assets/graphics/graphics-manifests.json
:language: json
```

Only files named `graphics-manifests.json` select definitions automatically.
Unlisted JSON files are ignored. The listed paths, texture references, and shader
source references resolve from the enclosing `graphics` directory. The `$schema`
URLs support editor validation; runtime loading uses local files.

Create an owned {cpp:struct}`CE::Assets::Loader` and load the root during `init()`:

```{literalinclude} ../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:start-after: // [assets]
:end-before: // [/assets]
:dedent: 8
```

`load_assets()` performs CPU preparation followed by upload on the active
provider's loading owner. It processes indexed programs/materials, images,
sprites, and tilesets, along with standalone PNGs below the root. Fonts and audio
use separate loading paths. Preparation validates definitions and image bounds
before publication; an upload failure can leave already completed cache entries.

Use a controlled root containing just this game's required assets. Pointing the
general loader at the repository's larger asset collection also selects its
optional downloaded artwork. `register_files()` only registers paths for manual
loading; it does not parse definitions, decode images, or upload resources.
The [asset-loading guide](../assets/asset-loading.md) explains publication and
retry behavior.

Resolve required named resources during initialization and check each result.
{cpp:struct}`CE::Assets::SpriteMgr` returns a retained sprite handle or null;
{cpp:struct}`CE::Assets::ShaderAssetMgr` supplies the materials used for drawing.
For this manifest, `get_asset("first-game", "actor")` identifies
`first-game:actor` exactly. `get_assets("first-game")` provides a namespace
collection keyed by local names when your game needs several sprites.

## 5. Pass retained resources into game-owned state

The sprite is shared resource data. The actor owns its position and selected
cell, while retaining the sprite passed into its constructor:

```{literalinclude} ../../projects/apps/first-game/src/first-game.h
:language: cpp
:start-after: // [actor]
:end-before: // [/actor]
:dedent: 4
```

The `actor_.emplace(...)` call above transfers a retained handle into this small
game-owned structure. Use the same approach when an enemy manager or another
game system needs a sprite collection: resolve the required names once, then
pass the handles into that system. Keep per-entity transforms, counters, and
playback state with their application owners. A namespace collection's iteration
order supplies no gameplay identity.

The loader can go out of scope after initialization. Cache lookups and the
actor's retained handles have their own lifetimes. The resources still belong to
their original backend domain; retaining a handle does not make it usable after
that backend has been destroyed. See the
[resource ownership guide](../resources/resource-residency.md).

## 6. Select a font and prepare the score label

Font selection chooses immutable font bytes and faces. It does not draw text or
upload a glyph atlas. Select a preferred installed family, lay out the message,
prepare its pixels and geometry, then upload that complete value:

```{literalinclude} ../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:start-after: // [score]
:end-before: // [/score]
:dedent: 8
```

{cpp:class}`CE::Text::FontCollection` tries the preferred `DejaVu Sans` family
and appends the embedded fallback. `automatic_system_fonts=false` disables the
additional automatic family choices; the explicitly preferred installed family
is still considered. A missing family is skipped, so the score remains usable
without that system installation.

{cpp:func}`CE::Text::layout_text` shapes the UTF-8 message using the selected
font data. {cpp:func}`CE::Assets::prepare_text` creates owned CPU atlas pages and
glyph geometry. {cpp:func}`CE::Assets::upload_text` uses the resource provider to
create a {cpp:class}`CE::Assets::RenderedText` generation. Initial upload is safe
here because `init()` runs on the graphics owner.

The retained text owns the layout and matching resources it needs after the local
font and preparation variables leave scope. Keep this static label as one
generation. When gameplay starts changing the score, prepare a replacement and
adopt it only when upload succeeds. The
[Unicode text guide](../assets/text-layout.md) covers fallback, wrapping, and
replacement ownership.

## 7. Update simulation and select image cells

Use {cpp:struct}`CE::GFramework::TickContext` during each update. It supplies the
simulation delta, accumulated semantic input, and copied window dimensions:

```{literalinclude} ../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:start-after: // [update]
:end-before: // [/update]
:dedent: 4
```

Movement uses `delta_seconds`, and the camera follows the tick's framebuffer
size. This supports concurrent mode without querying a live window from the
simulation thread. Escape requests a stop through the tick; the runtime performs
its normal cleanup after the update returns.

The sprite definition divides the 32 × 16 PNG into two 16 × 16 cells:

```{literalinclude} ../../projects/apps/first-game/assets/graphics/definitions/sprites.json
:language: json
```

A grid describes which image rectangle each cell selects. Space advances the
actor's cell index explicitly. This manifest supplies no timed animation clip.
For a sprite with authored timed clips, keep a separate
{cpp:class}`CE::Assets::SpriteAnimation` cursor per entity and advance it using
simulation time. The shared cached sprite holds definitions and resources;
its entities choose playback. See
[asset values and playback](../assets/asset-values-and-playback.md).

## 8. Submit the sprite and text as a complete frame

Frame preparation copies the simulation's current transforms and selects
retained resources. The renderer consumes those resolved packets later:

```{literalinclude} ../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:start-after: // [render]
:end-before: // [/render]
:dedent: 4
```

{cpp:class}`CE::Camera2D` uses Y-up pixel coordinates. Opening a pass copies its
projection and view matrices. The
{cpp:struct}`CE::Assets::SubmissionContext2D` carries those pass values and
selects the public `image` parameter at texture unit zero. Each
{cpp:struct}`CE::RenderAPIs::DrawStyle2D` chooses a material and supplies the
draw's model transform, scale, and alpha.

The checked-in
[material manifest](../../projects/apps/first-game/assets/graphics/shaders/materials.json)
gives the shared shader program two compatible draw recipes:

| Material | Geometry topology | Why it is separate |
| --- | --- | --- |
| `first-game:images` | Triangle strip | Each sprite cell uses a four-vertex strip, with nearest sampling for the small image. |
| `first-game:text` | Triangles | Each uploaded glyph uses a six-vertex quad, with linear sampling for its alpha atlas. |

The vertex shader applies the copied pass and draw transforms:

```{literalinclude} ../../projects/apps/first-game/assets/graphics/shaders/first-game.vert
:language: glsl
```

Material bindings map public parameter keys to the shader's native uniform names.
Your common submission code uses `image`, while this OpenGL recipe maps it to
`mytexture`. {cpp:func}`CE::Assets::resolve_sprite` and
{cpp:func}`CE::Assets::resolve_text` resolve complete CPU packets without binding
graphics state or uploading resources. Adding those packets to the pass retains
their geometry, materials, and selected images and copies their parameter values.
The renderer receives no borrowed actor, camera, or playback cursor. The
[pipeline and material guide](../rendering/pipelines-and-materials.md) explains
the contracts and additional recipes.

## 9. Release the game's resources

The runtime joins simulation and recycles accepted frames before calling
`deinit()` on the platform/graphics owner. Remove the game's mappings and release
its retained generations there:

```{literalinclude} ../../projects/apps/first-game/src/first-game.cpp
:language: cpp
:start-after: // [cleanup]
:end-before: // [/cleanup]
:dedent: 4
```

Cleanup also follows attempted initialization, so these resets tolerate resources
that were never created. The example uses the default `quiesce()` because it has
no external producers. Games that add listeners, audio producers, or background
services should stop those producers in `quiesce()` while their dependencies
remain alive. The [runtime frame boundary](../runtime/runtime-frame-boundary.md)
defines the complete shutdown ordering.

## Continue with background loading

The synchronous path makes initial ownership clear. For larger loads, use an
Engine worker group for `Loader::prepare()` and submit the resulting owned
`PreparedAssets` to the platform dispatcher for `upload()`. Simulation checks
futures for readiness and adopts complete retained results during update. Keep
the loader alive through preparation and handoff, and keep the prior resource
generation while its replacement is pending. Avoid waiting on the thread that
must complete the upload.

Text follows the same boundary: font/layout preparation can run on workers;
upload remains on the graphics owner. Follow the working sequences in
[asset loading](../assets/asset-loading.md),
[worker execution](../runtime/worker-execution.md), and
[thread dispatch](../runtime/thread-dispatch.md) when you add that flow. This
example keeps initialization synchronous and frame preparation CPU-only.

## Use the example as an independent project

The example's own CMake entry point supports source and installed SDK consumers.
Its application target links the public owners explicitly:

```{literalinclude} ../../projects/apps/first-game/CMakeLists.txt
:language: cmake
:start-at: add_executable
:end-before: Source Debug
```

To configure the example directly against the checkout, use a separate build
directory. Its bootstrap selects the required Engine, Native GLFW, OpenGL, and
startup targets and disables unrelated applications and modules:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S projects/apps/first-game -B build/first-game-source -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCHERYL_FIRST_GAME_ENGINE_SOURCE="$PWD" \
    -DGLFW_BUILD_WAYLAND=OFF -DGAINPUT_ENABLE_HID=OFF
  cmake --build build/first-game-source --target first-game --parallel
  ./build/first-game-source/first-game
)
```

For an installed SDK, leave `CHERYL_FIRST_GAME_ENGINE_SOURCE` empty and supply a
compatible SDK through `CMAKE_PREFIX_PATH`. It must include Native GLFW with
native input, OpenGL, and OpenGL startup. The SDK contains libraries and headers;
keep this example's source and `assets` directory alongside its CMake project:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S projects/apps/first-game -B build/first-game-sdk -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCHERYL_FIRST_GAME_ENGINE_SOURCE= \
    -DCMAKE_PREFIX_PATH=/absolute/path/to/extracted-sdk
  cmake --build build/first-game-sdk --target first-game --parallel
  ./build/first-game-sdk/first-game
)
```

The application's compiled default asset path names its source `assets`
directory. Use `--assets` when relocating those files with a packaged application.
For SDK ABI/configuration matching and Windows runtime DLL paths, follow
[consuming the engine](../development/consuming-engine.md#prebuilt-development-sdks).
