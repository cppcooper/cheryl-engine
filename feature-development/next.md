# Procedural-generation next action

PROCGEN-U4 is implemented and verified from accepted U3
`533fabba686653500dd6a247e46855c88bd79a0c` on `feat/procedural-generation`.
This checkpoint adds private finite 2D cave occupancy, integer fill, synchronous
Moore-8 smoothing, solid perimeter and retain-all/largest-floor component policies.
Outputs own bytes, replay, exact requested/effective controls and measurements.
Cardinal-4 connectivity distinguishes diagonal contacts, ties and empty outcomes.
U1–U3 maze implementation, replay and documented fixed-endpoint limits are retained.

Executed through the one-job shared wrapper in preserved Debug/GNU16.2.1/UBSan:
**10/10 cave CTest entries**, nine cases/844 assertions plus consumer2/2;
**33/33 maze entries**,32 cases/8635 assertions plus consumer8/8. All43 entries
passed after implementation, zero failures/skips/UBSan diagnostics. Final cave
version/algorithm diagnostics additionally pass the targeted invalid-input case
(36 assertions). Full logs, fixture calculations and immediate exits are in
`builds/procedural-generation/logs/u4/`. Exact contracts/provenance/checks are in
agent-notes PROCGEN-015–018, matching compact decisions in decisions.md.

Next concrete action: coordinator reviews this coherent committed U4 and its
saved logs. Publisher owns assigned-branch transport. Stop here; no automatic
next unit, sibling integration, presentation, save format or public extraction.
No user reply needed. No unresolved source/resource blockers or admission refusal.

Assumptions/confidence: chosen cellular controls are a useful first stylized
occupancy prototype (medium); bounded finite/simultaneous/cardinal contracts are
independently verified (high). Fill is a probability on interior cells, not exact
density. Largest mode can return empty with required=true/connected=false; it
never invents floor. Work preflight conservatively reserves all possible floors,
so realized solid-heavy caves may need less work than admission requires.
Other platforms/compiler, ASan, performance, public Engine and physical/subjective
quality remain unverified. Generation-storage guidance supplies replay/coordinate
concerns; it selects no persisted recipe, exact-base edit integration or sibling import.

Next useful proposal, subject to coordinator selection: bounded cave clearance/
distance measurements with independent tiny/empty fixtures. Tunnels, entrances,
masks/depth and advanced maze endpoints/route/branch/loop controls remain backlog.
Dungeons, geography, settlements and tectonic modification remain separate later
outputs with explicit ownership/provenance and dependent-layer revalidation.
