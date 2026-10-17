# How to...

Use these guides for one focused task after [Project setup](../project-setup.md)
and [First Game](../first-game.md). Each task identifies its loading, update,
frame-preparation and cleanup boundaries, then links the detailed contract.

The [maintained tutorial game](../../../projects/apps/tutorial-game/README.md)
demonstrates input, sprites, animation, camera movement and direct text updates
with a TGUI controls panel. The interactive GUI task below teaches a separate
RmlUi document workflow; [Create a HUD](../create-a/hud.md) covers TGUI widgets.
Native observation of rendering, fonts and focus remains a separate acceptance
step from compiling an example.

```{toctree}
:maxdepth: 1

Bind input actions <bind-input>
Load and draw a sprite <draw-sprite>
Play sprite animation <animate-sprite>
Display and update text <update-text>
Position objects with a camera <use-camera>
Create an interactive GUI with RmlUi <create-gui>
```

For a complete application-owned class, continue with
[Create a...](../create-a/index.md). The system relationships behind these tasks
are introduced in [Engine Systems](../../engine-systems/index.md).
