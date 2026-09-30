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
without permitting a fresh invocation after invalidation. Queued delivery follows
as a separate subtask.
