# Cheryl-Engine

Cheryl is a C++23 engine with composed platform/rendering backends, sequential and
concurrent simulation, retained 2D render submissions, and CPU asset preparation
separate from backend upload.

## Build and demo

Initialize dependencies with `git submodule update --init --recursive`. CMake 3.28+
and a C++23 compiler are required. GLAD's generator needs `jinja2` in the Python
interpreter selected by CMake. Install it there, or set
`-DPython_EXECUTABLE=/path/to/python-with-jinja2` during configuration.

Build with `cmake -S . -B build` and `cmake --build build --target demo`, then run
`./build/demo` from the repository root. Normal Linux builds require the X11/OpenGL
and GLFW platform development dependencies. The demo uses a system font and the
checked-in 2D shader without needing sprite textures. WASD pans the camera; R resets
it. Mouse movement/clicks, scrolling, and gamepad A update the input display. F2 opens
text focus, Enter/Esc releases it, and F5 reloads the material recipe. Add
`--concurrent` to use the simulation worker.

Run `./build/demo --full-assets` to load the manifest/image asset tree in addition
to the demo's explicitly selected font and shader material. Manifest PNGs are not
tracked and must be present under `assets/`. A positional argument selects another
asset root: `./build/demo --concurrent /path/to/assets`.

For a sandbox build without X11 or Gainput, configure with
`cmake -S . -B build-sandbox -DCHERYL_SANDBOX_BUILD=ON`. This builds GLFW's null
platform, omits the Gainput-backed input adapter and windowed demo, and keeps the
engine and aggregate test targets available. Sandbox OpenGL sources still need
OpenGL development/link dependencies.

## Project layout

`projects/engine/` owns the engine's `include/`, `src/`, `tests/` and supporting
targets. The demo lives in `projects/apps/demo/`; the Backward diagnostic tool lives
in `projects/tools/backward-cpp/`. Each target is defined in its owning directory;
root CMake selects and composes them. Link `Cheryl::Engine` to inherit public header
paths and dependencies; existing C++ include spellings remain supported.

Native GLFW/Gainput and the whole OpenGL backend have separate optional owners under
`projects/modules/`. The root selects the default native/OpenGL assembly. Disable
`CHERYL_BUILD_NATIVE_GLFW` and `CHERYL_BUILD_OPENGL` for Engine alone; link the selected
`Cheryl::NativeGLFW` and `Cheryl::OpenGL` targets for native graphics applications.
See [module composition and test selection](docs/development/modules.md).

## Documentation

The [documentation index](docs/README.md) groups the engine docs by runtime,
assets, rendering, resources, development, and planning.

Start with [runtime architecture and API boundaries](docs/runtime/runtime-architecture.md)
for the engine overview, [code style](docs/development/code-style.md) for contribution
conventions, or [unfinished engine work](docs/planning/todo.md) for current TODOs.
