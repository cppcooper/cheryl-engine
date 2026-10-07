# Thread dispatch

PlatformDispatcher accepts owned `work(EngineContext&)` and runs it on the
platform/graphics owner. SimulationDispatcher accepts owned `work()` and runs it
on the simulation owner. Both return futures and expose a copyable `submission()`
endpoint. Saved endpoints contain no borrowed service pointer and reject with
`failed_operation` before opening, after closing, and after service destruction.

The runtime owns queue opening, draining, and closing. Initialization can submit
simulation work before a concurrent worker starts; that worker binds execution
ownership before draining. Sequential mode binds the calling thread instead.
A failed initialization may close the still-unbound simulation dispatcher on the
platform because simulation ownership was never established.

Each drain detaches one FIFO batch. New posts, including posts by callbacks in the
batch, wait until a later drain. The owner never executes a submission inline,
and recursive draining does not consume reentrant posts. Queue mutexes protect
only state transfer; callbacks and cancelled capture destructors run without them.
The enqueue mutex defines order between overlapping producers, not their start
or completion times in other execution domains.

Submission owns callback captures. Borrowed captures must remain alive through
completion or cancellation. Exceptions from accepted work reach its future;
unexecuted work is cancelled by promise destruction (`broken_promise`). Do not
wait on a future from the thread required to complete it. Wake callbacks own their
scheduler state so an enqueue racing closure can safely finish its notification.

Callable arguments are decay-owned by submission. Copy endpoints for independent
producers and retain them through their calls; they do not extend runtime/adapter
lifetime. Reference captures and reference-valued results still borrow their targets.
`has_pending()` observes queued tasks only, so false does not establish that a detached
batch has finished or that acceptance has closed. Diagnostics are owned observations,
not completion barriers. Use the returned future and runtime lifecycle for completion.

Event delivery adapters use these saved endpoints through a callable. EventBus
remains independent of both dispatcher types; the adapters belong to composition
code. See [event-delivery.md](event-delivery.md).

## Simulation boundary

`runtime.simulation_dispatcher().submit(work)` accepts work after runtime
initialization opens the queue. Obtain `submission()` when a longer-lived adapter
needs to save the endpoint. At every scheduled update, the runtime drains one
simulation batch, checks stop, then transfers the entire available polling
backlog immediately before `game.update()`. No input observations are removed
merely because mailbox work ran. There is no total ordering between independent
input and mailbox producers.

In concurrent mode submission notifies the shared scheduler but does not advance
the scheduled update deadline. Delivery occurs at the next scheduled simulation
boundary; posting must not create an early simulation tick. SimulationTimingOptions
configures that cadence independently of polling. In sequential mode the same boundary runs on the calling
thread. Owner callbacks execute without the scheduler lock, permitting `stop()`
and further platform/simulation submissions.

Simulation shutdown cancels its pending batch on the simulation owner before
publishing worker completion. Initialization failure before owner binding cancels
on the initializing thread. The platform joins simulation before game cleanup.
Runtime stopping closes context CPU groups, stops/cancels simulation work,
and pumps platform requests while joining simulation and settling CPU work. A
worker awaiting accepted platform completion therefore cannot strand the owner
in a blocking join. After CPU groups settle, remaining platform requests cancel
before game/resource cleanup. A bounded 1 ms shutdown wait observes CPU completion
without making workers retain a borrowed runtime notification callback.

`game.quiesce()` runs on the platform after simulation joins and before accepted
CPU work finishes. Stop external producers and invalidate borrowed event
registrations there; do not block on callbacks/jobs that still require platform
dispatch. Keep their targets/resources alive until `deinit()`. This hook also
pairs with attempted initialization so partial startup can stop its producers.
Only context-created worker groups are closed; an injected root's unrelated
application groups are untouched. Cleanup preserves the original failure.
