# Play sprite animation

Animation is per-instance simulation state over shared immutable sprite data.
Start from [drawing a sprite](draw-sprite.md). The maintained actor has two cells
and deliberately has no timed clip; add a clip in your application's copied
definition before asking the asset for one.

## Define a clip

Inside the `actor` entry in your copy of `graphics/definitions/sprites.json`, add
an `animations` member beside `grid`:

```json
"animations": {
  "blink": {
    "frames": [
      {"cell": {"index": 0}, "duration_ms": 200},
      {"cell": {"index": 1}, "duration_ms": 200}
    ],
    "loop": true
  }
}
```

This is a member fragment, not a complete manifest; retain the existing version,
namespace, texture, grid and enclosing index. Every cell must fit that grid and
every frame duration must be positive. Load the edited root on initialization.
The [manifest guide](../../assets/asset-manifests.md#animations) describes facing
profiles for more elaborate sheets.

## Create an independent cursor

Add `<optional>` and `<chrono>` and retain this member beside the sprite handle:

```cpp
std::optional<CE::Assets::Sprites::Animation> animation_;
```

After validating/loading `sprite_` in `init()`:

```cpp
animation_.emplace(sprite_->animation("blink"));
```

Missing or ambiguous clips throw. A separately constructed cursor for each
entity has independent progress, even when entities share the same asset.
Do not retrieve a fresh cursor in every update: that restarts playback.

## Advance only during update

```cpp
animation_->advance(std::chrono::duration<double>(tick.delta_seconds));
cell_ = animation_->cell();
```

Use finite nonnegative simulation elapsed time. The example follows runtime's
simulation clock, so animation responds to its selected timing policy. At an
exact boundary the next frame becomes current. Looping clips wrap; nonlooping
clips remain on their last frame. `set_frame()` selects a normalized frame and
resets time within it when explicit restart/scrubbing is needed.

## Publish and release

Use the same `resolve_sprite(*sprite_, cell_, style, context)` path from
[drawing](draw-sprite.md). Frame preparation copies the selected cell without
advancing animation. This keeps playback correct even when concurrent rendering
skips a visual snapshot. Reset `animation_` and the game's resource handles during
cleanup, including partial initialization.

The actor should alternate its two cells every 200 milliseconds. Two instances
created at different times should keep independent phases. A nonlooping copy
should stop at its final cell. These observations check the teaching result;
they do not replace the engine's existing regression coverage.

Read [sprite playback contracts](../../assets/asset-values-and-playback.md#sprite-playback)
and the [animation interface](../../../projects/engine/include/cheryl/assets/types/2d/sprite.h)
for cursor ownership, facing lookup and invalid-time behavior. Tileset animation
uses a different interface; do not assume every sprite operation applies to it.
