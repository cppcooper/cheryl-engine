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

## Documentation

- [Runtime architecture and API boundaries](docs/RUNTIME-ARCHITECTURE.md)
- [Frame ownership and runtime lifecycle](docs/RUNTIME-FRAME-BOUNDARY.md)
- [Input State, Events, Text, and focus](docs/input-state-model.md)
- [Simulation timing and recovery](docs/SIMULATION-TIMING.md)
- [Thread dispatch](docs/THREAD-DISPATCH.md), [event delivery](docs/EVENT-DELIVERY.md),
  and [worker execution](docs/WORKER-EXECUTION.md)
- [Pipelines and materials](docs/PIPELINES-AND-MATERIALS.md) and
  [asset/render header boundaries](docs/ASSET-RENDER-BOUNDARIES.md)
- [Asset loading](docs/ASSET-LOADING.md) and [manifest format](docs/ASSET-MANIFESTS.md)
- [Resource residency](docs/RESOURCE-RESIDENCY.md) and
  [memory/resource lifetime](docs/resource-lifetime.md)
- [Recorded validation and repeatable commands](docs/ARCHITECTURE-VALIDATION.md),
  [desktop checks](docs/NATIVE-DESKTOP-CHECKS.md), and [code style](docs/CODE-STYLE.md)
- [Unfinished engine work](docs/TODO.md) and
  [unresolved asset metadata](docs/ASSET-MANIFEST-TODO.md)
