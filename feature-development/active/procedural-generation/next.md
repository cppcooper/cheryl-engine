# Procedural-generation U13 climate handoff

PROCGEN-U13 (CLIMATE-P2), unit2 of procgen-climate-20261024-0615, completes from
clean954bf3b38933378d09d1519f3b1c005440a0fc64. Committed U12 ready/true receipt,
actual source, preserved context/units and nonempty122+12 passing logs were
inspected before dependent work. No unresolved prerequisite was extended. All
older receipts, WORLD sources, provenance archives and replay contracts remain.

Implemented private face_upwind_moisture model1/numerics1: owned U12 baseline,
nonnegative vapour/cloud/finite-water depths, reference-level air-temperature
proxy, fixed integer tick and latest owned known-face forcing interval. Closed
and periodic boundaries only, no uninitialized/open ghosts. Actual shared-face
outgoing fractions conserve water; separate incoming-fraction intensive advection
preserves constant temperature under convergence. First order is diffusive.
Finite evaporation depletes actual available water; replenishment is external
supply; capacity/condensation/cloud re-evaporation/rain transfers are bounded and
rain leaves as an explicit export. Local capacity uses ONE chosen lapse correction,
not a second windward cooling term. No pressure solver or latent heat is hidden.

Exact source/context/derived cells are compared before advance; same-size/same-ID
arrays alone do not suffice. Admission metadata is irrelevant to content. Full
state/source/model/current arrays/clock and future forcing are needed to continue,
not just a seed. No borrow escapes. Copies/older reports remain independent;
expected refusal publishes nothing and allocation/programming exceptions propagate.
The report is paired with its result state, not a detached provenance/delivery/save
credential. Baseline remains a frozen initial snapshot; no climate regeneration.

Useful proof: a9mm pulse over1000m ridge with A280K/.0065K/m lapse sees capacity
6.175mm and exports2.825mm per affected row. Literal full ridge8475m3 rain,
resolved central gap5650m3, flat0; mirrored wind AND source mirror the result.
Actual retained17x11 U8 key42/pitch3/-80..120, explicit10m/tick/2km/1000s,2m/s east,
eight ticks: rain124950m3, finite depletion80m3, replenishment3520m3, retained
274650m3 from396080m3 initial. Moisture centre x8.93111232. Explicit water masks/
reservoirs, not U8 sea threshold/U10 fill/U11 counts, determine supplies.

Executed GNU16.2.1/Linux Ninja Debug C++23/UBSan, wrapped one-job native work:
single accumulated135/135 CTest passes,0 failures/0 skips;124 cases/45176 assertions,
eleven consumers/38 demonstrations, including twelve new cases/608 assertions and
moisture consumer5/5. All122 prior names/112 case counts and33 preservation hashes
match. Independent literal pulse/phase/area/ledger oracles, convergence, row sign,
zero wind/unit axes, direct4 versus2+2 ticks, exact/one-short resources, runtime
work/CFL/numeric exhaustion, overflow, ownership and bad_alloc propagation pass.
No native failure/rerun/compiler/UBSan diagnostic or resource75/auth/allowance
refusal. Waiting native sessions18373/86810 completed normally without cancellation/
relaunch; build75337 is drained. Complete logs/commands/exits/source evidence are
under builds/procedural-generation/logs/u13; no unchanged Engine/map suite ran.

Limits128x128/16MiB/100M charged blocks,1..10 whole ticks,1..64 substeps/tick.
Choose smallest n satisfying actual outgoing AND incoming fractions<=1. No wind
clamping/skipped time. Work charges unsuccessful schedule trials too. Start
peak16384+216*C/work64+8*C. Advance peak16384+304*C+8*F and exact
64+9*C+2*F+n*(4*C+1)+ticks+steps*(7*C+2*D+1), canonical nonself faces D, supplied
face count F, steps=ticks*n. Full grid5261312 payload bytes. Payload excludes
preexisting inputs/held results/caller copies/allocator/runtime/RSS. Water ledger
residual tolerance1e-10*max(1,initial+external+rain)m3; no mass repair. Explicit
state/parameter ranges and remaining details in matching CLIMATE-003/005/006 and
PROCGEN-049 compact/detailed records.

No unresolved blocker. Confidence high in checked discrete contracts/ownership/
bounds, medium in private APIs and game model. Capacity is empirical column proxy,
not measured RH/dew point; no energy/latent-heat, hydrodynamics/U11 discharge or
coarse rain-shadow calibration claim. Other compilers/OSes, ASan/leaks, performance,
physical forecast, graphics/attractiveness/fun, public Engine/SDK, file/save formats
and prior WORLD visual/input/assets/ABI/durability acceptance remain unverified.
Float replay is model/numerics/build/platform/order scoped, not cross-platform
bit identity from integer keys.

Ready execution receipt:
[PROCGEN-U13.json](../../queue/procgen-climate-20261024-0615/PROCGEN-U13.json).
Next action: remain idle for coordinator source/log review and publisher transport.
Only finite controller may select U14 after clean drain/committed passing receipt.
If separately assigned, inspect actual U13 first and add a named bounded dynamic
pressure/wind/thermal-memory owner while retaining coherent face transport/source/
clock and one lapse interpretation. Do not bypass exact baseline matching, silently
replace geometry, infer reservoirs or treat old exported rain as a new impact.
No next job, background service, public extraction or sibling integration is launched.
