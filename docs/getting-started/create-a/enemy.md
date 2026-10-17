# Create an Enemy

Create an application-owned patrol class sharing the same sprite/material as a
[Player](player.md). Its behavior is independent of physical input and belongs
to simulation. This example adds visible autonomous movement without assuming
built-in AI, physics or an entity/world container.

## Responsibilities and state

Enemy retains immutable visual resources and owns a horizontal patrol interval,
position and phase. A triangular path reflects motion at the interval ends,
including when an update crosses several ends. Rendering reads the resulting
position without advancing behavior.

Create `enemy.h`:

```{literalinclude} ../../../projects/apps/tutorial-game/src/enemy.h
:language: cpp
```

Cell one gives this object a different appearance using the maintained two-cell
actor. If using another sprite, select a valid cell for its grid. The interval
describes the sprite pivot's world position; it does not account for collision
shape or image extent. Time is reduced before multiplying by speed so a large
finite elapsed value does not overflow distance accumulation.

## Construct during initialization

Include `enemy.h` and add `std::optional<Tutorial::Enemy> enemy_` to the game.
After loading/validating the actor and image material, keep a shared copy for
Enemy before moving a handle into Player:

```cpp
auto sprite = CE::Assets::Sprites::Manager::get().get_asset("first-game", "actor");
if (!sprite || !images_)
    throw std::runtime_error("Player and Enemy require the actor and image material");
player_.emplace(sprite, images_, glm::vec2(160.0f, 100.0f), left_, right_);
enemy_.emplace(std::move(sprite), images_, 80.0f, 500.0f, 220.0f);
```

Both instances retain the shared asset; neither copies the texture/geometry
implementation or depends on the other's position. The values fit First Game's
initial 640 by 360 framebuffer. A game with different bounds should choose its
own patrol interval.

## Update and publish

After the game's quit handling and camera observation:

```cpp
player_->update(tick);
enemy_->update(tick.delta_seconds);
```

During frame preparation, begin the world pass and append both:

```cpp
player_->append(pass);
enemy_->append(pass);
```

Authored order controls draw order subject to the selected pipeline. Enemy's
phase advances for every simulation update, even if no frame can be prepared.
Do not derive patrol progress from render count or mutate this object from a
native callback.

## Release and observe

Reset `enemy_` and `player_` during `deinit()` before dropping game materials.
Keep the existing binding cleanup and partial-initialization-safe optionals.
No separate shutdown method is needed for this CPU state/shared-handle class.

The player should respond to input while the second cell patrols between the
chosen bounds. Changing one Enemy's interval/phase should not affect another.
Larger update intervals should still leave its pivot inside the interval.

Add [an animation cursor](../how-to/animate-sprite.md) per instance when using an
animated definition. Behavioral states may use [State](../../engine-systems/state.md)
if useful; this class does not require a state machine or imply pathfinding.
