# World integration decisions

WORLD-U1 implements a private combined CPU caller on exact candidate
aca49bdbfc5ac8d1f736d78f9f4ad8f4f815b56d. Executed checks pass 90 shared-Engine,
189 standalone storage and 71 procedural entries (350 total, no failures/skips),
including three new scenarios/six real child processes. Review-time byte
preflight adds a 3/3 affected recheck and six further child invocations. These are delegated
engineering choices, not inferred user API/game preferences. Detailed contracts
are in [agent-notes.md](agent-notes.md); commands/limits in [next.md](next.md).

Complete source decisions remain archived without changes:

- [Tilemap](features/tilemap/decisions.md), [contracts](features/tilemap/agent-notes.md), [evidence](features/tilemap/next.md), [provenance](features/tilemap/provenance.json).
- [Map storage](features/map-storage/decisions.md), [contracts](features/map-storage/agent-notes.md), [evidence](features/map-storage/next.md), [provenance](features/map-storage/provenance.json).
- [Procedural generation](features/procedural-generation/decisions.md), [contracts](features/procedural-generation/agent-notes.md), [evidence](features/procedural-generation/next.md), [provenance](features/procedural-generation/provenance.json).

## WINT-001 — Preserve exact history and records

Adopt only the specified clean descendant after exact tilemap-head and all three
source-ancestor gates. Keep both merge histories, nine archived records and their
provenance unchanged; extend root integration records. This preserves each
feature's evidence without confusing source merging with behavior acceptance.
The cost is explicit historical entry points. Confidence is high from static
ancestry/content checks; changing inputs requires another candidate and affected
verification. Coordinator review alone selects worker1 promotion.

## WINT-002 — Exercise one real shared Engine

Link the private combined caller to normal Cheryl::Engine. Reuse the existing
storage consumer within that same build; compile actual private cave sources in
a generator-only archive. Header-first, symbol/import/loader and producer checks
prove the selected source-tree boundary without implementation substitution.
This adds private CMake wiring and full Engine dependencies. Confidence is high
for GNU/Linux Debug; installed SDK, static and wider ABI remain unverified.
Reversal affects private build/caller policy.

## WINT-003 — Keep identities and metadata explicit

Own a reversible qualified-key/nonzero-ID table, reserving zero for null. Retain
map ID, extent, ordered layer names and ordered spawns separately from numeric
Map. Preflight conversions and adopt only complete owned candidates; compare
against independent full literal snapshots/numeric arrays in fresh processes.
Extra copies/scans trade simplicity for bounded isolation. Confidence is high in
ownership and tested identity separation, low as user taste evidence; reversal
is private and leaves strict v1 unchanged.

## WINT-004 — Separate runtime edits and file publication

Diff complete final snapshots against the exact untouched base, preserving storage
metadata and 65,536-cell/1,024-change/8-MiB caps. Reject unknown mappings, unsupported
metadata, mismatches and excess bounds. Engine terrain batches coalesce validated
last-wins requests; storage patches retain unique addresses. Checked save remains
a separate rename boundary, preserving predecessors on precommit failure. Full
witness/candidate costs buy correctness, not compact retention or durability.
Confidence is high locally; reversal changes private policy, not formats.

## WINT-005 — Save procedural meaning

Use the actual seeded 5x4 cave producer with one declared smoothing step and an
independently known full occupancy oracle. Map solid/floor to labels 11/23 and
persistent keys, while cardinal selection derives separate visual IDs1001–1016.
A coalesced terrain edit produces one final semantic patch; fresh reload rebuilds
all meaning and visuals. Simple diagnostic generation trades visual richness for
auditable integration. Confidence is high in CPU semantics; game meanings/control
choices remain private and reversible.

## WINT-006 — Bound and qualify evidence

Preserve the incremental shared Debug build; use separate clone-local standalone
storage/procedural outputs for their unchanged private suites. Run each accumulated
selection once, matching discovery and actual execution, through the one-job
resource wrapper. Save full logs, immediate exits and process evidence; record
resolved initial compile errors. This costs separate configurations but preserves
existing fault/replay coverage. Confidence is high in executed CPU results;
graphics, fun, leaks and broad platform acceptance remain open.

## Active procedural-generation continuation

Ongoing U8+ records use a separate namespace after the reviewed WORLD-U1 promotion:
[decisions](active/procedural-generation/decisions.md),
[contracts and evidence](active/procedural-generation/agent-notes.md),
[current handoff](active/procedural-generation/next.md),
[U8 execution receipt](active/procedural-generation/PROCGEN-U8.json).
Historical feature archives and WINT-001–006 records above remain preserved.
Current queued execution evidence:
[U9 receipt](queue/feature-round-20261024-0500/PROCGEN-U9.json).
[U10 receipt](queue/feature-round-20261024-0500/PROCGEN-U10.json) retains the initial
test-only counter failure and successful affected recheck.
[U11 receipt](queue/feature-round-20261024-0500/PROCGEN-U11.json) completes this
worker's finite round with actual-forest accumulation and continuous channels.

PROCGEN-U12 continues private climate work with explicit regional physical inputs
and derived solar/seasonal targets. Matching [active decisions](active/procedural-generation/decisions.md),
[contracts and evidence](active/procedural-generation/agent-notes.md),
[current handoff](active/procedural-generation/next.md) and
[U12 execution receipt](queue/procgen-climate-20261024-0615/PROCGEN-U12.json)
preserve all earlier archives and receipts; no WORLD/public integration is implied.
