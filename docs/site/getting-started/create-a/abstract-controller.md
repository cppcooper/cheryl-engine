# Choose an abstract controller interface

An abstract controller interface is useful when the game needs to invoke different
controller types through the same contract. Start with the working
[PlayerController](controller.md), [AIController](ai-controller.md), and
[UIController](ui-controller.md) examples before choosing that contract.

The maintained tutorial game retains concrete controllers and calls their useful
operations directly. Cheryl does not require an `AbstractController` base class.

## Identify the operation the caller needs

| Controller role | Persistent dependencies and state | Per-update operation |
| --- | --- | --- |
| Player | Its scoped action context and application-selected bindings. | Read copied input and produce an actor command. |
| AI | Goals, behavior state, and any longer-lived world services it borrows. | Observe actor/world state and time, then produce an actor command. |
| UI | Session, interface components, and retained scene/upload state. | Coordinate input, widgets, and scene publication. |

Player and AI controllers share an actor-command result. A common actor-decision
interface can make choosing or replacing that source useful. The UI controller
coordinates a different result: the application's interface and its published scene.

## Consider a shared update hook

For a game that owns a heterogeneous collection of controllers, a possible
application-defined interface is:

```cpp
#include <cheryl/core/game-framework/tick-context.h>

class AbstractController {
public:
    virtual ~AbstractController() = default;
    virtual void update(const CE::Game::TickContext& tick) = 0;
};
```

This illustrative interface would let the game call `update(tick)` through base
references or a collection of owned base pointers. Its implementations would
adapt their concrete operations to that hook. The tutorial's existing controller
classes do not inherit it.

The tick supplies this update's time, input, and dimensions. Each implementation
can retain its own dependencies, such as a controlled actor, world service, action
context, or UI session. Borrowed dependencies must outlive their use. Alternatively,
pass a focused observation into each decision call, as the patrol example does.

An update hook does not establish initialization, shutdown, or ordering policy.
The game must still preserve platform-owned resource setup, simulation-owned UI
operations, and dependency-ordered cleanup. Give a common interface only the
operations its caller actually needs.

## Choose the smallest useful contract

Keep concrete calls when they express the game's update order clearly. Introduce
a shared interface when a concrete caller needs uniform invocation or replaceable
behavior. For actor controllers, establish the observations and command result
before choosing a virtual method signature. For UI coordination, preserve the
session and publication lifecycle taught in [Create a UIController](ui-controller.md).

The useful outcome is an interface the game can explain and use. A shared class
name alone does not establish shared responsibilities.
