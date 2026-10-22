# Settings milestone decisions

SETTINGS-U1 implements app-private value/file portions of SET-002/006/007.
Editor, input, and runtime Apply choices remain proposed engineering policy,
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

Runtime alone polls input; simulation routes immutable records and owns TGUI.
The editor requests Exclusive keyboard focus and suppresses every gameplay
command through opening, editing, apply, closing, and a later neutral poll with
the published keyboard gate enabled after a platform-owned release watermark.
This covers collected edges, pointer input, and held-key reactivation that focus
alone cannot prevent. Gameplay pauses while UI/rendering continue. Confidence is
high in the boundary; sequential/concurrent scheduling acceptance remains outstanding.

## SET-004 — Bind portable physical presses in a dedicated epoch

Rebinding uses a separate Exclusive focus target and accepts one fresh physical
keyboard press matching its device, target, and epoch. Permit Space, letters,
and digits except existing A/D/J/L commands; reject modifiers, reserved keys,
repeat, and text. Persist canonical names rather than runtime IDs. This narrows
the product but avoids accidental widget activation and stale input capture.
Confidence is high in identity handling, medium in the selected key subset.

## SET-005 — Observe a staged score context before adopting it

Retain the old score context while attaching a candidate, then wait across
updates for a consumed published snapshot containing it. Propose the additive
`Snapshot::has_context` query because inactive actions cannot prove installation;
also publish `device_enabled` for reliable rearming past queued focused polls.
Save before moving the candidate into active ownership; keep gameplay gated
through rearming. This adds a small input contract instead of frame-count guesses
or mapper mutation. Confidence is medium until publication/removal/gate cases
establish sufficiency.

## SET-006 — Store one complete, strictly validated JSON snapshot

U1 fixes the caller's path and acquires at most 64 KiB plus one byte. A shallow
SAX reader rejects raw NUL, duplicate/unknown/missing fields, wrong types/versions,
and invalid values without a nested DOM. Absence selects defaults; invalid,
unreadable, or symlink files recover wholly with diagnostics. Strict rejection
costs tolerant compatibility; schema changes require versioning. Focused Linux
checks support high confidence in this bounded policy. History establishes no
JSON or recovery-format preference.

## SET-007 — Commit the file before nonthrowing active adoption

U1 validates and serializes before exclusive sibling `.pending` creation, then
writes, flushes, closes, and renames once. Collision or pre-rename failure preserves
old bytes; cleanup owns only this attempt. No fallible work follows rename.
Later Apply will adopt state separately. Synchronous I/O and visible stale-file
collisions trade convenience for explicit failure. Focused Linux checks support
high confidence; durability, other platforms, and multi-process guarantees remain
outside scope.
