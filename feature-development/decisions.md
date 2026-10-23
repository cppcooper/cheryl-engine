# Map-storage decisions

U1's CPU values/codec and U2's private Linux file adapter are implemented.
Executed checks and remaining limits are in [next.md](next.md). The CLI remains
proposed U3. These are engineering choices, not user preference declarations.
Full contracts, alternatives, provenance, confidence and
reversal costs are in [agent-notes.md](agent-notes.md).

## MS-001 CPU boundary

U1 implements private CPU snapshots and a memory-stream codec for two logical
layers and one named spawn. Indexing discovers paths; graphics publication has
native ownership and partial effects. Keeping these responsibilities separate
allows independent native checks. U2 layers explicit-path I/O over the codec;
the CLI is deferred. The tradeoff is a small private boundary. Confidence is high
in isolation and medium in API shape; reversal affects only private callers/tests
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

Use synchronous owned results and the same owning Error for anticipated codec
and file failures. U2 adds file phases and separate cleanup/leftover fields;
allocation/programming failures still propagate. A signaling native read buffer
distinguishes errors from EOF without changing the codec's stream contract.
Callers keep inputs stable and adopt only successful returns. This costs private
adapter machinery. Confidence is high in ownership, medium in exception shape;
reversal affects private callers/tests only.
[Details](agent-notes.md#ms-005-ownership-and-failure-reporting).

## MS-006 Failed-write preservation

U2 resolves paths once, encodes completely, exclusively stages with mkostemp,
checks unbuffered writes/close, and renames once. Linux close ownership is released
before the syscall; cleanup preserves the primary error and names leftovers.
Controlled regular-or-absent local files keep the scope testable, at the cost of
Linux-only support and new mode 0600. Confidence is high in the checked sequence,
medium beyond the local filesystem; reversing the adapter leaves the codec intact.
[Details](agent-notes.md#ms-006-failed-write-preservation).

## MS-007 Integration and implementation placement

Keep U1/U2 under `projects/tests/map-storage/` with the existing standalone CMake
runner linking only private codec/adapter and pinned gtest archives. Source/link
inspection and focused native checks establish this boundary without Engine
initialization. Tilemap adapters and active-level adoption remain future
responsibilities. Private placement may cost later
extraction, but keeps siblings independent. Confidence is high in checked
isolation and medium in final placement; relocation changes private targets
without public SDK migration.
[Details](agent-notes.md#ms-007-integration-and-implementation-placement).
