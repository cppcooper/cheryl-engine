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
Complete aggregate runs pass 340 native-enabled normal and 333 sandbox cases with
no skips after correcting two startup-fixture cancellation expectations; production
behavior is unchanged. Real Linux affinity mask cases execute.

Mesa 25.2.8 llvmpipe executes texture retirement, selected-context restoration and
retained-image release after engine/window destruction. Eight finite real-demo
configurations complete in both modes, including real default-font parsing/baking/
upload. Three further native cases validate retained shader-source frame generations after
cache clear, partial compile/link/file/reflection failure cleanup and live-window
shutdown with final foreign owner release. Maintenance/shutdown now unbinds the last
program so its native deletion completes before the borrowed context is destroyed.
The initial shutdown case reproduced the failure; all five native cases now pass.
The user reports the desktop checklist passed with expected behavior:
camera/mouse/text focus, resize, valid/failed/recovered F5 reload and normal close.
The checklist requests sequential and concurrent modes; GPU/driver identity and
exact local build flags were not supplied.

Real-font acceptance rejects each of 793 TrueType and 727 CFF scratch allocation
requests, before provider upload, and covers nested failures. The loader now raises
bad_alloc and releases outstanding scratch instead of accepting a partial atlas or
letting stb use null storage. Both finite demo modes pass after this change.

Forced timing/backlog/presentation overload, further native resource/failure
combinations, rotated FFont output and remaining
actual OS policy rejection/restriction acceptance stay open. The completed-task
source audit closes at checkpoint 90, including clang-format 23.1.2, declaration
inventory and controlled native/startup/retained-owner failure sources. Current
evidence and remaining task-9 gates are recorded in
[ARCHITECTURE-VALIDATION.md](ARCHITECTURE-VALIDATION.md). This is a local draft;
PR metadata publication remains separately pending.
