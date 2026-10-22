# Next map-storage unit

The next proposed unit is **U1: private CPU snapshot and JSON codec**. Implement
it only when a subsequent assignment activates implementation. This preparation
ends with design records; no implementation or executable check has run.

Use [MS-001 through MS-005](agent-notes.md#ms-001-cpu-boundary), with private
placement from [MS-007](agent-notes.md#ms-007-integration-and-implementation-placement).
Create the owning snapshot/layer/spawn types, value equality, bounded `parse` and
`encode`, and focused codec checks under `projects/tests/map-storage/`. Keep JSON
types private. The literal fixture is the 3x2 map with ordered `ground` and
`overlay` layers and spawn `entry` at ground cell (1,1) in
[the proposed caller](agent-notes.md#ms-001-cpu-boundary).

U1 excludes path-based load/save, staging/replacement, CLI publication, public SDK
exports, asset resolution, tilemap integration, and game lifecycle. Its result
is a validated in-memory round trip, not completion of the map-storage milestone.

Before editing, inspect status/diffs and applicable source/test instructions.
Check the existing dependency and build setup without altering active work. The
declared JSON header is absent at this checkpoint; no dependency was downloaded.
Future dependency retrieval and compilation must stay within that implementation
assignment's authority, preserve the pin, and follow the shared build lock/resource
rules. Plan a private CPU-only target using standard C++ and the declared JSON
headers, with no Engine/native module link. Do not run root configuration merely
to inspect it; compiler probes count as build operations.

U1's required checks, once permitted:

1. Encode/parse the asymmetric literal fixture through memory streams and compare
   every field, layer order, all twelve cells/empties, and spawn coordinates.
   Load two independent values; changing/destroying one must leave the other intact.
2. Verify deterministic encoding, property-order-independent reading, and exact
   rejection categories for malformed/truncated/trailing input, another format,
   integer versions 0/2, and missing/wrongly typed headers.
3. Exercise duplicate members at different object depths, missing/unknown fields,
   wrong integer types including booleans and `1.0`, invalid/duplicate IDs,
   mismatched cell counts, bad spawn references/coordinates, and syntactically
   valid unbound tile IDs that must round-trip without assets.
4. Exercise byte/depth/key/count limits at and across boundaries, checked dimension
   products, and encoded-size rejection. Verify a failure leaves a previously
   owned snapshot unchanged. Establish bounded parsing and duplicate detection
   using the declared JSON pin rather than assuming the default DOM reader does it.
5. Inspect target edges and execute only focused codec checks. Record the actual
   revision/configuration/results and limitations. Mocks, source inspection and
   successful compilation alone do not establish filesystem or CLI acceptance.

If this unit changes a contract, update the corresponding stable IDs in both
[agent-notes.md](agent-notes.md) and [decisions.md](decisions.md). Commit only its
coherent task changes locally using the established command identity. Hand off
to U2 file publication only after U1 review; do not automatically proceed through
the remaining units or start a recurring worker loop.
