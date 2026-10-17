# Create a Player

Create an application-owned class that moves a sprite from semantic input and
publishes its current appearance. Cheryl does not supply a built-in player,
entity system or collision model. Work in a copy of
[First Game](../first-game.md), keeping its startup, bindings, materials and
runtime hooks. This class replaces the example's small `Actor` value.

## Responsibilities and state

The game owns binding registration and loading because those operations require
the platform owner. Player retains immutable sprite/material handles and owns
position and movement policy. It receives action IDs rather than physical key
codes, so the game can change bindings without changing the class.

Create `player.h`:

```{literalinclude} ../../../projects/apps/tutorial-game/src/player.h
:language: cpp
```

Movement units are local pixels per simulation second. Both actions held gives
zero horizontal movement. This example selects cell zero and deliberately has
no collision or world-boundary behavior; add those policies to your game rather
than treating a renderer coordinate as an automatic physics constraint.

## Construct after loading

Include `player.h` and retain `std::optional<Tutorial::Player> player_` in the
game. Keep First Game's initialization of the camera, input bindings and
`images_` material. Replace its `actor_.emplace(...)` with:

```cpp
player_.emplace(std::move(sprite), images_, glm::vec2(160.0f, 100.0f), left_, right_);
```

Here `sprite` is the validated `first-game:actor` handle loaded by the existing
initialization block, and `left_`/`right_` are the game's action IDs, not binding
registration IDs. The optional remains empty if construction fails.

## Update and prepare

Keep the game's quit handling and camera resize observation. Replace its Actor
movement/cell block with:

```cpp
player_->update(tick);
```

Inside frame preparation, begin the world pass as before and replace the Actor's
image-style/`resolve_sprite()` block with:

```cpp
player_->append(pass);
```

The existing score text may still be added afterward. Do not keep both image
paths unless you intend to draw both objects. Update changes authoritative
position; append only copies visible state and retains resources in packets.
Concurrent runtime can skip append calls without slowing movement.

## Release and observe

Replace `actor_.reset()` with `player_.reset()` in `deinit()` and keep the
existing binding removal and material/text cleanup. Player's shared handles are
released there; completed frame packets may still have their own retained
generations. Runtime recycles frames before backend shutdown.

Left/right input should move one sprite while the quit action still stops the
game. Releasing movement stops it; holding both directions cancels movement.
Switching runtime execution mode should preserve the same ownership boundaries.
Input focus may intentionally gate keyboard gameplay while an editor is open.

Use [binding actions](../how-to/bind-input.md),
[sprite animation](../how-to/animate-sprite.md) and
[camera placement](../how-to/use-camera.md) to extend this class. Animation adds
an independent cursor to Player; shared sprite metadata remains immutable.
