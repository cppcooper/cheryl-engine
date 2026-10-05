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
    tgui/           # Optional toolkit integration
```

| Owner | Target | Engine contracts |
| --- | --- | --- |
| [platform/native-glfw](platform/native-glfw/README.md) | `Cheryl::NativeGLFW` | [`core/display`](../engine/include/cheryl/core/display): `iDisplaySystem`, `iWindow`; [`core/controls`](../engine/include/cheryl/core/controls): `iInputSystem`. |
| [graphics/opengl](graphics/opengl/README.md) | `Cheryl::OpenGL` | [`core/rendering`](../engine/include/cheryl/core/rendering): `iRenderer`, `iPresentationSurface`; [`assets/resources`](../engine/include/cheryl/assets/resources): `ResourceProvider`, `Image`, `Geometry2D`, `Shader`, `Pipeline`. |
| [ui/tgui](ui/tgui/README.md) | `Cheryl::UI::TGUI` | Consumes Engine input records through TGUI event translation. Retained rendering/resources and GUI lifetime are remaining U9 work; this module does not implement an Engine widget interface. |

An owner can fulfill several related contracts. Native GLFW couples display and
input lifetimes; OpenGL couples rendering and resource ownership with its context.
Their detailed contract maps live with those owners. Add another role folder when
an implemented module needs it.

Selection, standalone composition and test ownership are described in
[the module guide](../../docs/development/modules.md).
