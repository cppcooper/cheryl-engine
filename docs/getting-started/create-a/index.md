# Create a...

These tutorials organize engine operations into application-owned classes.
Cheryl supplies input, resources and rendering; your game defines each class's
behavior, state and lifetime. Begin with the maintained
[First Game](../first-game.md), keeping its initialization/update/frame/cleanup
structure as the integration point.

The [maintained tutorial game](../../../projects/apps/tutorial-game/README.md)
combines these classes and all six focused tasks in one example target. Its
headers are included directly in the tutorials, so class code and
the teaching fixture share one source. First Game remains the introductory
walkthrough; the larger example adds class organization and interactive UI.

```{toctree}
:maxdepth: 1

Player <player>
Enemy <enemy>
HUD <hud>
```

A Player responds to semantic actions, an Enemy owns autonomous behavior and a
HUD publishes game information. Share immutable assets between instances while
keeping positions, timers, animation cursors and score state independent.
Use the [How to...](../how-to/index.md) tasks when you need one operation in
greater focus; use [Engine Systems](../../engine-systems/index.md) for the
ownership and execution model.
