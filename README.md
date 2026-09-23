# Cheryl-Engine

## Demo

Build with `cmake -S . -B build` and `cmake --build build --target demo`, then run `./build/demo` from the repository root. The demo uses a system font and the checked-in 2D shader, without needing sprite textures. WASD pans the camera; R resets it. Move or click the mouse, scroll, or press gamepad A to see input values change.

For a sandbox build without X11 or Gainput, configure with `cmake -S . -B build-sandbox -DCHERYL_SANDBOX_BUILD=ON`. This builds GLFW's null platform, omits the Gainput-backed input adapter and windowed demo, and keeps the backend-independent engine and test targets available.

GLAD's generator needs `jinja2` in the Python interpreter selected by CMake. Install it in that interpreter, or configure with `cmake -S . -B build -DPython_EXECUTABLE=/path/to/python-with-jinja2` when another interpreter already has it.

Run `./build/demo --full-assets` to load all manifests, textures, fonts, and shaders. The PNGs referenced by the checked-in manifests are not tracked in this repository and must be present under `assets/` for this mode. An additional argument can specify a different asset directory.
