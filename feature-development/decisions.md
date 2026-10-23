# Map-storage decisions

U1's CPU values/codec and U2's private Linux file adapter are implemented.
U2F corrects native path meaning across ancestor symlinks.
U3 adds the private process CLI. Executed checks and remaining limits are in
[next.md](next.md). These are engineering choices, not user preference declarations.
Full contracts, alternatives, provenance, confidence and
reversal costs are in [agent-notes.md](agent-notes.md).

## MS-001 CPU boundary

U1 implements private CPU snapshots and a memory-stream codec for two logical
layers and one named spawn. Indexing discovers paths; graphics publication has
native ownership and partial effects. Keeping these responsibilities separate
allows independent native checks. U2 layers explicit-path I/O over the codec;
U3 exercises it through a private CLI. The tradeoff is a small private boundary.
Confidence is high in isolation and medium in API shape; reversal affects only private callers/tests
today.
[Details](agent-notes.md#ms-001-cpu-boundary).

## MS-002 Logical identity and coordinates

Store semantic qualified map/tile keys, map-local layer/spawn names, explicit
empties, and ordered dense layers with Y increasing downward. U1 checks equality,
copies and unbound tile keys without asset lookup. Repeated strings cost space
and future rendering needs bindings. Confidence remains medium, with little
personal-preference evidence. Reversal currently changes private fixtures/code;
changing identity meaning after persisted consumers exist would require a format
decision.
[Details](agent-notes.md#ms-002-logical-identity-and-coordinates).

## MS-003 Format and version

Use UTF-8 JSON identified as `cheryl.map-fixture`, integer version 1, with pinned
JSON 3.11.3 privately. A bounded SAX builder parses; validated fixed-schema output
is appended under the byte cap. Explicit version rejection and readable bytes aid
review, at a size/validation cost. Confidence is medium for this prototype and
low as a personal preference inference. Reversal is cheap before persisted
consumers; no universal save format is selected.
[Details](agent-notes.md#ms-003-format-and-version).

## MS-004 Validation and load publication

Acquire at most 8 MiB plus one detection byte; reject raw NUL, then enforce depth,
array/member/node and decoded-string budgets during SAX parsing. Decoded duplicate
names and early limits can precede later syntax errors. Validate dimensions and
semantics before returning an owned snapshot. Strict rejection costs extension
flexibility. Confidence is high in checked behavior and medium in budgets;
reversal changes private limits/tests and records.
[Details](agent-notes.md#ms-004-validation-and-load-publication).

## MS-005 Ownership and failure reporting

Use synchronous owned results and the same private Error. Path operations retain
an absolute path with components intact for native lookups and owning diagnostics,
including ancestor symlinks followed by `..`. Cleanup retains primary errors and
owned leftover details; allocation/programming failures reach CLI exit 1, while
owning storage errors use exit 2 and stderr. The tradeoff is a noncanonical
diagnostic spelling. Confidence is high in tested path identity, medium in
exception shape; reversal affects only the private adapter/tests.
[Details](agent-notes.md#ms-005-ownership-and-failure-reporting).

## MS-006 Failed-write preservation

Make paths absolute once without lexical normalization or final-link
canonicalization, honoring native ancestor-symlink/`..` meaning even for absent
targets. Encode fully, exclusively stage in the native parent, check writes and
release ownership before checked close, then rename once; cleanup preserves
primary errors. This retains controlled single-writer Linux scope and mode 0600,
at the cost of noncanonical spelling and limited guarantees. CLI failures after
save completion report that publication separately. Confidence is high
locally, medium elsewhere; reversal changes the private adapter/tests.
[Details](agent-notes.md#ms-006-failed-write-preservation).

## MS-007 Integration and implementation placement

Keep all units under `projects/tests/map-storage/`. The CLI links private
codec/adapter and the C++ runtime; tests add pinned gtest and an existing Python
standard-library process harness. Engine initialization and siblings remain
independent. Private placement and literal test duplication cost later extraction
but keep acceptance reviewable. Confidence is high in verified isolation and medium
in final placement; source/link and executable evidence belong in next.md.
[Details](agent-notes.md#ms-007-integration-and-implementation-placement).

## MS-008 Private CLI contract

Require exactly one explicit nonempty path for round-trip or verify. Round-trip
deliberately writes/replaces then loads; verify only reads. Compare the complete
owned snapshot to an independent literal fixture. Use exits 0/2/3/64/1 for success,
storage rejection, valid mismatch, usage and unexpected failure, with stderr
diagnostics retaining primary/cleanup details. Check output failures and report
completed saves accurately. Confidence is high in checked behavior, medium in
private output wording; reversal affects this consumer/harness.
[Details](agent-notes.md#ms-008-private-cli-contract).
