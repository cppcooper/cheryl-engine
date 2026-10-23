> **Parked by user direction, 2026-10-23.** See [parked.md](parked.md).
> U4 is an unverified interrupted draft; earlier U3 checks do not accept it.

# Settings milestone decisions

U1 implements app-private values/files in SET-002/006/007; U2 implements only
snapshot queries/metadata in SET-003/005. U3 implements the private score owner and
candidate lifecycle; U4 composes CPU Apply in SET-005/006/007. Game integration,
editor, modal gating and runtime scheduling remain proposed engineering policy,
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

`ScoreInput` owns active input and move-only Candidate/Prepared tokens. U4's private
`SettingsApply` retains one candidate, advancing generation for edits and retries.
Consumed polls and live leases establish readiness; preflight precedes saving,
then private unchecked noexcept adoption transfers ownership. U3's guarded adoption
remains available. Unchanged keys keep their lease. Borrowed owners require serialized
access and stable lifetimes. This trades composability for a small explicit boundary;
confidence is high in the engineering protocol, with modal admission still deferred.

## SET-006 — Store one complete, strictly validated JSON snapshot

Keep U1's complete version-1 schema, resolved path, shared validator, 64 KiB plus
one-byte acquisition and shallow SAX rejection. Loading rejects symlink leaves;
saving replaces the leaf link without changing its referent. U4 validates the entire
draft before staging; save validates and serializes again before temporary creation.
No setup/destructor autosaves. Strict rejection trades tolerant compatibility for
explicit recovery. Confidence is high in the Linux CPU/file boundary; these are
engineering choices, not inferred personal preferences.

## SET-007 — Commit the file before nonthrowing active adoption

U4 prepares the complete value and score token before U1's exclusive temporary
write/flush/close/rename. Rename commits; only allocation-free noexcept ownership,
value and enum completion follows. File failure propagates its diagnostic, releases
pending ownership and retains the draft for explicit retry. Cancellation/staleness
before saving writes nothing; repeated completion has no effects. Synchronous I/O
trades latency for a clear boundary. Confidence is high under serialized access;
concurrent teardown, power-loss durability and external effects remain excluded.
