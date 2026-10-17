# Create a...

These tutorials organize engine operations into application-owned classes.
Cheryl supplies input, resources and rendering; your game defines each class's
behavior, state and lifetime. Begin with the maintained
[First Game](../first-game.md), keeping its initialization/update/frame/cleanup
structure as the integration point.

Player and Enemy include their headers from the
[maintained tutorial game](../../../projects/apps/tutorial-game/README.md).
The HUD guide provides a TGUI widget class that extends First Game; the
[interactive GUI guide](../how-to/create-gui.md) teaches RmlUi documents.
First Game owns the direct text rendering walkthrough.

```{toctree}
:maxdepth: 1

Player <player>
Enemy <enemy>
HUD with TGUI <hud>
```

A Player responds to semantic actions, an Enemy owns autonomous behavior and a
HUD publishes game information. Share immutable assets between instances while
keeping positions, timers, animation cursors and score state independent.
Use the [How to...](../how-to/index.md) tasks when you need one operation in
greater focus; use [Engine Systems](../../engine-systems/index.md) for the
ownership and execution model.
