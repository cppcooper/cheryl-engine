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
dispatch. Bus destruction closes without blocking; callers must settle any
in-flight callbacks that borrow a bus or other target before destroying it.
Registry snapshots and invocation guards retain callback ownership as needed,
without permitting a fresh invocation after invalidation. Queued delivery is optional and described below.


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
exceptions, rejected posts, and accepted tasks later dropped by the target reach
the required error sink. Target cancellation reports `future_error/broken_promise`;
explicit unregister/close discards pending callbacks intentionally. Error sinks
must not throw and must remain valid through pending-task destruction; a throwing
sink terminates rather than disappearing in a discarded target future.

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
