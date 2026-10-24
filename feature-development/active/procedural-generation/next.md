# Procedural-generation handoff after PROCGEN-U15

Completed fourth/final CLIMATE-P4 on feat/procedural-generation, from clean passing
U14 7c022e3f489b027b07070c9b5e59b4cf79828b41 after actual source/receipt/log review.
Private outer_tick_soil_weather schema/soil_model/numerics1 owns U14 weather plus
explicit per-cell soil water/capacity/infiltration/drying. Each completed tick
adopts rain into soil or overflow export; bounded drying returns soil water to
next-tick vapour. The full regional ledger counts rain internally once, retaining
finite depletion and reported replenishment. No changed U8/U11/U12/U13/U14 meaning.

Typed owned checkpoints retain exact source/frame/calendar/model/forcing/clock,
all prognostic weather arrays and surface reservoirs, plus soil parameters and
versions. Resume compares full baseline values, not just size/IDs. Unknown versions
or malformed soil refuse; raw malformed weather images are prevented by U14's
immutable validated owner, not a new parser/wire format. History admits1..4 strictly
ordered same-source snapshots under aggregate limits. Two independently described
region windows sample one authoritative owner with exact overlap/time. Sleeping
regions freeze old observations until explicit current sampling, without seeded
regeneration. Distributed halo exchange remains future.

Small application saves its last-applied interval marker/alert count beside the
checkpoint and adopts weather+soil+marker together. An old observation/report cannot
repeat its chosen wet alert on restore. This is an in-memory trusted-producer
example, without arbitrary external exactly-once/durability/authentication claims.
Its alert granularity follows the host's chosen adoption intervals; model trajectory
grouping does not promise batching-invariant application effects.
Capacity/fog/dryness/wind/fire signals are game proxies; application owns fuel,
ignition,roads,crops,NPC/story consequences. No ecology/fire/flood or event framework.

Executed GNU16.2.1/Linux/C++23/Ninja Debug/UBSan: one accumulated161/161 CTest
passes,0 failures/skips,148 cases/48017 assertions,thirteen consumers/49 demos.
New thirteen cases/1503 assertions and six demonstrations retain every147 old
name and135 cases/46514 assertions. Literal soil/drying/overflow/full-balance,
resumed/grouped trajectories, stage isolation, source/version/nonfinite refusal,
exact/one-short field/cell/storage/work/checkpoint/history/ticks/substeps,
allocation/numeric exhaustion, ownership, shared regions/sleep and host marker
checks pass. Staged review adds explicit duplicate-adoption rejection; affected
host/caller2/2 rechecks pass, with production unchanged. Current counts combine
those checks with the single accumulated run. No native failures, compiler/UBSan
diagnostic or resource75/
auth/allowance refusal. Two builds preceded the single suite: review tightened
rejected-header charging without changing successful totals. Original queued
sessions were kept/drained, never canceled/relaunched.

Caller retains dry pre-storm36 and wet after-storm46 images. At tick76 cloud sum
is below1e-6mm while soil sum is5.4302383797mm; bounded10-tick drying reduces it
to5.3222383798mm. Eight overlap cells agree exactly at tick76; sleeping observation
remains tick36 until refresh. Final host marker86 retains two alerts, with no
repeat from old impact or restored completed save. Full logs/commands/immediate
exits/counts/current-source witnesses are under builds/procedural-generation/logs/u15.
47/50 preservation hashes match; three necessary U14 access-wrapper files change,
with its layout/equations/order/charges and all prior cases preserved. Archives,
earlier receipts,WORLD sources and sibling work remain untouched.

Caps128x128/32MiB accounted payload/4M eight-byte field slots/100M work blocks,
1..10 ticks/64 inner substeps,max4 history snapshots. Maximum-grid state4917248
bytes,advance12328968 bytes,history4 output19668992 bytes. Payload includes copies,
scratch/reports/child weather; excludes preexisting owners/caller copies/allocator/
RSS. Capture/resume/history preflight exact copied payload and validate before
allocation. Runtime exhaustion refuses the entire candidate and preserves old
state/images/reports. Allocation/programming exceptions propagate. Detailed
formulas and units are in CLIMATE-007/008 and PROCGEN-051/052 matching records.

No unresolved blocker. Reversible engineering assumptions: linear capacity-limited
infiltration,constant-rate drying after outer ticks,typed images,small ordered
history,one-owner window sampling and observation-freeze sleep. Confidence high
in checked water/accounting/ownership/limits and medium in API/game usefulness.
Intentional v3 typed-setup evidence remains medium with recorded counterevidence;
agent-written APIs are compatibility, not personal taste. Same model/numerics/
build/platform/libm/order and full state are needed for replay; a seed is insufficient.
Other platforms/ASan/leaks/broad performance and interactive runtime profile,
physical/energy/soil/hydraulic calibration,visual attractiveness/fun,distributed
exchange,public Engine/SDK/formats/durable delivery and prior WORLD input/assets/
ABI/durability gaps stay unverified.

Ready worker execution receipt:
[PROCGEN-U15.json](../../queue/procgen-climate-20261024-0615/PROCGEN-U15.json).
Next action: remain idle for coordinator source/log review and publisher transport.
This final finite-batch milestone authorizes no fifth unit, new queue/service,
sibling integration or automatic public extraction. Later runtime profiling,
physical refinement, persistence integration or visual observation requires a
separately selected task. No further job is launched here.
