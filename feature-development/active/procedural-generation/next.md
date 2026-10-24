# Procedural-generation handoff after blocked PROCGEN-U16

U16 starts from exact clean aacfa6a55b2944068882b6c8ca4d39bc681a6ec0 on
feat/procedural-generation. Actual committed U15 ready/true source/receipt/logs and
the coordinator climate-runtime-predecessors review were checked. This is one
standalone follow-up; older plans, receipts, archives and WORLD sources remain
unchanged. No U17, new queue, integration or background service starts.

Implemented/executed subset: an opt-in Release baseline caller uses actual U8
terrain -> physical baseline -> U14 weather -> U15 regional owner -> bounded
advance -> complete move adoption -> two overlapping regional reads with tick/
sample age -> separately measured old/report/final-owner destruction. Production
algorithms, replay versions, owned buffers and all old test names remain unchanged.
No simplified substitute solver, optimizer, new allocator domain or worker thread.

Observed GNU16.2.1/Linux Release baseline, strict warnings, no sanitizer/fast-math:

| Case | Ticks / seconds per tick / substeps / scales | Cold candidate ms | Warm candidate p50 / p95 / max ms | Warm synchronous p95 ms |
| --- | --- | --- | --- | --- |
|32x32|1 /60 /4 /2|1.057329|.766715 /.829412 /.829412|.834662|
|64x48|1 /120 /8 /2|5.195341|4.027616 /4.462149 /4.462149|4.466067|
|128x128 stress|1 /1000 /64 /4|165.894619|not executed after50ms stop|not measured|
|128x128 expensive admitted|2 /1000 /64 /4|277.497040|not executed after50ms stop|not measured|

Cold is one fresh-owner update, not an OS-cache reset. Small/representative each
have cold1,warmup1,warm3; stress/expensive have cold1 only. Twelve accepted evolving
updates plus one genuine63-versus-required64 substep refusal execute; refusal
.720178ms/197245 work preserves the prior state. Total update work105780281 within
predeclared600M/30s bound, with40s runner watchdog. Full campaign499.622994ms is
not one500ms update. Raw samples/setup/read/adoption/destruction and absent warm
percentiles remain explicit. No observed update reaches500ms, but representative
synchronous p95 misses2ms and stress misses50ms. No interactive profile or total
regional cadence/admission is certified.

BLOCKER: the one wrapped pinned submodule retrieval exits1 with
`Could not resolve host: github.com` for ctti/backward-cpp/spdlog. Git's own
scheduled retry is retained in the complete log; no second invocation or sibling/
protected-cache/system substitution follows. Actual Engine services were inspected:
Manager/managed blocks for bytes and typed objects Pool/Reservation/Factory/
Constructor for tracked class lifetimes. Existing allocator-traits/rebind/final/
count/lifetime gaps remain uncorrected/unverified. No allocator/Engine source
changes or actual allocation/copy/bookkeeping/reuse/high-water measurements occur.
All production legacy buffers retain their original standard allocation; fixed
benchmark setup/sample arrays add no heap adapter. Payload formulas are not RSS
or measured allocation counts. This is a prerequisite-boundary stop, not acceptance.

Separate limitation: the optimized all-target predecessor build exits1 on existing
traversal-tests.cpp maybe-uninitialized diagnostics under Werror. Warnings were not
suppressed and unrelated tests were not rewritten. Focused Release baseline builds/
runs0. Original failure logs remain. Existing Debug/UBSan cache is preserved;
one accumulated161/161 CTest run passes,0 failures/skips:148 cases/48017 assertions,
thirteen consumers/49 demonstrations. Conservation/thermal/forcing/grouped/resume/
regions/snapshot/host/refusal contracts re-execute. Comment/context-label correction
rebuild yields the identical timed binary, without another timing campaign.

Evidence: builds/procedural-generation/logs/u16/{commands.txt,exits.tsv,
dependencies.log,build-release.log,build-baseline.log,baseline-raw.jsonl,
context-before.json,context-after.json,context-deltas.json,timing-summary.json,
build-debug.log,discovery-debug.json,ctest-debug.log,counts.json,
binary-equivalence.json,current-source.json,preservation.json}, plus full build/
flag manifests. Detailed contracts/evidence: CLIMATE-009 and PROCGEN-053..055 in
matching decisions.md/agent-notes.md. Standalone execution receipt:
[PROCGEN-U16.json](../../continuation/PROCGEN-U16.json), blocked/checks_passed=false.
A successful baseline process or correctness suite cannot certify U16 runtime.

Assumptions are reversible engineering choices: flat x-fast baseline, sparse
finite samples, synchronous measurement, two small owned observations, cold meaning
fresh owner. Confidence high in raw samples/preservation, medium in baseline
usefulness, low in tail/platform inference. Existing typed APIs establish
compatibility, not personal taste. Broader platforms/ASan/leaks/durability/GUI/fun/
physics/forecast/public Engine gaps remain; no cache locality gains are claimed.

Next concrete action AFTER coordinator/environment resolution: make exact pinned
Engine dependencies available with working DNS; build the actual narrow CPU service
link closure; correct/prove primary allocator_traits/container routing and typed
object lifetime; instrument actual allocations/copies/bookkeeping and migrate the
measured owner/scratch path. Optimize from that evidence and repeat the SAME bounded
workloads, then implement/test interactive grid/tick/substep/forcing/total-work
admission and fixed cadence. Resolve optimized predecessor warning diagnostics
without weakening warnings. Remain idle now for coordinator review/publisher
transport; no unchanged retrieval retry or next job is launched.
