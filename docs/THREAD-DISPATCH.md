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

These dispatchers have distinct APIs and ownership domains. Event delivery can
later adapt their endpoints through a callable without an Executor base class or
an EventSystem dependency on either dispatcher.
