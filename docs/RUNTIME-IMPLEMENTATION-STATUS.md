# Runtime and framework implementation status

Current continuation: original task base
`e8c9788f63cf4688e144b84feeb8f9aabf62540f`, branch
`refactor-runtime-render-resource-architecture`. Tasks 1–5 of the second-pass
work order are implemented in source. **This continuation has not been compiled
or tested.** Historical normal/sandbox build results below predate these changes
and do not validate the current HEAD. Individual local commits remain intact.

Earlier input/runtime work began at
`d690266c389f08e74efe0481c5fc6b744870ea29`. Its compilation follow-up is retained
below as historical evidence; automated tests and real demo acceptance remain
open. Source preparation checks are separate from executed acceptance.

| Work item | Prepared source and regression coverage | Execution state |
| --- | --- | --- |
| Backend composition | Neutral display/window, presentation, input, renderer, and resource contracts; GLFW/OpenGL factory; in-memory adapter probe without native API headers. | Not executed. |
| Simulation and frames | Sequential scheduling, concurrent simulation worker, whole input batches, latest-frame handoff, reusable slots, immutable draw commands, and per-entity animation values. | Not executed. |
| Input contract | State timing, polling policies, ordered Events/Text, scoped capture, focus routing, adapter capabilities, and demo textbox. | Not executed; detailed acceptance remains in the input status document. |
| Lifecycle and ownership | Owned or borrowed input, single-use runtime/context sessions, cleanup after partial initialization, original-failure preservation, worker join, frame recycling, and input-before-window destruction. | Not executed. |
| Platform requests | Owned/move-only callbacks, future results and isolated failures, FIFO batch drains, shared scheduler wake, dispatch while polling is paused, and shutdown cancellation that destroys pending captures on the platform. | Not executed. |
| Materials and camera | Semantic pass/draw parameters, configurable GLSL names, cache-only linking, explicit successful-replacement reload, retained old programs, and scalar camera revision comparisons. | Not executed. |
| Cache publication | Shared-lock lookups, unique-lock publication, retained handles, destruction outside cache locks, loading-thread/provider checks, and refill rejection during provider teardown. | Not executed. |
| Asset preparation and upload | Owned loaders with independent roots, fresh deterministic scans, worker-safe parsing/decoding, validation against owned pixels, metadata snapshots, explicit application bootstrap, and transient vertex upload. | Not executed. |
| OpenGL context and lifetime | Owner plus actual-current-context guards, upload/draw bounds, deferred retirement, context restoration for shutdown, closed-handle rejection, and failure invalidation without GL calls. | Not executed. |
| Acceptance | Normal and sandbox builds pass; the aggregated regression suite and real GLFW/OpenGL checks in both runtime modes remain unrun. | Execution open. |

Source formatting, C++ syntax parsing, whitespace checks, and local mailbox replay
are preparation checks. They do not establish C++ type/link correctness, thread
correctness, GPU behavior, or platform acceptance. Any executed validation should
record its configurations, commands, and observed results here.

## Compilation follow-up

Based on pushed commit `84df4f39b26f8d21ccabee7e722ac1585ff4338e`, all default
targets compile and link with GCC 13.3, C++23, and CMake 3.31.10 on Linux:

- Normal Release configuration: X11 enabled, Wayland disabled, Gainput samples/tests
  disabled. The library, `demo`, `all-tests`, `cpp-testing`, and `backward-cpp` build.
- Sandbox Release configuration: `CHERYL_SANDBOX_BUILD=ON`. The library,
  `all-tests`, `cpp-testing`, and `backward-cpp` build; the demo is omitted by design.

Both configured builds complete successfully with:

```sh
cmake --build build-normal --parallel 3
cmake --build build-sandbox --parallel 3
```

GLAD uses its pinned specification with `REPRODUCIBLE`; its selected Python 3.12
interpreter has Jinja2 installed. Both configurations use
`CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST` so building does not execute
the test binary. No automated tests or real demo checks were run. These results
establish compilation/linking for the configurations above, not runtime acceptance.

## Discoveries resolved in this continuation

- Failed game initialization skipped `deinit()`, and stopped adapters could be
  reused. Cleanup now pairs with attempted initialization and rejects graph reuse.
- Simulation had no owned request path for graphics-thread work. The queue now
  transfers values and results; cancelled futures do not retain callback captures.
- Common draw code embedded GLSL uniform names, cache loading broadcast camera
  state, and the camera test called a removed API. Semantic bindings and explicit
  camera publication replace those conventions. F5 uses actual program reload.
- Asset maps and provider binding lacked publication/teardown synchronization.
  Readers now copy stable handles and teardown prevents reentrant cache refill.
- Loader singleton roots, stale file discovery, and host-font/shader side effects
  coupled generic loading to process state. Owned roots, fresh preparation, and
  application bootstrap replace that coupling.
- Pixel decoding and upload were combined, and shared vertex ownership obscured
  the transient contract. Owned prepared pixels and a span upload make the boundary
  explicit. OpenGL byte/count/range checks guard the corresponding uploads.
- A correct thread could have another OpenGL context current. Resource use now
  checks the selected context, and closed handles never query a borrowed context.

## Remaining acceptance

When execution is explicitly requested, run the `all-tests` regression suite in
the normal and sandbox configurations; rebuild if source or configuration changes.
Include runtime-adapter, platform-request, material/cache, asset-preparation,
OpenGL-lifetime, camera, manifest, rendering, and input cases. Sandbox adapters
cannot establish real GLFW/Gainput or GPU behavior.

Run the real demo sequentially and with `--concurrent`, including finite and
unlimited input backlogs, slow simulation/presentation, resize, and close. Confirm
F5 replaces the shader when linking succeeds and retains the last program when
linking fails. Confirm `--full-assets` still explicitly loads its font and shader.
Exercise CPU preparation followed by queued upload without blocking simulation.
Check resource requests during polling backpressure and shutdown cancellation.

Check partial game/input/renderer initialization and loop failures, preserving
the original error while completing cleanup. Confirm workers join before game
cleanup, pending captures and frame handles release on the platform, callbacks
detach before window destruction, and a second runtime cannot reuse the graph.
With a real context, switch another context onto the owner thread: use must fail
until the selected context is restored, and teardown must restore that context
before deletion. Check retained handles after cache clear and renderer closure.

The input-specific sequence remains in
[INPUT-IMPLEMENTATION-STATUS.md](INPUT-IMPLEMENTATION-STATUS.md).

## Scope limits

Singleton asset caches still support one active provider at a time. Repeated
generic loads preserve existing keys; upload is not an atomic hot-reload
transaction, and failed upload may leave completed cache entries while published
metadata stays unchanged. Platform callbacks own their data and preserve the
session's current context; application-owned preparation workers must finish
before their borrowed objects are destroyed.

Tile-map neighbor selection, autotiling gameplay, application view/orientation
meaning, audio, networking, world/physics systems, and full text editing/shaping
are separate work. Committed text scalars do not add IME composition, grapheme,
clipboard, or Unicode font shaping support.


## Execution architecture checkpoint

Completed in source: PlatformDispatcher rename and safe saved endpoints;
SimulationDispatcher delivery before whole-backlog input transfer; persistent
EventBus registrations, removal/in-flight barriers, owned queued delivery and
observable failure sinks; serial event streams on WorkerGroup; owned WorkerPool,
futures, weighted group scheduling, concurrency caps, capability-aware Linux CPU
policy, effective-mask verification, and explicit unsupported hard topology paths.

EngineContext supports lazy owned workers or an injected shared root. Both runtime
modes close context groups, stop simulation, quiesce application producers, and
pump platform requests while accepted work settles. Unrelated injected groups
remain open. Remaining platform requests cancel before frames/game/resources are
released. Partial startup and the first failure survive later cleanup failures.
The GLFW factory forwards execution configuration, and the loader example uses
an owned worker-to-platform handoff without blocking simulation.

Regression scenarios are prepared for event lifetime/order/cancellation, saved
dispatch handles, worker results/closure/caps/shares/CPU eligibility, and shutdown
with platform-dependent work. They have not been compiled or executed. Static
syntax parsing does not establish C++ type correctness or concurrency behavior.

Timing source is prepared: both modes share variable/fixed pacing, bounded
fixed/drop recovery and direct/hybrid VariableCatchUp, separate observation input
durations, dropped-time reporting, and retained latest frames. Scheduler/input/runtime
regressions are prepared, not compiled or executed. See [SIMULATION-TIMING.md](SIMULATION-TIMING.md).

Continue with task 6 (residency and idle retirement), task 7 (pipeline/material
contracts), and task 8 (resolved render packets). The numbered work order and discoveries are retained in
[ARCHITECTURE-WORK-LEDGER.md](ARCHITECTURE-WORK-LEDGER.md).
