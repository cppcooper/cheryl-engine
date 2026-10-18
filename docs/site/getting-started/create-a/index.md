(create-a)=
# Tutorials

Follow this sequence after [Project setup](../project-setup.md). First Game
introduces startup, the game lifecycle, sprites and direct text. The following
lessons organize those operations into application-owned classes: a Controller
chooses commands, a Character applies them, and a TGUI HUD presents game state.

The Controller, Character and HUD lessons use the
[maintained tutorial game](../../../../projects/apps/tutorial-game/README.md): reusable
input definitions drive Characters, an autonomous controller supplies the same
commands, and TGUI widgets present game state. The
[interactive GUI guide](../how-to/create-gui.md) teaches RmlUi documents.
First Game owns the direct text rendering walkthrough.

```{toctree}
:maxdepth: 1
:titlesonly:

First game <../first-game>
Controller <controller>
Character <character>
HUD with TGUI <hud>
```

A controller chooses commands, a Character applies them and prepares its
appearance, and a HUD presents game information. Share immutable assets between
instances while keeping action contexts, positions, timers, animation cursors and
score state independent.
Use the [Task index](../how-to/index.md) to find an operation or
[Guides by topic](../../guides/index.md) to browse procedures and reference
material. [Engine Systems](../../engine-systems/index.md) maps responsibilities,
public types and their relationships.
