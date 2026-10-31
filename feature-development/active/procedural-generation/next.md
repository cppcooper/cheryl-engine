# PROCGEN-U17 partial: Engine-backed weather memory and bounded CPU updates

Fresh-session recovery starts at exact clean
`b1bc851778fee46fd97658a554de1cc30da2d9bf` on `feat/procedural-generation` in the
same owned clone. The local-revival manifest owns this unit. The unavailable old
thread/runtime remains archived; U16 is still **blocked/checks_passed=false**.
No sibling repairs, integration, external boards, services or publication are used.

The useful implemented subset now builds actual Engine memory/object services.
Opt-in `PROCGEN_ENGINE_MEMORY` links source-tree `Cheryl::MemoryCPU`: the existing
Engine diagnostics, exceptions and logging implementations plus the exact pinned
ctti/backward-cpp/spdlog closure. It must not be linked alongside another Engine
implementation. Block storage includes pointer helpers instead of the graphics
math umbrella. The default standalone configuration remains available.

DefaultAllocator and the class-slot ObjectPoolAllocator use primary allocator
traits, explicit rebinding, generic subobject construction, count checks and
retained checkout/return authority. PoolState owns its actual byte-manager facade.
Native vector growth/copy/move, non-inline string, byte and typed map nodes,
over-alignment, overflow, generic construction, constructor/duplicate refusals,
post-facade growth, final lifetime and odd-substep ownership checks execute.
Instance-free nonstandard traits helpers are removed; inspected in-repo callers
already pass allocator instances. ObjectAllocator remains an independent legacy
adapter and was not repaired. Final returns can allocate bookkeeping and retain
Engine's report-and-terminate policy on failure; no OOM-safe return claim is made.

Actual Engine-backed water/flow/forcing outputs and fixed-shape scratch retain
live allocation owners. A typed Reservation constructs the scratch owner, which
outlives its local Pool facade/reservation. Scratch never aliases a published
state. Regional advance borrows the immutable old weather for its first tick,
then owns complete candidates, removing an immediately redundant deep copy.
Original deep-copy checkpoint/state semantics, algorithms/versions, conservative
work charges, conservation, replay, grouped/resume and refusal behavior remain.

Fresh measurements on the SAME U17 host (Ryzen 7 7840HS, GNU16.2.1, Linux7.2.11,
glibc2.44, Release -O3, strict warnings, no unsafe FP flags/sanitizer):

| Path | 32x32 warm candidate p50 / p95 ms | 64x48 warm candidate p50 / p95 ms |
| --- | --- | --- |
| Retained U16 binary, freshly run before edits | .681664 / .936311 | 3.513330 / 3.842795 |
| Engine, original copy, fresh scratch | .633676 / .633776 | 3.318288 / 3.332234 |
| Engine, original copy, retained scratch | .611023 / .612406 | 3.249860 / 3.260220 |
| Engine, removed copy, retained scratch | .609341 / .613219 | 3.357661 / 3.367600 |

These are three warm samples, not tail certification or a demonstrated latency
improvement from copy removal. Final retained synchronous p95 is .616275/3.373451ms.
128x128 one/two-tick stress candidates take 132.864206/272.479269ms cold; warm
samples remain absent after the 50ms repetition stop. No observed 500ms update is
a universal bound. Same timed checksum14967335.747224323 and original per-update
work hold across all five paths (including final non-retained scratch).

At64x48, representative warm update checkouts fall16 ->8 ->4, with metadata-node
allocation ranges63..64 ->23..26 ->3..6. Instrumented baseline/weather cell/face
copy payload falls1,573,760 ->761,856 bytes; soil98,304 bytes remains. Retained
scratch capacity is345,864 bytes. Final retained warm backing calls are1,0,0,
so this is NOT allocation-free steady state. Complete final finite campaign
backing peak6,930,720 bytes, metadata requested peak11,400 bytes and selected
control requested peak3,912 bytes are process/service observations, not RSS,
complete application memory or per-feature caps. Cold standard baseline/soil/
report allocations and descriptor buffers remain outside allocation counters;
copy counters cover specified payloads, not total memory traffic. No locality gain
or isolated kernel/copy/metadata latency is claimed.

`advance_runtime` admits one tick, width<=64,height<=48,cells<=3072,
seconds/tick<=120,forcing scales<=2,at most8 substeps,1.5M work/update,4MiB logical
candidate budget and512KiB retained scratch. A shared allowance admits at most4
updates/6M work across regions; constructor requests clamp to those hard caps
and remaining fields are private. Actual successful/refused solver work is charged.
Exceptions publish nothing and require discarding the batch allowance. Two real
32x32/64x48 profile calls pass; three128x128 calls refuse before Engine checkout.
Final campaigns charge106,883,065 work within600M/30s, with40s process watchdog.
The extra profile calls happen outside timed samples; all raw timings remain explicit.
This is a synchronous CPU admission profile, not a certified gameplay cadence.

Observed checks: Release selected54/54 before and54/54 after optimization;
Engine-backed Debug/UBSan52/52; preserved default Debug/UBSan161/161. No failures
or skips. Final oversized-allowance checks also pass Release/UBSan; the rebuilt
timing binary is byte-identical, so no timing campaign is repeated. The initial source-level probe/access and Reservation sign-conversion
errors are corrected. Existing allocation-denial fixtures move unchanged into a
separate test translation unit to fix affected optimized mismatched-new-delete
warnings; no warning suppression is added. Full optimized all-target/SDK/platform/
ASan/leak verification is not claimed; U16's unrelated traversal warning remains
historical unaccepted evidence, not silently repaired.

Evidence: `builds/procedural-generation/logs/u17/commands.txt`, `exits.tsv`,
`recovery.json`, `dependencies.json`, all configure/build/CTest logs,
`before/`, `engine-initial/{no-reuse,reuse}/`, `engine-after/{no-reuse,reuse}/`,
`comparison.json`, both configuration/compile/link manifests, `verification.json`
`cap-review-binary-equivalence.json` and `preservation.json`. Raw host/cgroup snapshots report no new memory.events
or CPU-throttling deltas during these timing campaigns. Detailed current choices:
CLIMATE-009 and PROCGEN-053/054/056..058 in matching decision views.
Standalone receipt: [PROCGEN-U17.json](../../continuation/PROCGEN-U17.json),
**partial/checks_passed=false** for the complete runtime goal.

Next bounded action for coordinator selection: implement one weather producer and
bounded immutable endpoint slots over this admitted profile, with simulation-side
pin/sample/retire tokens and once-only contiguous interval adoption. Prove late,
full, stale and restore/reconfiguration cases and producer-side reclamation before
claiming parallel publication or the2ms foreground target. Immutable baseline and
remaining hot standard buffers, metadata-allocation failure and wider allocation
adapters also need explicit scope/evidence. No background producer, interpolation,
fixed cadence, endpoint backpressure, GUI/platform/forecast/fun acceptance or total
reader-memory bound is implemented here. Stop after this unit; the existing
publisher alone handles its checkpoint, with no automatic next milestone.
