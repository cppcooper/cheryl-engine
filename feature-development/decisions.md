# Settings milestone decisions

These are proposed engineering choices for the first consumer, not implemented
features or direct user preferences. Reviewed source:
`3542f883391d4225329dab8d5d61a5987e69c8f0`, branch `feat/settings`.
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

The game owns one typed active value; the editor copies it into a draft and
validates a complete candidate before adoption. Cancel discards only the draft.
This gives clear lifetime and failure boundaries without a global registry or
widget-to-live setters. The tradeoff is application-specific code before reuse
is demonstrated. Design confidence is high; archived v3 provides medium support
for typed caller setup, not confirmed application-ownership preference.

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

Use a caller-selected absolute file path and version-1 JSON with all three values
required. Missing files use defaults; invalid or unreadable files use complete
defaults plus diagnostics and remain untouched until deliberate Apply. Reuse
existing JSON headers privately, with shared typed/file validation. Strict
rejection sacrifices tolerant forward compatibility for predictable recovery.
Confidence is medium-high as engineering policy; history establishes no JSON or
recovery-format preference.

## SET-007 — Commit the file before nonthrowing active adoption

Prepare valid serialized bytes and any replacement context, write/check/close
an exclusively created sibling temporary file, then rename once over the target.
Before that commit point, failure preserves the old file, active values, context,
and editable draft. Afterwards adopt with nonthrowing assignments/moves. Synchronous
Apply accepts brief owner latency and promises no power-loss or multi-process
transaction. Ordering confidence is high; filesystem failure behavior still needs
implementation and acceptance evidence.
