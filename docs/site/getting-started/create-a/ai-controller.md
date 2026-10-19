# Create an AIController

Drive a second Character with autonomous patrol behavior. The AI produces the
same `CharacterCommand` as the [PlayerController](controller.md), so movement and
appearance remain with the Character. Continue from [Create a Character](character.md).

`AIController` names the role. This concrete example is `Tutorial::PatrolController`:
it chooses left/right movement using actor observations and its own direction state.

## Define patrol behavior

Create `patrol-controller.h`:

```{literalinclude} ../../../../projects/apps/tutorial-game/src/patrol-controller.h
:language: cpp
```

The interval must be finite with `left < right`. Each patrol retains its own
direction. `read()` borrows the Character for the call and receives elapsed
simulation seconds; it changes its decision state and returns a command.
The game still applies that command through `Character::update()`.

This example needs position, speed, and time. More involved AI can receive a
read-only world observation for each decision or retain references to longer-lived
world services. Keep those dependencies explicit and alive for their period of use.
`TickContext` supplies per-update information; it does not contain the application's
world, goals, or behavior memory. A controller can receive only the tick fields it
uses, as this patrol does.

## Add an autonomous Character

Include `patrol-controller.h` in the game header. Add these members beside the
existing player, using its retained `sprite_` and `images_` resources:

```cpp
Tutorial::PatrolController patrol_{80.0f, 500.0f};
std::optional<Tutorial::Character> enemy_;
```

During platform-owned `init()`, after loading those resources:

```cpp
enemy_.emplace(sprite_, images_, glm::vec2(80.0f, 220.0f), 1, 80.0f);
```

The selected sprite needs cell `1`, as First Game's actor asset provides. Begin
inside the patrol interval. In `update()`, after the player update:

```cpp
enemy_->update(patrol_.read(*enemy_, tick.delta_seconds), tick.delta_seconds);
```

In `prepare_render_frame()`, append `enemy_` after the player in the same world pass:

```cpp
enemy_->append(world);
```

The player and patrol now produce the same kind of command through different
observations. To change an actor's decision source, the game selects which
controller supplies its command for that update. Keep one movement application
per actor per update.

## Release and observe

Reset `enemy_` during `deinit()` with the other Characters, before releasing the
retained appearance resources. `patrol_` owns only its numeric behavior state.

Expect the second Character to move between X positions `80` and `500` while the
player remains independently controllable. A long update stops the patrol at an
endpoint; a later update reverses it. This simple policy does not simulate multiple
reflections within one update, collisions, or the sprite's full visual extent.
The interval constrains the sprite pivot.

Continue with [Create a UIController](ui-controller.md) to coordinate interface
state and presentation. That controller has a different responsibility from the
actor-command producers here.
