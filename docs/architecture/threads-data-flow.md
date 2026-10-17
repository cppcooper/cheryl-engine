# Threads and Data Flow

Choose where work runs, give that work stable inputs, and publish owned results
at the next appropriate boundary. Sequential and concurrent runtime modes use the
same ownership rules; concurrent mode makes crossing them explicit.

## Choose where work runs

| Work | Execution owner | Transfer mechanism |
| --- | --- | --- |
| Poll native input, operate a window, upload, draw or present | Platform/graphics caller of `run()` | Platform dispatcher for requests from other owners. |
| Change authoritative game state or prepare a frame | Simulation; the calling thread in sequential mode or one dedicated thread in concurrent mode | Copied ticks, simulation dispatcher and completed result adoption. |
| Decode files, shape text or perform independent CPU preparation | Worker pool | A group submission with owned inputs and a future result. |
| Advance audio playback | Audio implementation's output clock | Synchronized audio controls; close it separately from the runtime adapters. |

The dedicated simulation thread is separate from general worker capacity. A CPU
worker has no graphics affinity. Toolkit widgets and documents belong to their
simulation/UI owner; native callbacks collect observations instead of calling them.
The [Engine Systems overview](../engine-systems/index.md#execution) maps these
owners to their systems.

## Choose how work is delivered

| Need | Use | Completion and ordering |
| --- | --- | --- |
| Perform an operation on its current owner | A direct call | It completes or throws before returning; the caller supplies valid access. |
| Reach the platform or simulation owner | A dispatcher submission | Always queued, with a future; one detached FIFO batch drains at an owner boundary. |
| Use independent CPU capacity | A worker-group job | A future reports its outcome; parallel jobs can finish out of submission order. |
| Notify registered consumers | A typed event channel | Immediate listeners run on the producer; queued listeners select delivery and an independent error sink. |
| Publish only the latest value | A synchronized snapshot or immutable generation | Readers retain a stable version; intermediate versions can be superseded. |

Use an event when subscribers should react to an occurrence. Use a latest-value
snapshot when a consumer needs current state rather than every occurrence. An
empty queue or a diagnostic count does not prove that detached work has finished.

## Prepare and publish a replacement

Asset loading, text updates and UI publication share this sequence:

```text
Simulation: request replacement; keep the current version
    -> Worker: prepare owned CPU data
    -> Platform: upload and return a complete candidate
    -> Simulation update: adopt the ready candidate
    -> Frame preparation: copy values and retain selected resources
    -> Platform: render and recycle the completed frame
```

A worker-to-platform request carries CPU data and retained inputs, not a pointer
to mutable game state. The platform callback returns a candidate instead of
modifying simulation through a captured `this`. Simulation checks readiness before
calling `get()` and handles any future error according to the game's failure policy.
Bound outstanding replacements to avoid an ever-growing queue of obsolete work.

For [text replacement](../getting-started/how-to/update-text.md), old frames retain
their matching text geometry and atlas while a newer generation is adopted. For
[asset batches](../assets/asset-loading.md#publication-and-retry), upload can publish
entries incrementally; a failed batch does not roll back earlier publications.
Choose the application's adoption boundary separately from the cache's behavior.

## Keep readers independent of live producers

Input delivers immutable observations; the tick's borrowed views last for that
update. Copy owned data for longer use. Frame preparation copies camera matrices,
transforms and selected animation cells so rendering needs no live entity or cursor.
GUI recordings own pixels and triangles so upload needs no live widget tree.

Immutable snapshots and resource generations solve the same lifetime problem:
consumers keep the version they were given while the producer prepares another.
They do not synchronize mutable objects that remain outside the snapshot.

## Preserve progress during waits and shutdown

Never block an owner on a future that requires that owner to complete it. A worker
can also strand its children by waiting on jobs in the same saturated pool.
Simulation dispatch waits for the next scheduled update; posting does not invent
an early tick. Worker FIFO selection alone does not guarantee callback completion
order; queued event delivery supplies a serial stream where that is required.

During shutdown, stop producers while their targets remain alive and let accepted
dependencies settle before releasing them. Runtime continues pumping platform
dependencies while joining simulation and draining its tracked groups. Follow
[Ownership and Cleanup](ownership-cleanup.md#shut-down-in-dependency-order) for
application-owned services. The detailed [dispatch](../runtime/thread-dispatch.md),
[worker](../runtime/worker-execution.md) and [event](../runtime/event-delivery.md)
guides define cancellation, closure and delivery guarantees.
