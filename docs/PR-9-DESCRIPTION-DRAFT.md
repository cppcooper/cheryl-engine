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

Aggregate regression sources cover dispatch/event lifetime,
worker policies, timing/input recovery, cache/native ownership, material parameters,
frame/text retention, reload, and failure cleanup. Changed-source syntax inspection,
whitespace/API/ownership review, and mailbox replay are preparation checks only.
Current normal/sandbox Release builds pass, including the normal demo. Complete
aggregate runs pass 332 normal and 330 sandbox tests with no skips after two
startup-fixture cancellation expectation corrections; production behavior is unchanged.
Real Linux affinity mask cases execute. Real sequential/concurrent demos, OS policy
rejection, driver reload/retirement/context recovery and font rendering remain open.
Earlier build reports are historical. The completed-task source audit closes at checkpoint 90, including clang-format
23.1.2, declaration inventory and controlled native/startup/retained-owner failure
sources. Controlled runtime and Linux affinity scenarios have executed; real GPU/demo/font
acceptance remains open. Current evidence and remaining gates are recorded in
docs/ARCHITECTURE-VALIDATION.md.

Native follow-up: Mesa 25.2.8 llvmpipe executes real texture retirement, selected-
context restoration and retained-image release after engine/window destruction.
The full native-enabled normal suite passes 334 cases; sandbox passes 330, with no
skips. Eight finite real-demo configurations complete in both modes. These results
cover the software driver and configured paths; desktop hardware/input/visual/
resize/reload observations, forced overload and remaining font/OS failures stay open.
