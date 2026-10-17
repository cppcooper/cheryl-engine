# Runtime and Concurrency

Choose game-hook boundaries, timing, window observations and work delivery.
[Runtime](../engine-systems/runtime.md) introduces sessions and hooks;
[Threads and Data Flow](../architecture/threads-data-flow.md) helps choose between
owner dispatch, worker jobs, event delivery and immutable publication.

```{toctree}
:maxdepth: 1

Initialize, run and stop a session <runtime-frame-boundary>
Choose simulation timing and recovery <simulation-timing>
Observe and operate windows <display-and-window-contract>
Deliver typed events <event-delivery>
Dispatch to an owner thread <thread-dispatch>
Configure worker pools and groups <worker-execution>
```
