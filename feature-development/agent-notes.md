# First map-storage milestone

U1 implements private owning CPU snapshots and a bounded memory-stream JSON codec.
Accepted U1 checkpoint `847d2a9c8fa8c190d2b55e0e59b596a9cbc4ff54` passed 86 focused
cases. U2 adds private checked Linux path loading and safe replacement. Accepted
U2F checkpoint `ef1d0ac741cdbe888d06d3f6216f8d1d2104f077` preserves native path
meaning through ancestor symlinks/parent components and passed 122 cases.
U3 adds the private CPU CLI; actual execution evidence is in [next.md](next.md). This
is not acceptance of the full persistence milestone.
The reader summary is [decisions.md](decisions.md);
actual commands/results and the next unit are in [next.md](next.md).

The inspected baseline is `3542f883391d4225329dab8d5d61a5987e69c8f0` on
`feat/map-storage`, matching the workspace's recorded source snapshot. Design
checkpoint `20cd3f74dd94932d18745b1e0d90eb68538378f7` contains the initial records.
U1 started from that clean checkpoint; U2 started clean at the accepted U1 commit.
U2F started clean at reviewed U2 checkpoint
`4751b271b1c7c75ae8951d71cddd1e8e303930f6` and changes only path resolution,
its private regression tests/comments and these internal records.
U3 started clean at accepted U2F; the codec/adapter and format/resource limits
remain intact. Only the private consumer, its fixture/diagnostics/tests, direct
standalone target/registration and internal records belong to this assignment.
Changes are confined to the private
`projects/tests/map-storage/` implementation/tests/build entry and these records.

The [workspace instructions](../../../AGENTS.md),
[autonomy policy](../../../coordination/autonomy-policy.md), and
[map-storage brief](../../../coordination/feature-briefs.md#map-storage) set the
scope. Repository workflow, investigation, architecture, documentation, testing,
and Git instructions were read, including `docs/AGENTS.md`. The current assignment
authorizes U3 implementation, focused native verification and its local commit,
and stops for coordinator acceptance. Protected source workspaces and sibling implementations are
not prerequisites or execution sites.

Implemented contracts and actually executed checks are distinguished below. These
are engineering choices under delegated authority; confidence in a requirement
or observed contract does not establish the user's personal taste.

## MS-001 CPU boundary

U1 uses a private finite-map snapshot and CPU codec. It reconstructs two logical
tile layers and a named spawn from memory, then compares values. U2 adds an
explicit-path adapter in `file-storage.h`, with no Engine dependency. The CLI
now exercises these same operations in a fresh process. No unit creates an engine Context, game,
provider, window,
image cache, or asset loader. Discovery may locate content in a later application;
it does not own persistence, select an output path, or make a write transactional.

The private namespace is `MapFixture`, aliased as `MF` below. `Snapshot` owns
strings, dimensions, ordered layers, optional tile keys, and spawn records; all
constituent types support value equality. This caller fragment uses the
implemented API from `codec.h` plus `<sstream>`; the corresponding complete
fixture and comparisons run in `codec-tests.cpp`:

```cpp
namespace MF = MapFixture;

bool memory_round_trip() {
    const MF::Snapshot original{
        .id = "fixture:two-layers", .width = 3, .height = 2,
        .layers = {
            {.id = "ground", .cells = {
                "fixture:grass", "fixture:stone", "fixture:grass",
                "fixture:grass", "fixture:grass", "fixture:stone"}},
            {.id = "overlay", .cells = {
                std::nullopt, "fixture:wall", std::nullopt,
                std::nullopt, std::nullopt, "fixture:wall"}}
        },
        .spawns = {{.name = "entry", .layer = "ground", .x = 1, .y = 1}}
    };
    const auto bytes = MF::encode(original);
    std::istringstream input(bytes);
    const auto restored = MF::parse(input, "memory:two-layers");
    return restored == original;
}
```

The implemented small path caller, with `original` as the literal fixture above
and `path` naming a file in a caller-created controlled directory, is:

```cpp
MF::save(path, original);
auto restored = MF::load(path);
const bool equal = restored == original;
```

The caller owns directory creation, single-writer access, candidate adoption and
any explicit retry. `files.round_trip` exercises this path against real files,
including independent owned values and exact encoded-byte comparison. No CLI
or implicit application path is introduced by the adapter itself.

The implemented small U3 consumer is:

```text
map_storage_cli round-trip <file>
map_storage_cli verify <file>
```

The caller creates/controls the directory. Round-trip deliberately saves the
literal fixture, may replace an existing regular target, then loads and compares
all owned values. Verify only loads and compares; it creates/rewrites/repairs
nothing and never supplies defaults. `fixture.h` holds independent literal C++
expectations with the exact identities/cells above. MS-008 owns the CLI's argument,
exit/output and diagnostic contract; [next.md](next.md) distinguishes source
inspection, process checks and remaining acceptance.

Extending `FileIndex` or `FileRegistry` with map state would mix discovery and
serialization and would not supply publication guarantees. Extending graphics
`Loader::upload()` would introduce native ownership and its partial effects.
Evidence: E01-E04, E07, H01-H04. **Confidence: high** in the required CPU boundary;
medium in the private API shape. Reversal affects only private codec/tests and
records; no public callers exist. No historical persistence preference is proven.

## MS-002 Logical identity and coordinates

| Value | Private identity and meaning |
| --- | --- |
| Map | Owned qualified key `fixture:two-layers`; independent of filename or runtime instance address. Loading twice creates independent values with the same content identity. |
| Layer | Map-local key, such as `ground` or `overlay`. Unique within that map. Vector order is authored order and must survive storage. |
| Cell | `std::optional<std::string>`: absent means empty; present holds a semantic qualified tile key such as `fixture:grass`. Repeated keys are valid. |
| Spawn | Map-local unique name `entry`, a layer key, and integer cell coordinates. It describes a point; it creates no entity and imposes no walkability rule. |
| Position | Zero-origin finite orthogonal grid; +x right, +y down; `index = y * width + x`. Shared width/height for all layers. Bounds are half-open. |

Map and tile keys use exactly one colon: namespace `[a-z0-9][a-z0-9._-]*`, then
local name `[a-z0-9][a-z0-9_-]*`. Layer and spawn keys use the local-name grammar.
Each complete key is at most 128 ASCII bytes. Comparisons are exact and
case-sensitive, with no normalization or basename fallback. Empty strings and
numeric sentinels are invalid; JSON `null` represents an empty cell. Layer and
spawn namespaces are distinct. Spawn array order is preserved for fidelity but
has no activation meaning.

Semantic keys deliberately do not identify a cached tileset, texture, source cell
ordinal, Wang terrain label, selection hash, or animation frame. Current authored
tilesets use `namespace:name`, and their original grid cell is meaningful only
within that definition. A future visual adapter can bind a semantic key to a
tileset ID and original cell, with an appropriate content revision policy. This
fixture invents no artwork semantics or binding catalogue. Syntactically valid
tile keys can round-trip without any matching installed assets.

Alternatives are storing `(tileset ID, original cell)` directly or using a
map-local numeric palette. The first couples saves to atlas revisions; the second
adds indirection before size demands it. Full semantic strings cost more bytes
but keep this experiment small and independent. A later palette can preserve
these identities; changing their meaning requires explicit migration.
Evidence: E05-E06, E09, H02-H04. **Confidence: medium** in semantic keys and dense
ordered layers; low as a claim about personal taste. Coordinates are a reversible
fixture convention, not an agreement with the sibling tilemap branch. U1's value
comparisons and independent copies pass. Changing semantic identity or coordinate
meaning would require a deliberate format change once files acquire consumers;
at present the reversal affects only this private codec and fixtures.

## MS-003 Format and version

Use one UTF-8 JSON object with `format: "cheryl.map-fixture"` and integer
`version: 1`. The format name deliberately scopes the experiment. This is neither
graphics manifest version `"2.0"` nor a universal application/save-game schema.
The file extension `.map.json` is a hint; the header controls interpretation.
No graphics index, `$schema` fetch, filesystem reference, or asset discovery is
needed. The codec accepts this fixture; the executable golden encoding uses the
same values with fixed two-space field indentation and compact cell arrays:

```json
{
  "format": "cheryl.map-fixture",
  "version": 1,
  "id": "fixture:two-layers",
  "width": 3,
  "height": 2,
  "layers": [
    {"id": "ground", "cells": [
      "fixture:grass", "fixture:stone", "fixture:grass",
      "fixture:grass", "fixture:grass", "fixture:stone"
    ]},
    {"id": "overlay", "cells": [
      null, "fixture:wall", null,
      null, null, "fixture:wall"
    ]}
  ],
  "spawns": [{"name": "entry", "layer": "ground", "x": 1, "y": 1}]
}
```

Every shown property is required, including arrays that may be empty where
permitted below. Object property order is immaterial to readers. Writers choose
a fixed field order, two-space indentation, LF endings, and one final newline;
they preserve both array orders. Deterministic encoding is useful for review,
but round-trip acceptance compares values, not arbitrary input whitespace.

Readers accept exactly the supported format and version. A recognizable different
format yields `UnsupportedFormat`; another nonnegative uint32 integer version,
including 0 or 2, yields `UnsupportedVersion`. Missing/wrongly typed headers or
out-of-range integer encodings are invalid data.
There is no inferred version, downgrade, automatic rewrite, or migration in this
milestone. An incompatible field or semantic change needs a new version and a
separately selected reader/migration. Unknown properties in version 1 reject.

The codec uses the already declared nlohmann/json dependency privately, with no
public JSON types or pin changes. The coordinator populated gitlink
`b36f4c477c40356a0ae1204b567cca3c2a57d201`; the build uses its private header path
and statically requires version 3.11.3. The parser uses public SAX events with a
bounded DOM builder. Encoding validates first, then appends known schema syntax
and validated ASCII identifiers into a bounded string, avoiding an oversized
temporary serialization. No system JSON header or newer strict-NUL macro is used.

JSON aids hand inspection and uses an existing authored-input vocabulary. Binary
encoding would add byte-order/layout and inspection work; a new ad hoc text grammar
would add another parser; adopting Tiled or another interchange format would
introduce semantics absent from this consumer. The existing graphics schema
describes atlas assets, not world cells/spawns. None is ruled out for later needs.
Evidence: E03, E05, E08, H02-H03. **Confidence: medium** in a reversible JSON
prototype; low in any inference that the user prefers it for all persistence.
Changing the private representation remains cheap while persisted files have
only disposable private test consumers; a later accepted format needs explicit
version handling for incompatible changes.

## MS-004 Validation and load publication

Read bounded bytes, parse one complete object, validate a candidate, and return a
new owning `Snapshot` only on complete success. No caller-provided map is mutated.
Acquisition consumes at most 8 MiB plus one detection byte and rejects every raw
NUL before parsing, including after a valid document. This guards JSON 3.11.3's
raw-NUL-as-EOF behavior. Signaled stream failures report `Read`. SAX limits and
decoded duplicate names can reject before a later syntax error; syntax/UTF-8
errors and numeric token overflow carry the parser's one-based byte offset.
After a complete bounded parse, check format/version, then version-specific
structure/semantics. No global error ordering is promised across these phases.
Truncated tokens/containers, trailing non-whitespace, comments, invalid UTF-8 and
a second document reject. No default/empty map masks failure.

Version 1 validation requires:

- Exact required field sets and types at every object. Reject duplicate JSON
  member names rather than accepting a DOM parser's last value. Booleans,
  fractions, and floating encodings such as `1.0` are not integers.
- Width and height each in 1..1024, 1..16 layers, 0..256 spawns, and at most
  262144 cell entries across all layers. Each layer has exactly `width * height`
  entries. Check ranges and multiplication before narrowing, allocation, or index
  arithmetic, and accumulate counts without overflow.
- Valid bounded keys, unique layer IDs, and unique spawn names. Every cell is
  `null` or a valid tile key. Each spawn references an existing layer and satisfies
  `0 <= x < width`, `0 <= y < height`; no numeric coercion is allowed.
- At most 8 MiB of document bytes and nesting depth 16 (root container is depth
  1; nested containers add one). Every decoded JSON string/member name is capped
  at 128 bytes. Before inserting DOM entries, cap any array at 262144 elements,
  each object at 256 members, and total nodes at 300000, counting every value,
  container and decoded member name. Per-object name sets compare decoded keys,
  including escaped equivalents; each sibling object gets a fresh set. Violations
  throw `Error`; a DOM callback returning false is not used to discard fields.
- Validate width/height/layer products with division guards before Snapshot
  allocation. All numeric fields require nonnegative uint32 integer encodings;
  booleans, fractions, exponents and overflows reject without narrowing. Capacity
  violations report `LimitExceeded`; wrong types/ranges/semantics report
  `InvalidValue`. Depth/node boundaries can pass SAX admission but still fail
  the map schema, since a valid v1 map is shallower and has fewer nodes.

The encoder revalidates mutable aggregates, enforces the same limits and the
encoded-byte cap while appending; it never produces more than 8 MiB. The codec
opens no output file. A semantically valid snapshot whose encoding would exceed
the byte cap rejects without changing the input. The limits are local operational choices,
not historical preference rules or an infinite-world capacity claim.

Asset existence, atlas bounds, collision, and spawn occupancy are intentionally
outside structural validation. Unknown semantic tile keys are preserved; a future
resolver reports an unresolved binding without silently turning it into empty.
Permissive loading or dropping unknown fields would hide misspellings and lose
future data on rewrite. Strict rejection trades convenience for explicit changes.
Evidence: E03-E06 and the acceptance brief. **Confidence: high** in complete
validation and unchanged caller state; medium in these initial budgets. U1's
executed cases cover these limits, duplicate handling and preservation. Budgets
are bounds on acquired bytes and logical DOM growth, not an exact heap-byte or
latency guarantee. They remain private constants: revising them costs focused
boundary tests and record updates. The current manifest parser alone would not
have supplied these guarantees.

## MS-005 Ownership and failure reporting

Implemented private operations:

```cpp
Snapshot parse(std::istream& input, const std::filesystem::path& origin);
std::string encode(const Snapshot& snapshot);
```

Streams and snapshot inputs are borrowed only through the call. Returned snapshots
and encoded bytes own all data; no iterators, JSON views, registry references,
native handles, or callbacks escape. `origin` supplies diagnostic context only.
`origin` is copied for diagnostics without opening/resolving it. The snapshot
stores no absolute path or stream lifetime. The parser consumes stream-buffer
bytes directly and preserves flags/exception masks, so `bad_alloc` and programming
exceptions from a stream buffer are not swallowed by an istream catch-all. An
unreadable initial state or a signaled `ios_base::failure` becomes `Read`; an EOF
reported by the buffer is treated as EOF. Silent device errors are outside this
memory-stream unit. U2 uses a private native-read stream buffer: negative read
results signal `ios_base::failure`, zero alone means EOF, and EINTR is retried.
Even a read error after a complete valid JSON document must reject. Native reads
fetch at most 8 MiB plus one detection byte; the existing parser still enforces
NUL and all structural/semantic limits. No ordinary ifstream flag is relied on.
Parsing and encoding perform no filesystem discovery or registry publication.

All operations run synchronously on the calling CPU thread. Independent values
can be used independently, with no singleton or hidden cache. The caller keeps
borrowed streams/snapshots stable, serializes access to the same input object, and
settles calls before destroying inputs. No job acceptance/completion split,
asynchronous cancellation, game thread, or shutdown pump is introduced.

The owning private `Error` derives from `std::runtime_error` and carries a `Code`,
operation, diagnostic origin, field location, optional one-based byte offset,
and an I/O error code when a stream signals one. U1 categories are `Read`,
`Malformed`, `UnsupportedFormat`, `UnsupportedVersion`, `InvalidValue` and
`LimitExceeded`. U2 appends `Open`, `Write`, `Replace`, `UnsupportedPlatform`,
retaining the same final Error type. File errors identify operation `load` or
`save`, the retained absolute path (original input when invalid/unresolvable) and `$path`,
`$open`, `$create`, `$write`, `$close` or
`$replace`, plus native errno. Codec errors retain `parse`/`encode` operation and
their locations; encoding diagnostics use origin `<snapshot>`. NUL in a diagnostic
path is printed as `\0`, while the owned `origin` retains the exact input.
`cleanup_error` and owned `staging_leftover` identify a failed unlink separately
from the primary error; they do not change its category/location/message.
U2F retains the original component order, including `.`/`..`, in that absolute
path. Native lookup and owning diagnostics therefore refer to the same target
even when an ancestor is a symlink. This is not canonical spelling or a security
boundary. Relative inputs use the captured current directory once; the adapter
does not change process cwd. Invalid empty/NUL paths still reject before syscalls.
Messages distinguish malformed bytes from an unsupported version and identify
the failing field. Unexpected allocation errors can propagate normally. Cleanup
must not replace the primary failure or throw from a destructor.

Failed parsing/assignment leaves existing snapshots intact; encoding also
preserves its input. Path load checks close before returning a candidate;
MS-006 implements file preservation. Borrowed paths/snapshots and private FileOps
references/handles never escape a synchronous call.
The CLI owns process exit handling and prints diagnostics; the codec does not log
to global engine services. A caller can adopt a successful return by moving it
into its own state after validation.

An explicit result variant would also be viable, especially if recoverable errors
become frequent in a future UI. Exceptions currently fit a short synchronous
consumer and several existing readers, but historical errors are inconsistent.
Reusing private engine exceptions would add unnecessary Engine coupling; empty
returns would obscure failures. Evidence: E01-E04, E07, H02-H04.
**Confidence: medium** in this private error shape; high in ownership isolation.
It is an engineering choice, not a recovered personal error-handling rule.
U1 injected I/O, allocation and programming failures and retained diagnostics
passed focused checks. U2 adds native-read, close, file-phase and leftover tests;
see [next.md](next.md) for actual results. Changing error style affects only the
private caller/tests now; no general result abstraction or public exception
hierarchy is selected.

U3 renders owning Error fields through `Cli::print_error`: category, operation,
origin and location, known byte offset and native error category/value/message,
the primary `what()` detail, then separate cleanup category/value/message and
owned staging leftover when present. Private formatter cases cover fields not
reliably produced by ordinary CLI fixtures (including combined primary/cleanup
failures); real process tests cover normal storage diagnostics. No error data is
borrowed from JSON or native handle lifetimes. Storage errors map to exit 2;
allocation/programming and output/reporting failures reach the outer process
boundary as exit 1, preserving the codec's narrow exception translation.

## MS-006 Failed-write preservation

Define the commit point as successful replacement of the destination directory
entry. The first executable profile is a single writer to a regular local Linux
file, or an absent file, in an existing caller-owned directory. Reject a symlink or
nonregular destination. Assume the caller controls the directory and does not
concurrently change paths. A future platform adapter must establish equivalent
replacement behavior; unsupported platforms reject before touching the target.

The implemented private file adapter (`file-storage.cpp`, E11) does the following:

1. Reject empty paths and embedded native NUL before any filesystem syscall.
   Make absolute once with `absolute(...)`, preserving every path component for
   native Linux lookup and owning diagnostics. Never lexically collapse `..`
   across an ancestor symlink: `base/link/../map.json`, with link pointing to
   `other/sub`, selects `other/map.json`, not `base/map.json`. Keep existing
   empty/`.`/`..` leaf checks. Validate and encode the complete bounded snapshot
   before staging. Inspect the existing parent with stat and final target with
   lstat; reject final symlinks (including dangling links) and nonregular targets.
   Ancestor symlinks remain allowed within the controlled-directory assumption;
   the final entry is not canonicalized through or silently accepted as a symlink.
2. Exclusively create a uniquely named staging file in the same directory, retaining
   its handle and ownership. Linux `mkostemp(..., O_CLOEXEC)` uses a six-character
   unique suffix and exclusive create. Its fixed prefix is selected to be
   disjoint from the target name, including temporary-looking absent targets.
   Never open a guessed existing path for
   truncation. A private staging file may become the new file with mode 0600;
   preserving old ACLs/permissions is outside this first consumer.
3. Use unbuffered native writes; loop over short writes and EINTR, reject zero
   progress, and check close. There is no output buffering/flush stage to test.
   Relinquish descriptor ownership *before* Linux close, including on EINTR;
   never retry a possibly reused descriptor. Close successfully before
   publication. Every preceding error discards only this operation's staging
   file, best effort.
4. Perform one same-directory native replacement (`rename` for the initial Linux
   profile). Never delete/truncate the old target or use remove-then-rename as a
   fallback. Replacement failure must retain the old valid file; exercise this on
   the supported filesystem rather than inferring it from mocked operations.
5. After replacement, return success without a later fallible validation, logging,
   or reopen step that could report an unchanged-target failure after commitment.

Before the commit point, failure leaves an existing valid file byte-for-byte
unchanged, or leaves an absent target absent. Report the primary phase/error;
on cleanup failure retain the primary Error and attach the owned staging path
and separate cleanup errno without allocating (move the preexisting string).
An already missing staging entry is clean. Cleanup attempts unlink once and
never retries close; destructors are no-throw. Unexpected allocation/programming
exceptions retain their types and receive best-effort cleanup rather than
conversion to Error. Do not sweep other temporary files or automatically retry.
After a successful call, a new load reads the new complete document. A later CLI comparison
failure is a separate read/verification outcome, not a claim that save rolled back.

Load opens with `O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK`, then fstat checks
the retained handle. Nonblocking prevents accidental special-file open blocking;
it does not extend the supported profile beyond regular files. Reads and close
are checked as MS-005 describes. Unsupported-platform branches throw before
resolving/opening the path or encoding the candidate. They are source-reviewed,
not executed on this Linux host.

E11 tests cover successful creation/replacement/reload, short I/O/EINTR, exact
8 MiB input and one-byte detection, raw NUL/truncation/trailing bytes, file-phase
errors, fd reuse after close failure, and preservation of valid A or absent target
while attempting distinct B. A deliberate exclusive collision reaches native
`open(O_EXCL)` and preserves the unowned entry. Cleanup failures keep primary
errno and a readable B staging leftover; explicit retry neither sweeps it nor
damages unrelated entries. Invalid/oversized encoding creates no staging file.

The real-rename failure test closes the fully written staging file, removes only
that owned entry inside a narrow injected boundary, then calls actual Linux
`rename(staging, existing_A)`. It observes ENOENT, exact unchanged A bytes and
successful reload, followed by successful explicit retry. This is native syscall
failure evidence, not a permission/quotas claim. A separate real save proves a
read-only target file can be replaced; mode 0400 is not used to fake rename
failure. All fixture changes/cleanup stay in unique clone-build child directories.

U2F's `files.ancestor_parent_*` regressions (E12) seed distinct valid A/B byte
sentinels at both native and lexically simplified candidate locations. Before
the production change, all four cases fail: load selects B, save replaces B
instead of A, and absent native destinations remain absent. The correction
retains components in the absolute path. Final checks cover absolute/relative
forms and existing/absent native targets, exact untouched B bytes and decoded B,
correct load and C replacement/reload, retained diagnostic origin, and cleanup
after an injected replacement failure in the native parent. Fixtures never
change process cwd; they derive relative inputs from their existing cwd and
assert it is unchanged. Actual commands/results are in [next.md](next.md).

Canonicalizing only the existing parent is a viable alternative, but adds a
lookup and a different diagnostic spelling policy. Canonicalizing the complete
path would require target existence and extra care to reject final symlinks.
Keeping components is the narrow correction and works for absent destinations.
**Confidence: high** in native selection for the tested Linux profile; reversal
cost is limited to this private resolver/tests and diagnostic spelling. No
personal preference inference or concurrency/security guarantee is added.

This profile covers ordinary process-visible I/O failures, not power-loss
durability, media corruption, network filesystems, hostile directory races, or
multiple writers. It makes no fsync/journal/recovery promise. Proving replacement
and its error boundary is mandatory before claiming failed-write acceptance.
If the selected filesystem cannot support it, reject the capability; do not weaken
the guarantee silently. Windows replacement and durable-save policy remain later
work. Direct truncating writes are simpler but cannot satisfy preservation;
backup rotation or a database adds recovery policy without a present consumer.
Evidence: the acceptance brief; E01-E04/H01-H05 establish no existing transaction
to reuse. **Confidence: high** in the preservation requirement and publication
sequence; medium when generalizing beyond this local Linux filesystem. Routine
native I/O choices are engineering decisions, not inferred user taste. Actual
commands/results and platform limits are in [next.md](next.md).
Changing the platform replacement mechanism is isolated to this private adapter;
the CPU codec and logical format need not change.

The U3 permission fixture executed the actual normal CLI against a distinct valid
predecessor with exact byte sentinels. Its owned mode-0500 parent rejected native
exclusive staging at `$create` with EACCES (13), CLI exit 2; `finally` restored
only that parent's original 0755 mode. Bytes stayed exact and verify returned 3
before/after: a successful full load intentionally differs from the fixed fixture,
rather than a storage rejection. Explicit retry and a fresh verify both returned
0. No test-only write fault control was needed on this UID-1000 Linux/ext4 host.
Read-only stdout and broken-pipe cases each returned 1 after completed publication;
their diagnostics reported that fact and separate verification returned 0.
Exact commands/counts/evidence are in [next.md](next.md).

## MS-007 Integration and implementation placement

The implementation is private under `projects/tests/map-storage/` with namespace
`MapFixture`. The standalone CMake entry adds `map_storage_cli`, linking only
codec/adapter and the ordinary C++ runtime. The existing test runner adds pinned
gtest/gtest_main; CLI acceptance uses the already available Python interpreter
and standard library (no package install). CLI cases are registered for Linux,
the accepted file profile. No Engine/platform/graphics target or sibling is used.
Generated compile/link inputs and executed headless evidence are recorded in
[next.md](next.md). The current Engine target has
additional dependency edges (E08), so an
Engine-linked headless process alone would not establish this smaller boundary.
Do not export a new SDK component before the caller and failure checks work.

| Neighbor | Overlap to record | Boundary retained |
| --- | --- | --- |
| Tilemap | Semantic tile IDs, empty cells, ordered layer identities, dimensions, origin/axis conventions, and snapshot extraction/adoption. | Use this branch's own fixture. A future adapter can intern IDs, invert Y, map to source cells, or flatten chunks. Storage does not choose drawing, culling, tile size, animation time, or tilemap container ownership. Authored layer order is preserved; a future renderer chooses how to consume it. |
| Authored assets | A visual binding may reference current qualified tileset IDs and original grid cells. | Keep asset definitions/images and map content distinct. Graphic manifest schema/version is not map format/version. Missing bindings need an explicit application outcome and content-revision policy. |
| Future level lifecycle | A map identity and a named spawn can select content and an entry point. | Loading returns an inactive candidate. A future lifecycle owner validates/resolves it and decides when to replace an active level. Storage creates no entity, changes no active game, schedules no unload, and owns no renderer resource lifetime. |
| Settings/dialogue and save games | They may eventually benefit from the publication mechanism. | Do not share a new format or make a sibling branch a dependency. Session flags, entity IDs, RNG/time, save slots, content migrations and multi-file transactions need their own demonstrated consumers. |

A shared world model now would lock in sibling choices that have not been
reviewed. Private duplication costs a later adapter/extraction, but leaves both
branches independently demonstrable. Promote only the proven reusable boundary
after the CLI and fault cases, keeping internal decision IDs current. Evidence:
E05-E09, H02-H05 and the workspace brief. **Confidence: high** in independence
and deferred lifecycle policy; medium in this private placement. Relocating these
private targets is cheap; public extraction remains deferred. Final public
names, integration ownership, and save-game composition remain unselected.

## MS-008 Private CLI contract

`round-trip <file>` and `verify <file>` each require exactly one explicit nonempty
path. Unknown commands (case-sensitive), missing/extra arguments and empty paths
write usage to stderr and return 64 before file work. Standalone `--help` writes
usage to stdout and returns 0; usage explicitly states that round-trip writes
and may replace an existing regular file, and verify only reads. No implicit
filename, asset search, cwd change, confirmation or automatic retry exists.

Round-trip calls save, records successful completion, then calls load. Verify
only calls load. Both compare `Snapshot` equality against a newly constructed
literal fixture: map ID, dimensions, layer count/order/IDs, every cell/empty,
spawn count/order/names/references/coordinates. Success prints the map identity,
3x2 dimensions, `[ground,overlay]` and `entry@ground(1,1)` to stdout. Valid but
different content writes a complete-snapshot mismatch to stderr and returns 3.
Storage/format Error writes structured fields plus primary/cleanup detail to
stderr and returns 2. Unexpected exceptions/failures return 1. Output failure
also returns 1; error-reporting failure cannot claim a successful diagnostic.

The CLI flushes and checks stdout, detaches stderr's tie to stdout, and ignores
SIGPIPE in its own process where available so broken pipe output becomes an
ordinary checked failure rather than an unreported signal exit. Once save has
returned successfully, any later failure also prints that save completed and
subsequent failure does not undo publication. It does not claim prior-byte
preservation for load/comparison/output failures after that commit point.

The process harness uses independent literal JSON expectations and actual execs,
not a function standing in for main. It retains per-invocation stdout/stderr,
actual/expected status, PID and argv in unique build-local child directories.
Coverage includes fresh verification, valid changes across all fields/cells,
format failures, path/argument rejections, U2F native path meaning, actual parent
permission failure with reliable restoration/retry and post-save stdout failure
through a read-only descriptor and a pipe without a reader. It uses no normal-CLI
fault controls. Nonzero CLI outcomes are deliberate fixture results, not passing
claims based on a zero-test runner; actual counts/results belong in next.md.

An in-process main shim would be smaller but would not establish independent
executable/stream/exit behavior. A generic serializer/comparator/result framework
would add policy without a new consumer. These engineering choices are narrow
and private, not recovered personal taste. **Confidence: high** in the explicit
requirements, selected isolation and checked behavior; medium in private output
wording, which is not a published machine protocol.
Reversal affects only private CLI, formatter and harness. No SDK extraction or
new persistence format is selected by this unit.

## Source evidence

Current-source references E01-E09 describe inspection at
`3542f883391d4225329dab8d5d61a5987e69c8f0`. They are compatibility facts, not
evidence of personal preference, even where Git lists the established author
identity. The baseline manifest tests below were not executed; private implementation
and executed-test evidence are identified separately as E10-E13.

| ID | Exact source locations and observed contract |
| --- | --- |
| E01 | [FileIndex declaration](../projects/engine/include/cheryl/core/resources/fileio/file-mgr.h), lines 17-38, 43-64; [implementation](../projects/engine/src/core/resources/fileio/file-mgr.cpp), lines 13-45. Constructor scans recursively; visited roots do not refresh; lowercased dotted-extension buckets borrow index storage. No content I/O or writer. Errors can leave a partial index. |
| E02 | [FileRegistry declaration](../projects/engine/include/cheryl/core/resources/asset-management/file-registry.h), lines 16-48; [implementation](../projects/engine/src/core/resources/asset-management/file-registry.cpp), lines 7-58. Normalized absolute paths grouped by basename, deduplicated and sorted; owned query results; exact-path lookup separate from first-basename selection. No content open or persistence. |
| E03 | [Reader declaration](../projects/engine/include/cheryl/core/resources/asset-management/manifest-loader.h), lines 16-39; [index reader](../projects/engine/src/core/resources/asset-management/graphics-manifest-loader.cpp), lines 33-57, 60-107, 114-145; [parser](../projects/engine/src/core/resources/asset-management/manifest-loader.cpp), lines 31-70, 97-124, 700-720, 920-933. Explicit selected indexes, graphics-relative exact registered references, version `"2.0"`, owned CPU definitions, field/type checks and exceptions. Unlisted JSON is not parsed. There is no map encoder or replacement protocol. |
| E04 | [Loader declaration](../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h), lines 26-41, 81-115, 149-200; [implementation](../projects/engine/src/core/resources/asset-management/asset-loader.cpp), lines 27-42, 163-208, 368-417. Fresh discovery/private registry, CPU parsing/source snapshots/PNG decode, then provider-verified registry and shader/image/grid publication. Earlier cache effects survive upload failure; retained definition snapshots publish only at the end. `register_files()` publishes unmanaged paths without decoding/parsing. |
| E05 | [Tileset definition](../projects/engine/include/cheryl/assets/definitions/tileset.h), lines 35-55, 87-101; [ID implementation](../projects/engine/src/assets/definitions/tileset.cpp), lines 4-14; [manager](../projects/engine/src/core/resources/asset-management/tileset-mgr.cpp), lines 15-60. Definition IDs are `namespace:name`; cells belong to a grid; Wang hashes are lookup helpers. Manager resolves uploaded textures/materials, builds geometry and publishes absent cache IDs, so it is not a CPU map loader. |
| E06 | [CPU selector](../projects/engine/include/cheryl/assets/selection/tile-selection.h), lines 18-43, 55-79; [runtime tileset](../projects/engine/include/cheryl/assets/types/2d/tileset.h), lines 19-24, 63-98. Application owns world coordinates/sampler and time; selector returns a source-cell result or an ordinary unresolved result. Runtime tile values retain uploaded resources. These supply neither world storage nor persistent instance IDs. |
| E07 | [First-game consumer](../projects/apps/first-game/src/first-game.cpp), lines 35-56; [tutorial consumer](../projects/apps/tutorial-game/src/game.cpp), lines 37-54; [asset demo](../projects/apps/demo/src/asset-demo.cpp), lines 73-100; [audio demo](../projects/apps/demo/src/audio-demo.cpp), lines 32-44. Games upload then retrieve named assets; demo can parse an explicit manifest, then load resources. Audio opens/streams a discovered path separately. Discovery, parsing, and native realization have different owners. |
| E08 | [Engine build declarations](../projects/engine/CMakeLists.txt), lines 21-45; [.gitmodules](../.gitmodules), lines 28-30; baseline tree gitlink `extern/json` at `b36f4c477c40356a0ae1204b567cca3c2a57d201`. Engine uses C++23 and private JSON includes, with other dependency links. The header was absent at the design checkpoint; it is now populated by the coordinator. Engine itself was not configured/built for U1. |
| E09 | [Status](../docs/STATUS.md), lines 100-103; [manifest metadata gaps](../docs/planning/long-term/asset-manifest-metadata.md); [discovery guide](../docs/site/assets/file-and-font-discovery.md), lines 16-62. Documented intent distinguishes save serialization from manifest parsing and defers world storage. Missing semantic artwork metadata must not be invented. The current task selects this narrow experiment without reconciling published roadmaps. |
| E10 | U1 [values](../projects/tests/map-storage/snapshot.h), [private API](../projects/tests/map-storage/codec.h), [codec](../projects/tests/map-storage/codec.cpp), [focused cases](../projects/tests/map-storage/codec-tests.cpp) and [standalone build](../projects/tests/map-storage/CMakeLists.txt). Actual commands and saved 86-case results are recorded in [next.md](next.md). These are new engineering choices, not personal preference evidence. |
| E11 | U2 [path API](../projects/tests/map-storage/file-storage.h), [native adapter](../projects/tests/map-storage/file-storage.cpp), [private fault boundary](../projects/tests/map-storage/file-operations.h), [real-file/fault tests](../projects/tests/map-storage/file-storage-tests.cpp). The unchanged schema/codec is used through its existing stream boundary; Error gains only file categories/cleanup fields and printable NUL path handling. Build/test commands, saved logs, nonempty selections and outcome limits are in [next.md](next.md). This is current agent-authored compatibility evidence, not user preference evidence. |
| E12 | U2F [resolver](../projects/tests/map-storage/file-storage.cpp) `Detail::resolve` preserves components in `absolute(input)`; [file regressions](../projects/tests/map-storage/file-storage-tests.cpp) `files::ancestor_parent` and four `files.ancestor_parent_*` cases distinguish native and lexically simplified targets. Expected pre-fix failure and final discovery/build/native outcomes are saved separately in [next.md](next.md). This corrects the E11 implementation contract; no historical preference is inferred. |
| E13 | U3 [consumer](../projects/tests/map-storage/cli.cpp), [literal fixture](../projects/tests/map-storage/fixture.h), [diagnostic renderer](../projects/tests/map-storage/cli-diagnostics.h), [formatter tests](../projects/tests/map-storage/cli-diagnostics-tests.cpp), [process harness](../projects/tests/map-storage/cli-checks.py), and direct [standalone target/registration](../projects/tests/map-storage/CMakeLists.txt). No codec/adapter/schema change. Implementation facts and delegated CLI requirements are engineering evidence; actual commands/counts/outcomes and source/link inspection are in [next.md](next.md), not preference evidence. |

Existing checks worth reusing as contract examples, not as storage coverage:
[manifest tests](../projects/engine/tests/all-tests/src/assets/manifest.cpp),
lines 124-137, 165-175, 271-306, inspect index selection, qualified tileset IDs,
and invalid-cell diagnostics. They are selected into the unit suite by
[test routing](../projects/engine/tests/CMakeLists.txt), lines 4-13, 20-23, and
[unit registration](../projects/engine/tests/unit/CMakeLists.txt), lines 1-8.
They neither establish a writer nor need to be run for this records-only change.

### Historical provenance and preference limits

All historical paths below are relative to the named commit, not the current
tree. Reproduce a location with `git show <full-commit>:<path>` and number that
blob's lines. Both the root investigation and a bounded read-only subagent
inspected relevant historical blobs; the root checked the cited distinctions.

The archive boundary is explicit: `eb3f81c219e779b0200c1e82e5bc7239386528e8`
is `V3.0a (#12)`, whose commit body includes partial implementations, incomplete
refactoring and WIP. `8854ef84f360fafa3572b58ee5440b5aabc0e67c` retires that
superseded snapshot and restores a baseline for replaying modern development.
`3f65efc8a821e9e68f05e76ed12f833d62696a36` marks the flattened-history
demarcation point. The earlier modern examples below retain 2024 author dates
but were replayed in 2026; replay/author identity alone proves neither intent nor
personal preference.

| ID | Exact historical provenance | Inference, confidence, and counterevidence |
| --- | --- | --- |
| H01 | `9fdf2fa3642c929948f55df38876374a97767cf0` (`Adds FileIO`): `include/cheryl/resources/fileio/file-mgr.h:10-17`; `src/resources/fileio/file-mgr.cpp:11-46`. | Concrete modern pre-demarcation v3 example: path construction, explicit scan, borrowed extension buckets. High confidence in behavior; medium as intentional API evidence. It contains no reader/writer; borrowed discovery results imply no preference for borrowed loaded maps. |
| H02 | `5ddd560c4c4fc298d1d6af92355e333c1b1ebbc8` (`Adds assets`): `src/assets/2d/tileset.cpp:22-52`. | Path-taking JSON loader returns `TilesetData` by value with explicit grid metadata. High confidence in observation; medium support for a narrow JSON experiment, low personal-format inference. It immediately uses `TextureMgr`/geometry memory and returns `{}` on missing metadata, so it supplies no CPU persistence or strict failure contract. |
| H03 | Same `5ddd560c4c4fc298d1d6af92355e333c1b1ebbc8`: `src/assets/2d/sprite.cpp:49-89`. | Named animation keys coexist with physical input paths; open/missing-data failures throw. Medium/low support for considering semantic names; no universal identity system follows. Error behavior differs from H02's empty fallback, so historical errors are not a consistent preference rule. |
| H04 | `d69b0a2377dc9f4b688904ddca5d199c5b49ad11` (`Adds asset loading & managers`): `include/cheryl/templates/asset-mgr.h:12-29,44`; `src/resources/assets/asset-loader.cpp:7-27,57-65`. | Path-keyed shared-pointer caches and explicit loading orchestration establish asset lifetime vocabulary, not save ownership. High observation confidence; low preference inference. The loader requests `"json"` although indexing stores `".json"`, and dispatches four of five managers. Copying such incidental details is unjustified. |
| H05 | `eb3f81c219e779b0200c1e82e5bc7239386528e8`: `src/core/filemgr.cpp:49-82,85-120`; `include/core/resources.h:15-27`; `src/asset-types/tile-set.cpp:1-7,10-64,85-95`. | Archived manifest loading registers directory contents; unresolved Policy and unload TODOs identify experiments. Legacy-looking `TileSet` uses stale includes, `GameAssets`, ad hoc text/assertions and direct GL. Original v1 origin of that blob is unproven here; neither older v1 leftovers nor tentative v3 import membership is evidence of intentional v3 storage taste. Excluded from positive preference evidence. |

No inspected historical example implements map writes, version migration, atomic
replacement, or a universal save-game facility. The proposed ownership, logical
identities, validation, and failed-write rules are new engineering choices tied
to this consumer. Intentional-looking v3 examples constrain inference only to
the demonstrated caller shapes, with their counterevidence retained.

## Implementation units

U1 and U2F are accepted. This assignment implements U3 only and stops for
coordinator acceptance. Executed evidence is recorded in [next.md](next.md).

1. **U1 — CPU value and codec, complete.** Private snapshot types, equality,
   validation, bounded JSON parse/encode and 86 passing focused checks under
   `projects/tests/map-storage/`. Its codec uses only standard C++ and private
   declared JSON headers; the test runner adds pinned gtest. [next.md](next.md)
   owns reproducible commands and verification limits.
2. **U2 — File publication adapter, implemented.** Explicit-path load/save,
   checked native reads, same-directory exclusive staging and Linux replacement,
   with a narrow private file-operations seam and focused real-file/fault tests.
   Accepted U2F preserves native ancestor-symlink/`..` semantics with 122 passing
   file/codec cases.
   No expansion to general save slots,
   recovery scanning, background services or cross-platform guarantees.
3. **U3 — CLI acceptance, implemented.** Literal fixture, round-trip/verify,
   complete equality, owning stderr diagnostics and process-level checks. Keep
   source/observed-results distinctions and acceptance with the coordinator;
   extraction and integration require a later assignment. No rendering, game
   startup, sibling merge or artwork is needed.

Each unit keeps the decision summary and next action consistent with actual
evidence. The U1 pre-interruption checks used the then-current shared
wrapper and clone-local Debug output, with four compilation jobs. The memory
continuation permits only one compilation/test job through the wrapper.
U2 reuses the incremental Debug build and reruns the 86 codec cases because the
private Error diagnostics/header and target changed. No dependency retrieval,
system installation, device run or documentation generation was performed.

## Observable acceptance

This table retains the full milestone requirements. U1 demonstrates the
in-memory portions of A3/A4 and codec isolation in A7, plus memory round-trip,
ownership and deterministic encoding. U2 exercises A5/A6 via private real-file
and fault tests, plus checked load and codec regressions. U3's standalone round-trip
and fresh verify both exited 0; all 136 cases passed (the accepted 122, two formatter
cases and twelve process cases with 71 actual CLI invocations). These demonstrate
the private CLI portions of A1/A2/A7 on this host; coordinator acceptance remains
separate. Commands, outputs/statuses and limits are recorded in [next.md](next.md).

| Check | Required observation |
| --- | --- |
| A1 — Content | `round-trip` exits 0 and reports the map, two layers, and `entry`. Compare ID, 3x2 dimensions, both ordered IDs, all twelve cell values including empties, and spawn name/layer/(1,1). The asymmetric fixture distinguishes transposition and layer swaps. |
| A2 — Fresh read | A separate `verify` invocation opens the produced bytes and compares to the literal expected snapshot. A second independent snapshot remains unchanged when the first is edited/destroyed. No hidden cache can supply the result. |
| A3 — Format failures | Empty input, invalid JSON, truncation inside a token/container and trailing garbage produce a malformed diagnostic. A valid object with another format or integer version 0/2 produces the corresponding unsupported diagnostic. Loading never rewrites input or substitutes a default. |
| A4 — Validation | Missing/unknown/duplicate members, wrong numeric types, overflow/limit cases, mismatched cell counts, bad/duplicate IDs, missing spawn layer and out-of-bounds coordinates reject with origin and field/offset. Boundary-valid values pass. The previous owned snapshot stays equal to its saved comparison copy. |
| A5 — Failed writes | Seed a real file with valid fixture A and save distinct valid B. Exercise invalid candidate, encoding/size rejection, staging-create failure, partial write then failure, close failure (also flush failure if buffered), and replacement failure. For each, check the primary category/phase, byte-for-byte equality with A, and successful reload of A. Include an actual OS replacement error against an existing valid file; mocked success/failure alone is insufficient. |
| A6 — Successful replacement | Saving B succeeds, then a new load reads B with no mixture of A/B. Repeat with an absent target; precommit failures leave it absent. Temp-name collisions never overwrite another file; cleanup touches only the operation's own staging file. Cleanup failure retains the primary diagnostic and does not damage A. |
| A7 — CPU isolation | Codec and CLI target dependency/source inspection plus actual headless execution show no Engine startup, graphics/platform module, provider, global asset manager, renderer, active game or sibling branch requirement. An Engine-linked stub or mocked CLI is insufficient evidence. |

Material open points are filesystem behavior beyond this tested local profile, future
Windows/durability support, additional toolchain acceptance, and future
tilemap/asset binding and level adoption contracts. The API and budgets are
deliberately private so findings can revise them before public callers accumulate.

U1's accepted verification was the standalone Debug build and 86 executed codec
cases. U2 adds the native adapter/file cases and source/link review, with actual
counts/commands in [next.md](next.md). U3 adds a private CLI; its actual checks and
limits are recorded there. Successful checks and these local checkpoints do not establish the
full map-storage feature's acceptance.
