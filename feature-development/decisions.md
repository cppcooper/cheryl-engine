# Map-storage decisions

U1's CPU values/codec and U2's private Linux file adapter are implemented.
U2F corrects native path meaning across ancestor symlinks.
U3 adds the private process CLI. U4 moves the same implementation into Engine-owned
public CPU source and adds a source-tree consumer. U5 selects normal shared-Engine
consumption of that same boundary. U6 adds a private exact-base cell-edit proof;
U7 exposes bounded public validation; U8 promotes bounded patches into Engine.
U9 adds bounded cell differences. None changes v1 wire behavior. Executed checks
and limits are in
[next.md](next.md). These are engineering choices, not user preference declarations.
Full contracts, alternatives, provenance, confidence and
reversal costs are in [agent-notes.md](agent-notes.md).

## MS-014 Bounded cell differences

Add CellPatches::diff for validated snapshots with identical metadata and ordered
spawns. Count changes before reserving at most 1,024 edits, then emit deterministic
layer-major/row-major set/clear values with a full original-base witness. Reuse
public validation and patch limits; append InvalidTarget/UnsupportedChange without
renumbering existing codes. This avoids metadata migration and candidate copies,
at the cost of two scans and full witness storage. Confidence is high in semantics,
medium in diagnostic choices; reversal affects public source/ABI callers.
[Details](agent-notes.md#ms-014--bounded-cell-differences-u9).

## MS-013 Engine-owned exact-base cell patches

Promote the verified proof to CE::Maps::CellPatches with one public header/source,
exported capture/apply, witness operations and owning Error RTTI. Tests and private
process consumers use that implementation; the independent consumer links the
real shared Engine. This removes duplicate authority while preserving bounds,
exact matching and validation. Full witnesses still cost up to 8 MiB and are not
compact or cross-version identities. Confidence is high locally, medium in naming;
reversal affects public source/ABI callers, with SDK/platform acceptance deferred.
[Details](agent-notes.md#ms-013--engine-owned-exact-base-cell-patches-u8).

## MS-012 Reusable encodability validation

Expose CE::Maps::validate as a synchronous semantic and v1 encodability check,
counting the same formatter fragments as encode without storing output bytes.
This shares policy and exact byte limits while avoiding serialization buffers.
Owned errors identify `validate`/`<snapshot>`; existing parse/encode context stays
unchanged. Patch candidates now validate once after copying; semantic errors use
candidate order. Confidence is high in the shared mechanism; reversal affects the
new public symbol and branch-local callers, with other-platform ABI still unverified.
[Details](agent-notes.md#ms-012--reusable-encodability-validation-u7).

## MS-011 Private exact-base cell patches

Capture full canonical base bytes and compare before applying cell-only edits
into an independent Snapshot. Cap bases at 65,536 cells, batches at 1,024 unique
addresses and encoded values at 8 MiB. U7 validates one candidate after allocation
instead of per-key probes. This prevents replay on changed cells, order or spawns.
Full witnesses cost memory, without compact-save or recipe claims. Confidence is
high in semantics, medium in caps; reversal now affects public source/ABI
callers (MS-013).
[Details](agent-notes.md#ms-011--private-exact-base-cell-patches-u6).

## MS-001 CPU boundary

U4 exposes the proven owning Snapshot and synchronous codec/file operations in
Engine source as CE::Maps. Directory indexing still discovers paths; storage
reconstructs independent CPU values without assets, Runtime, Workers or rendering.
Existing CLI/tests now consume those same files, preserving format and behavior.
The tradeoff is a narrow public source boundary before full integration evidence.
Confidence is high in isolation, medium in naming; reversal affects branch-local
headers and consumers.
[Details](agent-notes.md#ms-001-cpu-boundary).

## MS-002 Logical identity and coordinates

Store semantic qualified map/tile keys, map-local layer/spawn names, explicit
empties, and ordered dense layers with Y increasing downward. U1 checks equality,
copies and unbound tile keys without asset lookup. Repeated strings cost space
and future rendering needs bindings. Confidence remains medium, with little
personal-preference evidence. Reversal changes the public CPU representation and
branch-local callers; changing identity meaning after persisted consumers exist would require a format
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
reversal changes public limits, tests and callers.
[Details](agent-notes.md#ms-004-validation-and-load-publication).

## MS-005 Ownership and failure reporting

Keep synchronous owned results and the unchanged Error contract, now CE::Maps::Error
with Engine visibility. Paths retain native components, including ancestor symlinks
followed by `..`; cleanup retains primary and owned leftover details. Allocation
and programming failures propagate, while the private CLI owns process exits.
Noncanonical spelling and C++ exception ABI are tradeoffs. Confidence is high in
tested ownership/path identity and Linux/GNU cross-DSO copying, medium in
exception shape; other-toolchain/installed ABI remains unverified.
[Details](agent-notes.md#ms-005-ownership-and-failure-reporting).

## MS-006 Failed-write preservation

Make paths absolute once without lexical normalization or final-link
canonicalization, honoring native ancestor-symlink/`..` meaning even for absent
targets. Encode fully, exclusively stage in the native parent, check writes and
release ownership before checked close, then rename once; cleanup preserves
primary errors. This retains single-writer Linux scope and mode 0600,
at the cost of noncanonical spelling. CLI failures after
save completion report that publication separately. Confidence is high
locally, medium elsewhere; reversal changes the adapter and public contract.
[Details](agent-notes.md#ms-006-failed-write-preservation).

## MS-007 Integration and implementation placement

Move the single implementation into Engine include/src under core/resources/map-storage,
using CE::Maps and CE_API on noninline boundaries. JSON and native fault operations
stay private. Existing source discovery includes the files; a small Engine-owned
source-tree target builds them independently for CLI/tests/consumer. This avoids
duplicate authorities; U5 verifies the shared source producer, with static Engine/SDK
evidence deferred. Confidence
is high in scoped isolation, medium in names; reversal changes source-tree callers,
not wire bytes.
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

## MS-009 Source-tree CPU usage requirements and tests-off consumer

Declare C++23, public Engine includes and CE_ENGINE_STATIC on Cheryl::MapStorageCPU;
keep pinned JSON private. MAP_STORAGE_BUILD_TESTS defaults on; off bypasses both
gtest and Python. An independent tests-off consumer links only that target, with
header-first probes, owned encode/parse checks, fresh literal file verification
and failed-save predecessor preservation. The extra build proves a dependency
boundary masked by tests-on. Confidence is high in this bounded proof; U5 separately
checks shared Engine. Installed/static Engine acceptance remains unverified.
[Details](agent-notes.md#ms-009-source-tree-cpu-usage-requirements-and-tests-off-consumer).

## MS-010 Normal shared-Engine consumer

Add an opt-in shared-Engine mode to the existing garden consumer, retaining CPU
proof mode. Reuse the normal scoped Engine bootstrap, link only Cheryl::Engine
and inherit its visibility/dependency requirements. Producer-aware header probes,
real cross-DSO values/errors, fresh processes and symbol/link inspection address
a gap the alternate archive could not prove. Full Engine dependencies cost build
time. Confidence is high in Linux/GNU execution; leak checking, installed/static Engine
and other-toolchain acceptance remain separate.
[Details](agent-notes.md#ms-010-normal-shared-engine-consumer).
