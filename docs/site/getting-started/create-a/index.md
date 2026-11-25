(create-a)=
# Tutorials

Follow this sequence after [Project setup](../project-setup.md). First Game
introduces startup, game lifecycle, sprites, and direct text. The following lessons
separate actor decisions, actor state, and interface coordination into
application-owned classes using the [maintained tutorial game](../../../../projects/apps/tutorial-game/README.md).

```{toctree}
:maxdepth: 1
:titlesonly:

First game <../first-game>
Actor controllers and commands <actor-controller>
PlayerController <controller>
Character <character>
AIController <ai-controller>
UIController <ui-controller>
HUD with TGUI <hud>
Event <event>
Abstract controller interfaces <abstract-controller>
```

```{toctree}
:maxdepth: 1
:titlesonly:
:caption: Advanced tutorials

Developer console <developer-console>
```

The actor-controller lesson establishes a command contract. Player and AI
controllers produce that command from different observations; the Character
applies it and prepares its appearance. The UIController owns the shared TGUI
session and scene publication, while HUD/menu/chat components manage their widgets.
The final abstract-interface discussion is optional: use the concrete examples to
identify whether the game needs a shared invocation contract.

[Create an event](event.md) is an independent example of a typed game notification,
from its payload and channel to listeners, publication and cleanup.

## Advanced tutorials

These lessons extend a working game with features that cross input, UI,
application ownership, and testing boundaries. Each names its starting scaffold.

[Add a developer console](developer-console.md) starts from First Game and adds
typed commands, owner-backed settings, the default graphical input integration,
and bounded script assertions. It also explains the separate file-job/report
layer and composition with other interfaces.

Share immutable assets between instances while keeping action contexts, positions,
behavior state, and UI component state independent. The
[interactive GUI task](../how-to/create-gui.md) teaches RmlUi document authoring.
Use the [Task index](../how-to/index.md) to find an operation or
[Guides by topic](../../guides/index.md) to browse procedures and references.
[Engine Systems](../../engine-systems/index.md) maps public types and relationships.
