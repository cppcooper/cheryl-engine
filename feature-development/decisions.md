# Settings milestone decisions

U1 implements app-private values/files in SET-002/006/007; U2 implements only
snapshot queries/metadata in SET-003/005. Editor, score-context integration, modal
gating, and runtime Apply remain proposed engineering policy,
not direct user preferences. Design checkpoint:
`157c75736bf5d325c8ea5174430b5dbbd75a2ae4`, branch `feat/settings`.
[Agent notes](agent-notes.md) carry the matching IDs, contracts, alternatives,
source/history provenance, counterevidence, and acceptance limits.

## SET-001 — Use the existing tutorial TGUI consumer

Expose camera pan speed (default 100 world units/second), health visibility
(default true), and the existing add-score action (default Space) in the tutorial's
single TGUI session. Existing visible effects keep the milestone observable
without another application or toolkit lifetime. This limits the first consumer
to game settings and keyboard rebinding; engine options wait. Confidence is high
in scope fit, with native interaction still unverified.

## SET-002 — Keep typed active and draft values in the game

U1 provides app-private `GameSettings` and one validator used by loading and saving.
The later editor will own a draft and adopt only a valid complete value. This
keeps ownership explicit without a global registry; it costs application-specific
code before reuse is demonstrated. Confidence is high in this boundary, and
reversal is cheap before UI callers exist. Archived v3 supports typed caller
setup moderately, not application ownership as a personal preference.

## SET-003 — Combine Exclusive focus with a modal gameplay gate

U2 copies publication-time device admission into immutable snapshots; unlisted
IDs default enabled without implying hardware existence. Future modal editing still needs Exclusive
focus, whole-batch gameplay suppression, and neutral rearming from a new consumed
poll beyond its release watermark. Empty batches return a baseline and cannot
rearm. This preserves queued policy history at the cost of copying a gate set per
mapper poll. Confidence is high in the query boundary; editor isolation and
scheduling remain unverified.

## SET-004 — Bind portable physical presses in a dedicated epoch

Rebinding uses a separate Exclusive focus target and accepts one fresh physical
keyboard press matching its device, target, and epoch. Permit Space, letters,
and digits except existing A/D/J/L commands; reject modifiers, reserved keys,
repeat, and text. Persist canonical names rather than runtime IDs. This narrows
the product but avoids accidental widget activation and stale input capture.
Confidence is high in identity handling, medium in the selected key subset.

## SET-005 — Observe a staged score context before adopting it

U2 adds `Snapshot::has_context` for sample membership, including final departing
release; zero, pending and retired identities are absent. It neither retains nor
validates a lease. Future Apply must retain a valid candidate, matching editor
generation and consumed publication evidence before replacing the old score
context. This avoids inactive-action probes and frame guesses without changing
mapper ownership. Tradeoff: membership is historical, so ownership checks stay
with callers. Confidence is high in the snapshot contract; the editor protocol
remains proposed.

## SET-006 — Store one complete, strictly validated JSON snapshot

U1 resolves paths once and acquires at most 64 KiB plus one detection byte.
Shallow SAX rejects raw NUL, duplicate/unknown/missing fields, wrong types/versions
and invalid values. Absent files use defaults; invalid/unreadable files recover
wholly with diagnostics. Loading rejects symlink leaves; saving replaces the leaf
link without changing its referent. Strict rejection sacrifices tolerant
compatibility. Confidence is high after focused Linux checks; this engineering
policy is not inferred user taste.

## SET-007 — Commit the file before nonthrowing active adoption

U1 validates and serializes before exclusive sibling `.pending` creation, then
writes, flushes, closes, and renames once. Collision or pre-rename failure preserves
old bytes; cleanup owns only this attempt. Rename replaces leaf links, preserving
referents. No fallible work follows rename.
Later Apply will adopt state separately. Synchronous I/O and visible stale-file
collisions trade convenience for explicit failure. Focused Linux checks support
high confidence; durability, other platforms, and multi-process guarantees remain
outside scope.
