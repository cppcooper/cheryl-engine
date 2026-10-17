# How to...

Use these guides for one focused task after [Project setup](../project-setup.md)
and [First Game](../first-game.md). Each task identifies its loading, update,
frame-preparation and cleanup boundaries, then links the detailed contract.

The [maintained tutorial game](../../../projects/apps/tutorial-game/README.md)
combines all six tasks with Player, Enemy and HUD classes and a small explicit
asset set. Its compile target is `tutorial-game`; native observation of rendering,
fonts and focus remains a separate acceptance step.

```{toctree}
:maxdepth: 1

Bind input actions <bind-input>
Load and draw a sprite <draw-sprite>
Play sprite animation <animate-sprite>
Display and update text <update-text>
Position objects with a camera <use-camera>
Create an interactive GUI <create-gui>
```

For a complete application-owned class, continue with
[Create a...](../create-a/index.md). The system relationships behind these tasks
are introduced in [Engine Systems](../../engine-systems/index.md).
