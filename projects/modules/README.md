# Module roles and engine contracts

Modules are grouped by the engine role they provide. Each concrete owner defines
one selected library, its public headers, dependencies and tests. The role folders
organize those owners; engine interfaces remain in `projects/engine/`.

```text
modules/
  platform/
    native-glfw/    # Display, windows and input
  graphics/
    opengl/         # Rendering, presentation and graphics resources
  ui/
    tgui/           # TGUI toolkit integration
    rmlui/          # RmlUi toolkit integration
```

| Owner | Build target | Public alias | Root selection | Engine contracts |
| --- | --- | --- | --- | --- |
| [platform/native-glfw](platform/native-glfw/README.md) | `module_native_glfw` | `Cheryl::NativeGLFW` | `CHERYL_BUILD_NATIVE_GLFW` | [`core/display`](../engine/include/cheryl/core/display): `iDisplaySystem`, `iWindow`; [`core/controls`](../engine/include/cheryl/core/controls): `iInputSystem`. |
| [graphics/opengl](graphics/opengl/README.md) | `module_opengl` | `Cheryl::OpenGL` | `CHERYL_BUILD_OPENGL` | [`core/rendering`](../engine/include/cheryl/core/rendering): `iRenderer`, `iPresentationSurface`; [`assets/resources`](../engine/include/cheryl/assets/resources): `ResourceProvider`, `Image`, `Geometry2D`, `Shader`, `Pipeline`. |
| [ui/tgui](ui/tgui/README.md) | `module_ui_tgui` | `Cheryl::UI::TGUI` | `CHERYL_BUILD_UI_TGUI` | Translates input and records/uploads retained scenes through Engine contracts. The toolkit owns its widget API. |
| [ui/rmlui](ui/rmlui/README.md) | `module_ui_rmlui` | `Cheryl::UI::RmlUi` | `CHERYL_BUILD_UI_RMLUI` | Native document sessions, routed input and premultiplied retained rendering through Engine contracts. |

Native GLFW, OpenGL, TGUI and RmlUi are selected by default.
OpenGL requires Native GLFW. Each UI owner links only Engine and its own toolkit;
either or both can be selected. Disable all module selection options for
Engine alone. Link the public targets to inherit their include directories and
dependency requirements; each owner also supplies a standalone CMake entry point.

An owner can fulfill several related contracts. Native GLFW couples display and
input lifetimes; OpenGL couples rendering and resource ownership with its context.
Their detailed contract maps live with those owners. Add another role folder when
an implemented module needs it.

Selection, standalone composition and test ownership are described in
[the module guide](../../docs/development/modules.md).
The root README lists [all target groups](../../README.md#targets), including
support libraries, test types and output filenames.
