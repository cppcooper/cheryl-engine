# Ownership and Cleanup

Decide what an application component owns, what it borrows, and which work must
finish before its dependencies can be destroyed. A C++ handle's lifetime and a
native backend's usable lifetime are separate obligations.

## Choose what to retain

| Relationship | Application responsibility | Cheryl examples |
| --- | --- | --- |
| Own an object | Control destruction and finish its callers before releasing it. | A Startup result owns its context; the application owns its game, GUI session and audio system. |
| Borrow an object | Keep its owner alive and obey the object's access rules. | A runtime borrows its context/game; context accessors and display-owned windows return borrowed access. |
| Retain an immutable result | Keep a strong handle for as long as that version is needed. | Assets, input poll snapshots, uploaded text and frame packets retain their selected data. |
| Hold a scoped request | Retain the lease while the request is wanted; release it when that consumer finishes. | Input Events/Text capture and keyboard focus. |
| Save a submission endpoint | Own callback inputs or keep borrowed captures alive through completion or cancellation. | Dispatcher submissions remain safe to call after closure, but reject new work and do not retain the runtime. |

For example, two game objects can retain the same sprite while keeping independent
positions and animation cursors. Dropping one object's handle releases only that
owner. A manager cache or published frame can still retain the sprite's resources.
The [resource guide](../resources/resource-residency.md#cache-ownership) explains
which owners to inspect before treating continued residency as a leak.

## Use a lease for a temporary input request

With a live input system and an application-selected nonzero `editor_target`:

```cpp
auto events = input.capture(CE::Input::Capture::Channel::Events);
auto text = input.capture(CE::Input::Capture::Channel::Text);
auto focus = input.routing().focus(editor_target);
// Retain these handles while the editor receives input.
// During update, read tick.input.records_for(editor_target) in order.
focus.reset();
text.reset();
events.reset();
```

Keep the handles as editor members when the request spans updates. Their
destructors also release the requests. An older focus lease cannot clear a newer
request. These leases retain request state, not an input adapter, window or widget;
capture and focus take effect at a subsequent poll. Toolkit sessions manage their
own leases through their focus methods. The
[input guide](../runtime/input-state-model.md#focus-routing-and-editing-controls)
defines poll epochs and already captured records.

## Keep native domains alive for native use

A retained draw packet keeps geometry, material and image generations alive. It
does not keep a destroyed graphics context usable. Likewise, an audio voice handle
can survive system closure but then reports `Closed`; controls reject.

CPU release state can have a different lifetime. Pooled object handles retain
their return/construction contexts, allowing final release after a pool facade
dies. Their own callers still need to finish before that facade is destroyed.
Use [CPU memory lifetime](../resources/resource-lifetime.md) when allocating
application objects, and the backend's native retirement contract for GPU handles.

## Shut down in dependency order

The runtime provides the sequence below for context-owned work. Application-owned
services join that sequence through the game's hooks:

| Boundary | What the application does |
| --- | --- |
| Request stop | Return from the current update when no further game work is needed. A stop request does not interrupt an executing callback. |
| Simulation joins | Finish game updates and frame preparation before transferring UI teardown to platform. |
| `quiesce()` | Stop external producers and invalidate borrowed event listeners while their targets remain alive. Avoid waiting for work that still needs platform dispatch. |
| Accepted work settles | Keep worker inputs, upload dependencies and callback targets alive while platform continues servicing accepted dependencies. |
| Frames recycle and `deinit()` runs | Release game resources and application-held toolkit objects; close GUI sessions after UI quiescence. Settle and close separately owned audio services. |
| Adapters close | Input detaches from a live window; native resources retire before graphics/window destruction. |

Cleanup pairs with attempted initialization, so retain only successfully acquired
handles and make cleanup tolerate partial startup. For borrowed `[this]` event
callbacks, use `unregister_and_wait()` before destroying the target; ordinary
unregister prevents new entry but permits an already running callback to finish.

The [runtime lifecycle](../runtime/runtime-frame-boundary.md#shutdown) owns the
exact shutdown and failure order. [Threads and Data Flow](threads-data-flow.md)
explains which dependencies require continued owner-thread progress.
