# Map-storage decisions

U1's CPU values and codec are implemented and checked. File publication and the
CLI remain proposals for later units. These are engineering choices, not user
preference declarations. Full contracts, alternatives, provenance, confidence and
reversal costs are in [agent-notes.md](agent-notes.md).

## MS-001 CPU boundary

U1 implements private CPU snapshots and a memory-stream codec for two logical
layers and one named spawn. Indexing discovers paths; graphics publication has
native ownership and partial effects. Keeping these responsibilities separate
allows independent native checks. The tradeoff is a small new boundary, with file
publication deferred. Confidence is high in isolation and medium in API shape;
reversal affects only private callers/tests today.
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

Use synchronous owned results and an owning private Error for anticipated codec
failures. Direct stream-buffer consumption preserves allocation/programming
exceptions and leaves stream flags/masks unchanged. Callers keep borrowed inputs
stable; failed parsing does not replace their snapshots. This unusual stream
contract needs explicit documentation and caller handling. Confidence is high in
checked ownership and medium in exception style. Reversal affects only private
callers/tests; historical styles are inconsistent.
[Details](agent-notes.md#ms-005-ownership-and-failure-reporting).

## MS-006 Failed-write preservation

For proposed U2, encode completely, exclusively stage beside the destination, check writes and
close, then replace once. Before that commit point, errors preserve the prior
file; afterward save returns success. Start with controlled regular local Linux
files and reject unsupported capabilities. This adds platform and fault-testing
work; power-loss durability and Windows replacement remain deferred. Confidence
is high in the preservation requirement and medium in the unexecuted platform
mechanism. Reversing the platform mechanism leaves the CPU codec intact.
[Details](agent-notes.md#ms-006-failed-write-preservation).

## MS-007 Integration and implementation placement

Keep U1 under `projects/tests/map-storage/` with a standalone CMake runner linking
only the codec and pinned gtest archives. Executed checks establish this CPU
boundary without Engine initialization. Tilemap adapters and active-level
adoption remain future responsibilities. Private placement may cost later
extraction, but keeps siblings independent. Confidence is high in checked
isolation and medium in final placement; relocation changes private targets
without public SDK migration.
[Details](agent-notes.md#ms-007-integration-and-implementation-placement).
