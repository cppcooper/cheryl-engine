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

Initial pump submission is serialized with stream publication. Another producer
cannot return accepted merely because a pump which may still fail submission was
marked scheduled. Closure checks also share the stream mutex with the pump's final
empty-queue check. Submission failure removes only the initiating request, and
its captures leave the stream lock before error reporting. Lock order is stream
then pool; worker accounting releases the pool lock before touching a stream or
releasing callback captures. Prepared closure/reentry and independent-stream
scenarios exercise these lifecycle contracts; the old submission window was found
by interleaving review, without a deterministic executed reproduction.

Checkpoint 55 extends the handoff for accepted pumps rejected by native policy
before entry. Cancellation belongs to the submitted callable, not a producer's
local shared owner. An atomic preparing/accepted/cancelled handshake records early
loss without taking the already-held stream lock; the producer then withdraws
only its initiating request. After publication, unentered callable destruction
abandons the stream outside producer posting locks. This closes a second window
in which another listener's cancellation sink could run under the first listener's
posting lock. The finding comes from ownership/interleaving review; executed
native-policy acceptance remains open.

Checkpoint 56 adds an internal submission seam, retaining WorkerGroup::submit for
the public adapter. Controlled sources discard a pump before publication and check
same-listener sink reentry/recovery, then discard one published pump shared by two
listeners on another thread and check per-listener cancellation and recovery.
The aggregate target alone receives the private source include path. These fixtures
model loss without running an OS policy adapter; they are uncompiled/unexecuted and
do not force every old producer interleaving or prove actual affinity failure.

Checkpoint 57 also prepares throwing submission with original-error retention and
same-listener recovery. No controlled fixture invokes a pump inline while offered;
explicit test playback happens after publication. Closure guards invalidate borrowed
recording sinks before pending captures unwind on an assertion's early return.
