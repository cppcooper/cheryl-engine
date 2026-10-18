# Tutorial game

This maintained example follows [Controller → Character → HUD](../../../docs/site/getting-started/create-a/index.md).
A player controller translates scoped input actions into a game-owned movement
command; a patrol controller produces the same command without input. Both drive
one Character class. The HUD uses TGUI score and health widgets. The
[task index](../../../docs/site/getting-started/how-to/index.md) links the engine
operations behind these examples.

It consumes Startup, Native GLFW, OpenGL and TGUI. The two-cell actor image comes
from First Game. Sprites and TGUI use the selected provider's automatic materials
with embedded OpenGL shaders; TGUI uses the engine's configured default font.
The small asset set is listed explicitly in CMake and needs no optional art download.
[First Game](../../../docs/site/getting-started/first-game.md) retains the simple direct
input and text path; the [RmlUi guide](../../../docs/site/getting-started/how-to/create-gui.md)
teaches interactive document authoring.

Left/Right move the first character. F2 joins a second player using J/L; F3 removes
that player. Both player controllers reuse the same local action IDs, with
independent binding lifetimes. A/D move the world view, Space increments the HUD
score, and Escape quits. A second character patrols between fixed bounds and a
third sprite alternates cells on simulation time. The widget HUD stays attached
to the screen corners while the world view moves. The HUD is passive and requests
no keyboard focus.

The engine accepts independent action contexts for supplied device IDs. The
current GLFW adapter exposes one gamepad; this example uses separate keys on one
keyboard and does not establish multiple-gamepad discovery or physical-device QA.

The `tutorial-game` CMake target requires selected `Cheryl::Startup`,
`Cheryl::NativeGLFW`, `Cheryl::OpenGL` and `Cheryl::UI::TGUI` targets, native input,
and configured OpenGL Startup integration. When configured independently, it
finds a matching installed SDK through `find_package(Cheryl 4 CONFIG REQUIRED)`.
The source-root build selects it through the repository's application options.
Its default asset root is this source directory's `assets/` folder.

After [preparing the source dependencies](../../../docs/site/development/building.md#dependency-and-toolchain-settings),
configure and run the Linux x64 example from anywhere inside the checkout:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/tutorial-game -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCHERYL_BUILD_TUTORIAL_GAME=ON -DCHERYL_BUILD_DEMO=OFF \
    -DCHERYL_BUILD_FIRST_GAME=OFF -DCHERYL_BUILD_STARTUP=ON \
    -DCHERYL_STARTUP_OPENGL=ON -DCHERYL_BUILD_NATIVE_GLFW=ON \
    -DCHERYL_BUILD_OPENGL=ON -DCHERYL_NATIVE_INPUT=ON \
    -DCHERYL_NATIVE_NULL_PLATFORM=OFF -DCHERYL_BUILD_UI_TGUI=ON \
    -DCHERYL_BUILD_UI_RMLUI=OFF -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
    -DCHERYL_BUILD_WORKERS=OFF -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
    -DCHERYL_BUILD_DOCS=OFF -DCHERYL_BUILD_SDK=OFF -DCHERYL_BUILD_TESTS=OFF \
    -DGLFW_BUILD_WAYLAND=OFF -DGAINPUT_ENABLE_HID=OFF
  cmake --build build/tutorial-game --target tutorial-game --parallel
  ./build/tutorial-game/tutorial-game
)
```

Use the build guide's matching triplet/CRT settings on Windows; the executable
has an `.exe` suffix. A standalone SDK consumer configures this example directory
with `CMAKE_PREFIX_PATH` pointing to a matching SDK containing Startup/OpenGL,
native input and TGUI.

The example supports the standard startup/runtime arguments, including
`--concurrent`. UI objects live on simulation; graphics upload runs through the
platform dispatcher. Shutdown releases external widgets after simulation joins,
settles accepted requests and releases game resources before backend closure.
Compile acceptance and native observation are separate: a successful build
alone does not establish visible widgets, fonts, focus or graphics teardown.
