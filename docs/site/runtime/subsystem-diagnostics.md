# Subsystem diagnostics

Engine operational records use the named engine, os-platform, rendering, assets,
and memory destinations described in [logging.md](logging.md), with the existing
target-consistent logging profile. All writes name their destination explicitly;
the application's default logger cannot redirect them. The application owns early
explicit configuration of each used category, producer stop,
and final logger close. Closed/degraded sinks and stripped levels do not change
engine exceptions or futures. Required failure context uses bounded emergency
output independently of logging.

Records identify subsystem, operation/phase, outcome, and a monotonic process-local
domain. IDs are neither addresses nor native handles and are not persistent keys.
Zero indicates identity exhaustion. Session failures keep the original exception
and phase while distinct cleanup failures retain their own emergency context.

## Investigate a runtime symptom

Enable the relevant [logging category](logging.md#enable-useful-output), keep the
failure's operation/domain context, then select the observation below. Read runtime
statistics after synchronizing with `run()` returning or throwing; live sampling
rejects. Queue and resource snapshots have their own access rules.

| Symptom | Evidence | Next action |
| --- | --- | --- |
| Input is delayed while rendering continues | Polling-capacity WARN/recovery records, completed-session peak backlog and update counts | Check whether a slow simulation fills lockstep/finite capacity. Reduce blocking update work; choose a polling policy and spacing appropriate to the application. Increasing capacity cannot reconstruct native observations that were never pumped. |
| Updates and rendered frames differ | Published, rendered, superseded and skipped-publication counts | Account for independent pacing, reused frames and backpressure. Keep gameplay progression in update, since preparation can be skipped. |
| An upload fails after some assets become available | Loader batch/provider identity, completion flag and publication/replacement counters | Retain the original upload error. Retry from fresh or preserved CPU preparation; do not assume rollback or that a reused key replaced existing content. |
| Worker work fails or falls back | Job future, group policy/status and its pool/group domains | Inspect effective eligibility and available capabilities. Required unsupported policy rejects; a preferred fallback is observable rather than a promised placement or speedup. |
| Shutdown waits | Futures, dispatcher/group observations and producer lifetimes | Remove same-owner waits and settle external producers while dependencies are alive. An empty queue does not prove that its detached batch finished. |
| A resource appears to remain allocated | Cache/application/frame owners and native live/pending statistics | Follow the ownership sequence below; handle counts do not measure physical GPU bytes. |

The [timing guide](simulation-timing.md), [polling contract](input-state-model.md#polling-backlog-and-scheduling)
and [Threads and Data Flow](../architecture/threads-data-flow.md) explain the
corresponding scheduling decisions. A counter establishes its documented operation,
not a completion barrier or a diagnosis by itself.

### Trace retained resource owners

1. Release the application's handles that no longer need the resource. Inspect
   materials and uploaded text/UI scenes as well as the original sprite/image.
2. Check the relevant strong caches. Clearing a sprite cache drops that cache's
   ownership; an image or shader catalogue can independently retain its resources.
   Serialize clear/replacement with loading and teardown on the provider owner.
3. Allow published frames to finish or be superseded and recycled. The platform
   retains its latest complete frame between publications, so dropping the game's
   handle alone does not release that frame's copy.
4. Compare native live and pending retirement observations. Final logical release
   queues retirement; runtime owner-thread maintenance collects it with the correct
   context current. Long presentation/update work or OS scheduling can delay that
   opportunity.

The [residency guide](../resources/resource-residency.md) defines cache ownership,
maintenance and shutdown. No unused-asset eviction or residency budget is supplied.
Do not issue native deletion from another thread to force an observed count down.

## Runtime statistics

`CE::Runtime::RuntimeStats` is published after the simulation worker joins. Callers synchronize
with run start/return and may inspect an unstarted/completed session; live sampling
rejects. Counts describe update attempts, completed polls, published/rendered frames,
supersession, skipped publication, dropped timing batches/duration, peak polling
backlog, and resize observations. Normal ticks/frames emit no INFO records. Session
start/capabilities/end and one final DEBUG summary are bounded by session lifecycle;
input observations count records and focus epochs without retaining their payloads.
Input capability/focus records reside in os-platform.log; lifecycle/timing/frame
summaries reside in engine.log.
Each runtime owner emits its own DEBUG summary at most every two seconds, outside
the scheduler lock. Repeated lag drops and polling capacity held for at least
100 milliseconds produce coalesced WARN records at that cadence; recovery produces
one INFO transition. A single timing drop remains visible in counters/DEBUG without
a shutdown warning. Final summaries include polling backpressure observations.

## Native resource statistics

`CE::Rendering::OpenGL::ResourceStats` counts registrations, live/pending native handles, collection,
shutdown deletion, and abandonment for one resource lifetime. Taking the snapshot
performs no GL calls or logging. Final release only queues retirement. Abandonment
and failed untracked cleanup use emergency records after the lifetime lock releases;
they never try to recover by issuing GL calls on an unavailable context. Explicit
renderer shutdown reports lifecycle totals, while destructor recovery has no ordinary
logger dependency.

## Worker statistics

`CE::Workers::GroupStatus` correlates group and physical pool domains and adds callback
failure, peak pending, and cumulative queue-wait nanosecond observations. Job
exceptions still reach only their futures. Explicit pool/group report_diagnostics
observers run without application/collection locks: pool capacity is announced once,
preferred fallback once, and native policy failures as newly observed count ranges.
`CE::Runtime::Context` invokes them after group publication and after drain; standalone
pool owners choose their own observation boundary. Constructors and pool destruction
do not log inside a containing context lock. Unsupported required policy/startup
is reported at the context operation owner before the original error propagates.

## Dispatcher statistics

`CE::Diagnostics::DispatchStats` covers accepted, completed, cancelled, failed, queued and detached
batch counts. Callback failures are counted before settling their own futures.
Opening/binding/closing uses DEBUG; queue peaks of at least 1024 produce a single shutdown
warning. This is an observation threshold, not a new admission limit. Close during
a callback can leave detached work executing, so the final joined snapshot is more
complete than that close record. No per-request ordinary emission occurs.

## Event statistics

`CE::Events::EventStats` counts registration, dispatch, invocation, explicit discard and active
queued-delivery failures. Registration IDs remain bus-qualified, with bus_id for
correlation. The listener retains a separate counter owner without retaining the
bus/registry. `CE::Events::EventBus::ErrorHandler` remains the sole queued-error owner, including cancelled
active work; explicit unregister/close discards do not generate additional errors.
report_diagnostics is explicit because event operations may run inside a platform
callback or a containing delivery lock. It samples no names or payloads.

## Asset diagnostics

`CE::Assets::Loading::Prepared` carries a batch identity through copies, worker preparation and
platform upload. `CE::Rendering::Resources::Provider` identities identify adapter instances, while cache
snapshots count cumulative publications/replacements separately from current entries.
`CE::Assets::Loading::Loader` records batch begin/end, counts and elapsed time. Its last
`CE::Assets::Loading::UploadStats` includes
completed image/manifest calls and actual newly published entries, even when failure
occurs inside a manifest. Diagnostic counter sampling is best effort and cannot
replace an upload error; unavailable counts are explicit. Reused keys are completed
calls, not new publications. Neither counts nor records promise batch rollback.
Per-key program/material reload records preserve the prior generation on failure;
keys/paths and shader source are omitted from operational records.

## Shader diagnostics

Explicit `CE::Rendering::OpenGL::Program::print_active_*` calls retain the existing reflection/error
contract and emit DEBUG count summaries and TRACE name/type/size/location records.
No direct stdout tables remain. Query APIs already serve reflection consumers; the
selected consumer does not require an additional event transport. Successful shader
compiler/linker diagnostics are WARN presence/byte-count records, without copying
driver text/source into the ordinary log.

## Memory statistics

`CE::Memory::Stats` samples the shared byte-domain collections before formatting/emission.
It counts total/available bytes, owners, partitions and pending release owners.
Normal lookup/reuse/split/merge misses remain quiet. Explicit preallocation reports
completed blocks/width or a partial attempt; report_diagnostics is a host-triggered
DEBUG snapshot, so the host chooses the sampling period. These are bookkeeping
observations, not allocation-pressure/eviction budgets.

## Platform diagnostics

GLFW initialization installs a bounded numeric error observer before the first
`CE::Platform::GLFW::DisplaySystem` initializes the library. It chains the existing host callback with
the borrowed description, catches a throwing host, and retains no description.
Ordinary reporting happens after native callbacks and library lifetime locks have
returned, with a two-second error cadence. Required native failures use emergency
operation/code records. Final termination restores the borrowed callback while
preserving a later host replacement. These lifecycle operations belong to the
platform/main thread, consistent with [GLFW's callback contract](https://www.glfw.org/docs/latest/group__init.html).
Destructor observations use emergency reporting only.

Display/window records describe monitor counts, window mode and logical/pixel
dimensions without titles. Resize callbacks accumulate counts; an unlocked
platform observation emits the latest dimensions at most every two seconds.
Routine input records describe State/Event/Text/Focus capability, native attachment,
window focus, routing focus epochs and gamepad availability transitions. Focus
routing is queried only when the adapter advertises support; observation failures
cannot replace runtime failures. Routine records omit character data, physical keys
and event payloads.

Explicit [controller tracing](../modules/native-glfw.md#controller-diagnostics)
adds TRACE gamepad callback/sample IDs and values, including callbacks with unresolved
device IDs, and a one-second sampled-state heartbeat. The native adapter's GLFW
keyboard, pointer and committed-text values are not traced. This opt-in observer
does not change input mapping and is separate from HID/device-notification acceptance.

## OpenGL diagnostics

`CE::Rendering::OpenGL::Renderer::set_native_diagnostics(true)` opts in before startup. Debug output
requires GL 4.3 or KHR_debug and the loaded entry points; OpenGL 3.3 remains the
renderer baseline. Existing host callbacks prevent installation. When installed,
the callback uses synchronous delivery and a stack-scoped thread-local observer
around renderer-owned commands; GL receives no renderer user pointer. This follows
the synchronous callback rules in [KHR_debug](https://registry.khronos.org/OpenGL/extensions/KHR/KHR_debug.txt).
The callback performs no allocation, ordinary logging, GL/window calls or driver
text copying. It counts high/medium severities and filters lower severities.
Resource maintenance coalesces ERROR/WARN counts at most every two seconds;
explicit shutdown reports remaining counts after native restoration. Host/raw
upload commands outside renderer scopes are not observed.

Successful shutdown restores the callback/user parameter and output/synchronous
flags on the owning current context, unless a host has replaced the callback.
Failed context recovery performs no restoration calls. The remaining static
callback has no captured owner and ignores commands after its scope disappears,
including after renderer destruction. This avoids a dangling renderer pointer
without taking ownership of host callback state.

## Validation and limits

The diagnostics acceptance runner checks bounded INFO records in engine/os-platform,
matching runtime/input domains, no normal WARN/ERROR output, and absence of input
payloads. A separate process verifies actual asset, shader reflection and memory
ownership records use their named files, without initializing the legacy destination:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  python3 projects/tests/diagnostics.py path/to/developer-build
  python3 projects/tests/diagnostics.py path/to/off-build --info-stripped
)
```

It uses previously built tests. Recording tests cover capability rejection and
callback ownership;
native_opengl.debug_output separately exercises a real supported context when
CHERYL_NATIVE_GL_TESTS=1.

These counters describe operations and handle ownership, not GPU allocation bytes,
durability, scheduler guarantees, or race freedom. Native recording tests establish
engine behavior; actual driver/platform acceptance is separate.
