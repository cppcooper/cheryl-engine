# Event buses and persistent registration

EventSystem remains the global default access layer. Its owned EventBus contains
registrations. A subsystem may instead own an EventBus to isolate its channels and
lifecycle; a bus is a registry, not a thread or worker pool.

```cpp
CE::SubSystems::EventBus local_bus;
auto id = local_bus.register_listener("level-loaded", callback);
local_bus.dispatch("level-loaded", payload);
// EventSystem::get() exposes the same API through its default bus.
```

Registration is persistent. The bus strongly owns its callback until explicit
unregister or bus destruction; throwing away `id` does not remove it. Identifiers
include bus identity, so an ID from a different bus cannot remove a local listener.
Immediate delivery runs on dispatch's caller, in registration order within that
dispatch. Overlapping producers can overlap callbacks; immediate listeners must
be safe on each producer thread. Nested dispatch is synchronous and completes
inside the outer callback. Exceptions propagate to the immediate caller.

The registry lock is released before callbacks run. Registration during dispatch
joins the next snapshot, not the already detached one. String channel names and
`std::any` payload typing retain their current application-level contract.
`unregister_listener(id)` invalidates invocation entry before it returns. A
listener already running may finish. For a callback borrowing an object (for
example `[this]`), call `unregister_and_wait(id)` before destroying that object, or
unregister then call `wait_for_listener(id)`. The barrier waits only for invocations
already entered, not for queued tasks which can safely discard invalid entries.
Do not wait while holding a lock needed by the callback. Self-unregister works;
waiting from the same listener, including nested invocation, rejects instead of
deadlocking. Callback exceptions still release in-flight accounting.

`close()` invalidates all registrations and rejects subsequent registration and
dispatch. Concurrent close/removal calls serialize invalidation with registry
removal: every returning closer has closed all remaining invocation gates.
Callback captures are still released outside both locks, so their destructors may
reenter the bus. Close does not wait for running callbacks. Bus destruction closes
without blocking; callers must settle any
in-flight callbacks that borrow a bus or other target before destroying it.
Registry snapshots and invocation guards retain callback ownership as needed,
without permitting a fresh invocation after invalidation. Queued delivery is optional and described below.

## Typed channels

`EventChannel<Payload>` owns an exact channel name. Its name plus its unqualified,
copy-constructible payload type identify a channel within each bus. Equivalent
name/type values share registrations, independent of token lifetime; another
payload type with the same name is a separate channel. Legacy string/`std::any`
channels retain their application-defined type contract and are separate from typed
channels. Type identity is in-process; names and RTTI are not a serialized protocol.

```cpp
struct LevelLoaded {
    int level;
};
const CE::SubSystems::EventChannel<LevelLoaded> loaded{"level-loaded"};
int last_level = 0;
auto id = local_bus.register_listener(loaded, [&](const LevelLoaded& event) {
    last_level = event.level;
});
local_bus.dispatch(loaded, LevelLoaded{3});
local_bus.unregister_and_wait(id);
```

Typed submission requires the exact payload type rather than implicit numeric or
other conversions. It copies an lvalue or constructs from an rvalue into an owned
payload before dispatch; even `std::any` can be a typed payload without flattening
its contents. Each listener receives the existing invocation-owned payload copy
through a borrowed `const Payload&`. Copy it for retained use, and keep pointees
alive according to Payload's own ownership. A checked erased-type lookup precedes
the typed callback; inconsistent internal payloads throw `bad_any_cast` instead of
reaching that callback. Public named producers cannot target a typed channel.

Persistent registration, snapshot order, invalidation, waits, close and optional
delivery use the same listener machinery as named channels. `EventSystem` forwards
both APIs to its default bus. Initial typed-payload construction failures reach the
producer before the registry dispatch begins. Per-listener queued copying,
callback/target failures and cancellation still belong to the registered error sink;
invalidation waits protect the callback's borrowed target, not that error sink's
independent lifetime through pending-ticket destruction. No typed API chooses a
thread or changes worker-stream ordering.

`CE::window_resized_event` in `core/display/window-interface.h` is the canonical
typed channel for `WindowResized`. Native GLFW explicitly publishes both it and the
legacy named `"window-resized"` notification; this producer bridge does not make
generic named and typed channels equivalent. Register with the typed channel to
receive a checked `const WindowResized&`, optionally selecting platform or simulation
delivery as below. Its dimensions are copied, while its window pointer remains
borrowed and cannot be dereferenced on simulation. The
[display/window contract](display-and-window-contract.md#sizes-modes-and-observation)
defines observation, ordering and native failure behavior.

## Optional queued delivery

Pass a delivery callable and error sink to `register_listener`. The bus accepts
only the small `bool(Work)` delivery contract, with move-owned `Work`; its header
has no dependency on dispatchers, runtime, or worker pools. A target owns deferred
work when it returns true. False or an exception rejects it without retaining it.
It must not invoke work inline or wait for it, and must provide FIFO execution for
each listener stream. Concurrent producers serialize enqueue for a given listener.
There is no global completion order between listeners or independent targets.

Queued tasks copy their payload and retain registration state. At actual execution
they enter the same invalidation handshake as immediate callbacks. Callback
exceptions, payload/task preparation failures, rejected posts, and accepted tasks later dropped by the target reach
the required error sink. Target cancellation reports `future_error/broken_promise`;
explicit unregister/close discards pending callbacks intentionally. Error sinks
receive the original preparation/target exception; such queued failures do not
also escape from dispatch. Immediate failures still propagate to the caller. Sinks
must not throw and must remain valid through pending-task destruction; a throwing
sink terminates rather than disappearing in a discarded target future.

The local delivery ticket owns the copied payload as well as rejection reporting.
If a target destroys rejected work during its offer, final payload destruction
waits until the listener's posting lock has been released. Payload destructors,
including final shared-owner deleters, may therefore dispatch again without
reentering that lock. Payload-copy failure still reports the original exception.

The completion barrier protects the listener callback's borrowed target. An error
sink is independent: prefer an owned logging/reporting handle, not the same raw
`this`. Unregister can race a delivery already being offered; that task can still
be accepted but will not enter the invalidated listener.

`platform_event_delivery(engine.platform_dispatcher().submission())` and
`simulation_event_delivery(runtime.simulation_dispatcher().submission())` are
optional composition adapters in `core/engine/event-delivery.h`. Their saved
endpoints reject safely after service closure. `worker_event_delivery(group)` provides one serial drain stream on the group's
shared parallel capacity. Copies of that delivery callable share FIFO execution
and completion order, including across listeners. Separate adapter construction
creates independent streams. Ordinary jobs in the group remain parallel; a pool's
FIFO selection alone does not impose completion order.

Accepted stream work drains before group completion. Group closure rejects new
stream work even when a pump is active. A failed/cancelled pump releases its
pending captures outside stream locks, letting owned event tickets report loss.
Do not block a stream callback on another callback in that same stream.

## Worker-stream acceptance and cancellation

Initial pump submission is serialized with stream publication. Another producer
cannot return accepted before that submission succeeds. Closure checks share the
stream mutex with the pump's final empty-queue check. Submission failure removes
only the initiating request, and its captures leave the stream lock before error
reporting. Lock order is stream then pool; worker accounting releases the pool lock
before touching a stream or releasing callback captures.

Cancellation belongs to the submitted callable. An atomic preparing/accepted/
cancelled handshake records loss before callback entry without taking the producer's
already-held stream lock. Before publication, the producer withdraws only its own
request. After publication, unentered callable destruction abandons the stream
outside producer posting locks. This permits error-sink redispatch and capture-
destructor reentry without running another listener's cancellation under that lock.

[Validation procedures](../development/architecture-validation.md) includes controlled pump rejection,
concurrent producers, payload-copy invalidation, native-policy suppression, and
reentrant recovery. Typed identity/ownership, compile-time constraints, waits,
queued failure/error-sink lifetime and worker FIFO have additional regression
coverage. Linux/X11 typed-event and resize acceptance is complete. Remaining Windows
acceptance is shelved in the [platform plan](../planning/long-term/platform-acceptance.md).
