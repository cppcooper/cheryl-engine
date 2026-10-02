# Refactors runtime execution and retained rendering architecture

Runtime execution now separates platform and simulation dispatch, persistent event
registrations, and shared CPU worker groups. Queued event delivery owns payloads,
preserves stream ordering, reports failures, and supports explicit unsubscribe and
in-flight barriers. Worker groups enforce concurrency caps, weighted scheduling,
and capability-aware CPU eligibility. Both runtime modes service accepted platform
dependencies during shutdown before releasing application and rendering resources.

Simulation timing uses bounded variable/fixed recovery with separate observation
input durations and dropped-time reporting. CPU submission resolves immutable
pipeline/material contracts into retained frame packets. The renderer plays those
packets with explicit parameters, image units, geometry, and full supported state;
reload preserves old generations held by published frames. Strong cache residency,
owner/current-context guards, deferred native retirement, and idle maintenance
govern resource lifetime. Immediate high-level asset drawing APIs are retired.

Aggregate regression sources cover dispatch/event lifetime, worker policies,
timing/input recovery, cache/native ownership, material parameters, frame/text
retention, reload and failure cleanup. Both normal/sandbox Release builds pass.
Complete aggregate runs pass 349 native-enabled normal and 335 sandbox cases with
no skips. Startup-fixture cancellation expectations match accepted-request
broken_promise behavior. Real Linux affinity mask cases execute.

Mesa 25.2.8 llvmpipe executes texture retirement, selected-context restoration and
retained-image release after engine/window destruction. Eight finite real-demo
configurations complete in both modes, including real default-font parsing/baking/
upload. Three further native cases validate retained shader-source frame generations after
cache clear, partial compile/link/file/reflection failure cleanup and live-window
shutdown with final foreign owner release. Maintenance/shutdown now unbinds the last
program so its native deletion completes before the borrowed context is destroyed.
The initial shutdown case reproduced the failure; its five native cases pass.
The user reports the desktop checklist passed with expected behavior:
camera/mouse/text focus, resize, valid/failed/recovered F5 reload and normal close.
The checklist requests sequential and concurrent modes; GPU/driver identity and
exact local build flags were not supplied.

Real-font acceptance rejects each of 793 TrueType and 727 CFF scratch allocation
requests, before provider upload, and covers nested failures. The loader now raises
bad_alloc and releases outstanding scratch instead of accepting a partial atlas or
letting stb use null storage. Both finite demo modes pass after this change.

Two further native cases validate actual rotated FFont packets, both banks,
widths/spaces, transformed newline and retained resource retirement. They exposed
RGBA atlas rows being uploaded in the wrong order. File/raw and provider RGBA
uploads now reverse source rows to match geometry UVs, while caller pixels and
stb alpha ordering stay intact. Driver readbacks verify real PNG decoding and
all three image creation routes. All seven native cases pass; font acceptance
is complete for the recorded scopes. Both finite demo modes pass after this fix.

Three additional native runtime cases force slow updates under four timing
policies in both modes, hold real-swap presentation while concurrent updates
continue despite occupied frame slots, and fill actual State polling capacity
while platform requests and retained-frame presentation remain live. Eight updates
and two preparations are observed during the held presentation; ordered full-batch
consumption and resumed polling pass. All ten native cases execute. These are
controlled workloads on the software driver, not physical compositor stall claims.

An additional X11 native case sends synthetic server keys through normal GLFW
key/character conversion in both modes under fixed-drop/capped catch-up. Eleven
combined records retain order and old/new focus metadata across capture replacement;
a full concurrent batch holds later server events until polling resumes. Exclusive
State suppression and one pass-through semantic tap survive recovery without replay.
All eleven native cases execute; bounded timing/input/presentation acceptance is
complete with prior demo/desktop evidence. Physical keyboard/IME coverage is separate.

A further native case runs six initialization/partial-frame/presentation failures
across both runtime modes with real resource creation and accepted CPU/platform
uploads. Cleanup preserves the original error through a later deinit exception,
restores the actual unbound context, and deletes driver handles while the window
remains alive. All twelve native cases execute.

Two production Linux affinity checks narrow/restore only the caller's inherited
mask, verify worker discovery and required/preferred eligibility, and send an
in-range absent-CPU request to the kernel. The actual Invalid argument error reaches
the callback future, followed by successful required-group recovery on the same
worker. Controlled pre-callback policy failures retain their separate evidence.

Bounded task-9 technical acceptance is complete at checkpoint 110. The completed-task
source audit closes at checkpoint 90, including clang-format 23.1.2, declaration
inventory and controlled native/startup/retained-owner failure sources. Current
evidence and bounded task-9 acceptance closure are recorded in
[ARCHITECTURE-VALIDATION.md](ARCHITECTURE-VALIDATION.md). This is a local draft;
PR metadata publication remains separately pending.
