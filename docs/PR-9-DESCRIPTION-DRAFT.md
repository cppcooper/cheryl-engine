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

Validation prepared: aggregate regression sources cover dispatch/event lifetime,
worker policies, timing/input recovery, cache/native ownership, material parameters,
frame/text retention, reload, and failure cleanup. Changed-source syntax inspection,
whitespace/API/ownership review, and mailbox replay are preparation checks only.
Current changes have not been compiled or executed. Normal/sandbox builds, aggregate
regressions, real sequential/concurrent demos, affinity failures, native reload and
retirement, and shutdown/failure acceptance remain open. Earlier build reports are
historical. The completed-task source audit closes at checkpoint 90, including clang-format
23.1.2, declaration inventory and controlled native/startup/retained-owner failure
sources. No executed native/OS/runtime acceptance is claimed. The evidence and
remaining gates are recorded in docs/ARCHITECTURE-CONVERGENCE-REVIEW.md.
