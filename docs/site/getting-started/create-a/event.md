# Create an event

An event describes something that happened in your game. Define its data as a
plain struct, give it a typed channel, then publish it through the same bus your
listeners use. The game code chooses the bus when publishing.

Start with [Project setup](../project-setup.md) and the `CE::Runtime::Context`
borrowed by your game in [First Game](../first-game.md). This example applies
damage, then updates a value a HUD could display.

```{graphviz}
:alt: Game code publishes DamageTaken through the event bus to HUD and sound listeners.

digraph {
    rankdir=LR;
    "Game code" -> "Event bus" [label="DamageTaken"];
    "Event bus" -> { "HUD listener" "Sound listener" };
}
```

The listeners belong to your game. This lesson models the HUD observation; a
sound listener could subscribe independently.

## Define the payload and channel

Create `game-events.h` in your game's source directory:

```cpp
#pragma once

#include <cheryl/core/subsystems/event-channel.h>

namespace GameEvents {
    struct DamageTaken {
        int amount;
        int health_remaining;
    };

    inline const CE::Events::EventChannel<DamageTaken> damage_taken{"game.damage-taken"};
}
```

`DamageTaken` carries the facts. `damage_taken` identifies this event by its exact
name and payload type. Include this header wherever you produce or consume it.
The payload must be copy-constructible; small value fields make ownership clear.

## Subscribe, publish and clean up

Add this illustrative helper near the top of your game's `.cpp` file, before its
game method definitions:

```cpp
#include "game-events.h"
#include <cheryl/core/engine/engine-context.h>

bool demonstrate_damage_event(CE::Runtime::Context& engine) {
    auto& events = engine.events();
    int displayed_health = 100;

    const auto hud_listener = events.register_listener(
        GameEvents::damage_taken,
        [&displayed_health](const GameEvents::DamageTaken& event) {
            displayed_health = event.health_remaining;
        }
    );

    int health = 100;
    health -= 13;
    events.notify(GameEvents::damage_taken, GameEvents::DamageTaken{13, health});

    events.unregister_and_wait(hud_listener);
    return displayed_health == 87;
}
```

Call `demonstrate_damage_event(engine_)` once from First Game's `init()`.
It returns `true`: health becomes `87`, and the listener copies that value into
`displayed_health` before `notify()` returns. This variable models the HUD's retained
state. Both registration and publication use the same Context bus. A channel
alone does not select a bus.

In a lasting HUD, retain the registration as a member and remove it before
destroying the state its callback borrows. Dropping the registration does not
unsubscribe. Call `unregister_and_wait()` outside that listener's own callback.
The callback's event reference is borrowed; copy any values you need later.

## Choose delivery behavior

Use `notify()` for observations of changes already applied, such as this health
update. It reports notification failures and continues attempting the remaining
listeners if an immediate callback throws. `dispatch()` instead propagates an
immediate callback exception to the publisher and stops that delivery's remaining
listeners. Neither operation rolls back gameplay state.

Both run callbacks synchronously on the publishing thread by default. Keep this
example on one owner; publishing from a worker does not automatically move a HUD
callback to simulation. For cross-thread listeners, choose an explicit
[queued delivery adapter](../../runtime/event-delivery.md#optional-queued-delivery).
See [event delivery](../../runtime/event-delivery.md) for channel identity,
failure handling and listener lifetime details.
