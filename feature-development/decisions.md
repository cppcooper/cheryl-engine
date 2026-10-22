# Map-storage decisions

These are proposed engineering choices for the first CLI fixture, not implemented
behavior or user preference declarations. Full contracts, alternatives, source
provenance, and acceptance limits are in [agent-notes.md](agent-notes.md).

## MS-001 CPU boundary

Use a private CPU snapshot, codec, and explicit-path file adapter for a CLI round
trip of two layers and one named spawn. Existing indexing discovers paths;
graphics loading has native ownership and partial publication. Separating these
responsibilities permits testing without a game or renderer. The tradeoff is a
small new boundary. Confidence is high in scope and medium in the proposed API.
[Details](agent-notes.md#ms-001-cpu-boundary).

## MS-002 Logical identity and coordinates

Store semantic qualified map/tile keys, map-local layer/spawn names, explicit
empty cells, and ordered dense layers in a zero-origin grid with Y increasing
downward. These values survive resource recreation without persisting cache
handles or atlas ordinals. Repeating strings costs space, and future rendering
needs a binding adapter. Confidence is medium; coordinate and naming choices are
reversible fixture conventions, with little evidence of personal preference.
[Details](agent-notes.md#ms-002-logical-identity-and-coordinates).

## MS-003 Format and version

Choose UTF-8 JSON identified as `cheryl.map-fixture`, integer version 1, using the
already declared JSON dependency privately. Explicit headers and rejection of
unknown versions make changes visible; readable bytes simplify review. This
costs size and parser validation work. No migration or universal save-game format
is selected. Confidence is medium for this prototype and low as a preference
inference; the dependency header is currently absent.
[Details](agent-notes.md#ms-003-format-and-version).

## MS-004 Validation and load publication

Return a new snapshot only after complete syntax, header, structural, identity,
reference, bounds, and resource-limit validation. Reject duplicate members,
unknown fields, numeric coercion and truncated input. This preserves existing
caller state and prevents silent data loss, at the cost of rejecting extensions
until versioning handles them. Confidence is high in strict publication and
medium in the initial size/count budgets; bounded parser behavior remains untested.
[Details](agent-notes.md#ms-004-validation-and-load-publication).

## MS-005 Ownership and failure reporting

Use synchronous calls with owned return values and structured owning exceptions
for anticipated storage failures. Borrow streams and inputs only during calls;
retain no game, registry, native resource, or global cache. Callers serialize
access to shared inputs and files. This makes failure adoption explicit but
requires a CLI exception boundary. Confidence is high in ownership isolation and
medium in exceptions; historical error styles are inconsistent.
[Details](agent-notes.md#ms-005-ownership-and-failure-reporting).

## MS-006 Failed-write preservation

Encode completely, exclusively stage beside the destination, check writes and
close, then replace once. Before that commit point, errors preserve the prior
file; afterward save returns success. Start with controlled regular local Linux
files and reject unsupported capabilities. This adds platform and fault-testing
work; power-loss durability and Windows replacement remain deferred. Confidence
is high in the preservation requirement and medium in the unexecuted platform
mechanism.
[Details](agent-notes.md#ms-006-failed-write-preservation).

## MS-007 Integration and implementation placement

Keep the prototype under `projects/tests/map-storage/` and demonstrate codec,
file publication, then CLI before choosing public exports. Tilemap adapts logical
values; future level lifecycle owns active-state adoption and spawning. Neither
sibling implementations nor a common save schema are dependencies. Private
duplication may require later extraction, but prevents premature shared
contracts. Confidence is high in independence and medium in final placement.
[Details](agent-notes.md#ms-007-integration-and-implementation-placement).
