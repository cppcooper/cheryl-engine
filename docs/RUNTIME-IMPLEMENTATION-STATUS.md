# Runtime and framework implementation status

Prepared on 2026-09-30, extending the input work from original task base
`d690266c389f08e74efe0481c5fc6b744870ea29`. Branch and PR organization remain
deferred. Individual implementation commits are retained in the cumulative patch.
Compilation, automated tests, and real demo execution have not been run in this
patch-preparation pass, as requested.

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
| Acceptance | Supported builds, the aggregated regression suite, and real GLFW/OpenGL checks in both runtime modes. | Open. |

Source formatting, C++ syntax parsing, whitespace checks, and local mailbox replay
are preparation checks. They do not establish C++ type/link correctness, thread
correctness, GPU behavior, or platform acceptance. Any executed validation should
record its configurations, commands, and observed results here.

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

After compilation and execution are explicitly requested, build the supported
normal and sandbox configurations and run the `all-tests` regression suite.
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
