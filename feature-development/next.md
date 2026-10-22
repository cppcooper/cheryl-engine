# Next settings implementation unit

Implement only the app-private typed settings and file boundary described by
[SET-002, SET-006, and SET-007](agent-notes.md). Do not start this unit within the
completed design task. Read the implementation/testing instructions and inspect
Git status/diffs when a later task selects it.

The outcome is a CPU-usable `GameSettings` value, one shared validator, and a
`SettingsFile` adapter for missing/loaded/recovered reads and failure-preserving
replacement saves. Defaults are pan speed 100, health visible, and add-score
Space. The schema, key whitelist, ranges, 64 KiB limit, explicit resolved path,
diagnostics, and rename commit point are fixed in the matching decision records.
Keep JSON types out of headers and do not promote a general Engine settings API.

Expected scope is `projects/apps/tutorial-game/src/settings.h`, `settings.cpp`,
and `tests/settings.cpp`, plus directly necessary tutorial CMake registration.
Use the existing pinned JSON dependency privately for the implementation/tests;
Engine's private JSON include is not transitive. Do not change pins, retrieve
dependencies on the authority of this design record, introduce Workers, touch
sibling clones, or add UI/runtime callers in this unit. Preserve the existing
`tests-tutorial-game` gameplay cases. `extern/json` is currently unpopulated at
pin `b36f4c477c40356a0ae1204b567cca3c2a57d201`; a later task must supply that
dependency before compiling parser cases. This is a verification prerequisite,
not a request to fetch it during the design phase.

The checks for this unit are:

- Typed defaults and boundary values validate; nonfinite/out-of-range speed and
  unsupported/reserved key identities reject without changing the input value.
- A complete version-1 document round trips; absent files return defaults without
  creating files. Invalid/truncated/trailing JSON, duplicate/unknown/missing
  fields, wrong types/version, numeric overflow, bad tokens, and oversized input
  recover to the entire default snapshot with path/field or operation diagnostics.
  Rejected source bytes remain unchanged until an explicit save.
- With a valid existing file, force temporary-open, partial-write, flush, close,
  and rename failures through a small app-private file-operation test seam.
  Assert exact predecessor bytes, the reported operation, and ownership of any
  temporary cleanup. No failure path removes/truncates the target or touches an
  unowned temporary file. A real replacement and reload use a disposable path.
- Relative path resolution is fixed when the adapter is constructed; changing
  the process working directory later cannot redirect a save. Use isolated
  directories and restore any test process state.
- Inspect test registration: add cases to the existing `tests-tutorial-game`
  suite with only necessary private includes. Once execution is selected and
  authorized, use a compatible existing build, the workspace build lock, and
  the focused new settings selection plus existing `tutorial_game.*` cases.
  Do not infer native execution from static inspection or a skipped selection.

Stop after that value/file unit and its permitted checks. Report the exact
revision/results and any file replacement limitations, update both decision views
if contracts change, and commit only the unit. The subsequent input unit adds
`Snapshot::has_context`/`device_enabled` and stages the score context; the TGUI
editor follows.
No implementation, new tests, build, dependency retrieval, or native acceptance
has been performed by this design milestone.
