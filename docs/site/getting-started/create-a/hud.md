# Create a HUD with TGUI

Build an application-owned HUD from TGUI widgets: a score label at the top-left
and a health bar at the top-right. Game logic owns the values; the HUD owns
their presentation, layout and retained UI scene. Begin with
[First Game](../first-game.md), or continue from
[Create a Character](character.md).

First Game explains direct text rendering and
[screen-space placement](../first-game.md#keep-text-in-screen-coordinates).
This tutorial replaces that score display with toolkit widgets. For document
authoring, styling and interactive input through the other UI integration, see
[Create an interactive GUI with RmlUi](../how-to/create-gui.md).

## Select TGUI

Enable `CHERYL_BUILD_UI_TGUI=ON` in First Game's source configuration, or use an
SDK containing that module. Add the UI target to your game's existing links:

```cmake
target_link_libraries(game PRIVATE Cheryl::UI::TGUI)
```

Use your executable's target name in place of `game`. Prepare the pinned toolkit
through [Project setup](../project-setup.md); enabling an option does not download
the dependency.

If using the copied CMake project from Project setup, change its local
`set(CHERYL_BUILD_UI_TGUI OFF)` to `ON` before `add_subdirectory()`. That template's
{ref}`scoped module selections <source-module-selection>` take
precedence over a same-name command-line `-D` cache option.

The uploader obtains compatible rendering materials from the selected graphics
provider automatically. With OpenGL, their shaders are embedded in the library;
this HUD needs no additional shader files or material setup.

## Give the HUD its own class

Create `hud.h` with the following class. Its constructor saves input/upload
dependencies on platform; the first update creates the toolkit objects on
their simulation owner.

```{literalinclude} ../../../../projects/apps/tutorial-game/src/hud.h
:language: cpp
```

TGUI owns font layout and widget geometry, including the health bar's background
and filled portion. The context-aware session uses the engine catalogue's default
font. Configure that catalogue once before constructing consumers; with no
configuration, the engine supplies its embedded safety font.
Its coordinates are logical UI units with a downward-positive Y axis.

Add each widget to the GUI before calling `set_layout()`. The label attaches its
top-left corner to the parent's top-left; the bar attaches its top-right corner
to the parent's top-right. Those bindings follow view resizing without manual
framebuffer arithmetic. See [typed layout](../../modules/tgui.md#typed-layout)
for bounded proportional sizes and parent-relative offsets.

Target `2` is a chosen nonzero routing identity. This passive HUD requests no
keyboard focus and selects no pointer delivery, so the game's movement, score
and quit actions remain available. It still feeds the complete ordered input
batch for session bookkeeping. An interactive HUD must explicitly choose its
focus and pointer policy.

TGUI permits one session per process. If the game already has TGUI menus or
controls, put the HUD widgets in that session instead of constructing a second
one. This standalone class assumes it owns the game's only TGUI session.

## Connect game state to the widgets

Include `hud.h`, `<optional>` and `<limits>` in the appropriate game files.
Replace First Game's raw `score_` generation, font/layout/upload block, direct
text draw and generation cleanup with the HUD integration below.

Add these game members:

```cpp
std::optional<Tutorial::Hud> hud_;
int points_ = 0;
unsigned int health_ = 100;
```

During platform-owned `init()`, after graphics initialization:

```cpp
hud_.emplace(engine_);
```

After quit handling and gameplay updates, publish current game values to the
HUD. Here First Game's Space action also increments the example score:

```cpp
if (tick.input.button(next_cell_).pressed() && points_ < std::numeric_limits<int>::max())
    ++points_;
hud_->set_score(points_);
hud_->set_health(health_);
hud_->update(tick);
```

Actual game rules award points and change `health_`. HUD clamps health to its
0–100 display range; it does not decide when damage or healing occurs.

## Append the retained UI scene

After the world pass containing the sprite or character draws, in
`prepare_render_frame()`:

```cpp
hud_->append(frame);
```

The UI scene supplies its own screen projection, so world-camera movement does
not move the HUD. It needs no separate `Camera2D` or manual text draw. The first
scene can be empty until its upload completes. Later updates retain the last
complete scene while at most one replacement uploads; subsequent widget changes
are recorded when that request finishes. Ready upload failures propagate through
the runtime's normal failure handling.

Record and mutate widgets only on simulation. The upload captures owned scene
data and creates the default materials lazily on the platform owner. Published
frames retain their selected graphics resources without accessing live widgets.
To use custom shaders, pass loaded material handles through the optional
[`Materials` override](../../rendering/pipelines-and-materials.md#custom-ui-materials).

## Release and observe

During `deinit()`, after simulation has joined and accepted work has settled:

```cpp
if (hud_)
    hud_->close_after_quiescence();
hud_.reset();
```

The class drops its external widget handles before session closure. Keep the
input adapter and graphics dependencies alive through this cleanup, including
after partial initialization.

Expect `Score: 0` and a full health bar. Space advances the score; setting
`health_` to `25` shows a quarter-full bar. Resize the window and move the world
camera: score and health should remain at their screen corners, while gameplay
keys continue to work. Check both runtime modes for input and teardown behavior.

The [maintained tutorial game](../../../../projects/apps/tutorial-game/README.md)
uses this widget HUD with its shared Character and player/AI controllers.
Its passive widgets leave keyboard gameplay available.
The [TGUI demo](../../../../projects/apps/demo/src/tgui-demo.cpp) also demonstrates
interactive widgets. Read [TGUI contracts](../../modules/tgui.md) and
[GUI ownership](../../gui/index.md) for the integration boundaries.
