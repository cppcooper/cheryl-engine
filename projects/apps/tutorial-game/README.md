# Tutorial game

This maintained example combines the [Player and Enemy classes](../../../docs/getting-started/create-a/index.md)
with input, sprite animation, camera movement, a direct text score and TGUI
controls. The [focused tasks](../../../docs/getting-started/how-to/index.md)
explain its input, sprite, camera and text operations. It consumes the standard
Startup module, Native GLFW, OpenGL and TGUI. The two-cell actor and base
image/text shaders are copied from First Game; TGUI uses the selected provider's
automatic materials with embedded OpenGL shaders. Its small asset set is listed
explicitly in CMake and needs no optional external art download.

The [TGUI widget HUD tutorial](../../../docs/getting-started/create-a/hud.md) and
[RmlUi GUI guide](../../../docs/getting-started/how-to/create-gui.md) describe
separate extensions to First Game. This executable uses its direct text `Hud` and
TGUI `Controls` classes; it does not implement those two interfaces.

The game starts with an editable field focused. Escape releases it; a later
Escape press quits. F2 returns focus to editing. With editor focus released,
Left/Right move Player, A/D position the world view and Space increments the HUD.
Enemy patrols independently and a third sprite alternates cells on simulation
time. Clicking Apply changes the field and increments the HUD. The HUD and UI
remain in screen coordinates while the world view moves.

The `tutorial-game` CMake target requires selected `Cheryl::Startup`,
`Cheryl::NativeGLFW`, `Cheryl::OpenGL` and `Cheryl::UI::TGUI` targets, native input,
and configured OpenGL Startup integration. When configured independently, it
finds a matching installed SDK through `find_package(Cheryl 4 CONFIG REQUIRED)`.
The source-root build selects it through the repository's application options.
Its default asset root is this source directory's `assets/` folder.

After [preparing the source dependencies](../../../docs/development/building.md#dependency-and-toolchain-settings),
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
