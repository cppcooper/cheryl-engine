# Runtime and concurrency

Choose game-hook boundaries, timing, window observations and work delivery.
[Runtime](../engine-systems/runtime.md) introduces sessions and hooks;
[Threads and Data Flow](../architecture/threads-data-flow.md) helps choose between
owner dispatch, worker jobs, event delivery and immutable publication.
For a background-work question, start with that decision guide, then use the
matching detailed procedure below. For a first runnable session, start with
[First Game](../getting-started/first-game.md) and return here for lifecycle choices.

```{toctree}
:maxdepth: 1
:titlesonly:

Ownership and cleanup <../architecture/ownership-cleanup>
Threads and data flow <../architecture/threads-data-flow>
Initialize, run and stop a session <runtime-frame-boundary>
Choose simulation timing and recovery <simulation-timing>
Observe and operate windows <display-and-window-contract>
Deliver typed events <event-delivery>
Dispatch to an owner thread <thread-dispatch>
Configure worker pools and groups <worker-execution>
Application startup reference <../modules/startup>
Worker configuration and topology <../modules/workers>
Native GLFW platform reference <../modules/native-glfw>
```
