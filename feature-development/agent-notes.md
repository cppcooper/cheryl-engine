# First map-storage milestone

This is a design checkpoint, dated 2026-10-22, for a future CPU-only command-line
round trip. No storage implementation or executable acceptance is supplied here.
The reader summary is [decisions.md](decisions.md); the single next implementation
unit is [next.md](next.md).

The inspected baseline is `3542f883391d4225329dab8d5d61a5987e69c8f0` on
`feat/map-storage`, matching the workspace's recorded source snapshot. The index
and working tree were clean before this task; `feature-development/` did not
exist. Only these three requested internal records belong to this change.

The [workspace instructions](../../../AGENTS.md),
[autonomy policy](../../../coordination/autonomy-policy.md), and
[map-storage brief](../../../coordination/feature-briefs.md#map-storage) set the
scope. Repository workflow, investigation, architecture, documentation, testing,
and Git instructions were read, including `docs/AGENTS.md`. The explicit current
assignment stops at reviewed, locally committed design records. Protected source
workspaces and sibling implementations are not prerequisites or execution sites.

All decisions below are agent proposals under delegated authority. Confidence in
a requirement or observed contract does not establish the user's personal taste.

## MS-001 CPU boundary

Use a private finite-map snapshot, a CPU codec, and an explicit-path file adapter.
The first consumer writes and reconstructs two logical tile layers and a named
spawn, then compares values. It creates no engine Context, game, provider, window,
image cache, or asset loader. Discovery may locate content in a later application;
it does not own persistence, select an output path, or make a write transactional.

Proposed private namespace: `MapFixture`, aliased as `MF` below. These are design
fragments, not existing APIs or compiled examples. `Snapshot` owns strings,
dimensions, ordered layers, optional tile keys, and spawn records; all constituent
types support value equality. The CLI's round-trip body would be:

```cpp
namespace MF = MapFixture;

int round_trip(const std::filesystem::path& destination) {
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
    MF::save(destination, original);       // returns only after replacement
    const auto restored = MF::load(destination); // owns newly decoded values
    if (restored != original)
        return 3;
    std::cout << "map=fixture:two-layers layers=2 spawn=entry round-trip=ok\n";
    return 0;
}
```

The eventual CLI takes an explicit file under a caller-created disposable
directory. `round-trip <file>` uses the sequence above; `verify <file>` only loads
and compares against the literal fixture, including in a fresh process. Its outer
boundary prints operation/path/field diagnostics: exit 0 for success, 2 for a
storage rejection, 3 for a value mismatch, 64 for usage, and 1 for an unexpected
exception. These are proposed consumer outcomes, not observed results.

Extending `FileIndex` or `FileRegistry` with map state would mix discovery and
serialization and would not supply publication guarantees. Extending graphics
`Loader::upload()` would introduce native ownership and its partial effects.
Evidence: E01-E04, E07, H01-H04. **Confidence: high** in the required CPU boundary;
medium in the private API shape. No historical persistence preference is proven.

## MS-002 Logical identity and coordinates

| Value | Proposed identity and meaning |
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
fixture convention, not an agreement with the sibling tilemap branch.

## MS-003 Format and version

Use one UTF-8 JSON object with `format: "cheryl.map-fixture"` and integer
`version: 1`. The format name deliberately scopes the experiment. This is neither
graphics manifest version `"2.0"` nor a universal application/save-game schema.
The file extension `.map.json` is a hint; the header controls interpretation.
No graphics index, `$schema` fetch, filesystem reference, or asset discovery is
needed. Proposed exact fixture:

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
format yields `UnsupportedFormat`; another integer version, including 0 or 2,
yields `UnsupportedVersion`. Missing/wrongly typed headers are invalid data.
There is no inferred version, downgrade, automatic rewrite, or migration in this
milestone. An incompatible field or semantic change needs a new version and a
separately selected reader/migration. Unknown properties in version 1 reject.

Reuse the already declared nlohmann/json dependency privately, with no public JSON
types or new dependency/pin changes. The baseline gitlink is
`b36f4c477c40356a0ae1204b567cca3c2a57d201`; its header is absent in this clone at
this checkpoint. Nothing was retrieved. Availability and parser facilities remain
implementation checks, not demonstrated build readiness.

JSON aids hand inspection and uses an existing authored-input vocabulary. Binary
encoding would add byte-order/layout and inspection work; a new ad hoc text grammar
would add another parser; adopting Tiled or another interchange format would
introduce semantics absent from this consumer. The existing graphics schema
describes atlas assets, not world cells/spawns. None is ruled out for later needs.
Evidence: E03, E05, E08, H02-H03. **Confidence: medium** in a reversible JSON
prototype; low in any inference that the user prefers it for all persistence.

## MS-004 Validation and load publication

Read bounded bytes, parse one complete object, validate a candidate, and return a
new owning `Snapshot` only on complete success. No caller-provided map is mutated.
Input I/O failures report `Read`; byte/depth limits can reject before syntax or
header checks. EOF inside a JSON token/container is malformed/truncated input.
Trailing non-whitespace, comments, invalid encoding, or a second document reject. Syntax
errors precede header interpretation; once syntax is valid, check format/version
before interpreting version-specific content. No default/empty map masks failure.

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
- At most 8 MiB of document bytes, nesting depth at most 16 (root object is depth
  1; each nested array/object adds one), and the key limit from MS-002. Enforce
  byte/depth limits while consuming input, before building an unbounded DOM;
  check collection limits before constructing the snapshot.

The encoder revalidates mutable aggregates, enforces the same limits and the
encoded-byte cap, and finishes all serialization before opening an output staging
file. A semantically valid snapshot whose encoding exceeds the byte cap rejects
without touching the destination. The limits are local operational choices,
not historical preference rules or an infinite-world capacity claim.

Asset existence, atlas bounds, collision, and spawn occupancy are intentionally
outside structural validation. Unknown semantic tile keys are preserved; a future
resolver reports an unresolved binding without silently turning it into empty.
Permissive loading or dropping unknown fields would hide misspellings and lose
future data on rewrite. Strict rejection trades convenience for explicit changes.
Evidence: E03-E06 and the acceptance brief. **Confidence: high** in complete
validation and unchanged caller state; medium in these initial budgets. Duplicate
member detection and bounded parsing need focused implementation coverage; the
current manifest parser does not by itself establish those guarantees.

## MS-005 Ownership and failure reporting

Proposed private operations:

```cpp
Snapshot parse(std::istream& input, const std::filesystem::path& origin);
std::string encode(const Snapshot& snapshot);
Snapshot load(const std::filesystem::path& source);
void save(const std::filesystem::path& destination, const Snapshot& snapshot);
```

Streams and snapshot inputs are borrowed only through the call. Returned snapshots
and encoded bytes own all data; no iterators, JSON views, registry references,
native handles, or callbacks escape. `origin` supplies diagnostic context only.
Paths are explicit; file operations resolve relative paths against the working
directory once at entry. The snapshot stores no absolute path or stream lifetime.
Parsing and encoding perform no filesystem discovery or registry publication.

All operations run synchronously on the calling CPU thread. Independent values
can be used independently, with no singleton or hidden cache. The caller keeps
borrowed streams/snapshots stable, serializes calls affecting the same file, and
settles calls before destroying inputs. No job acceptance/completion split,
asynchronous cancellation, game thread, or shutdown pump is introduced.

Use an owning private `Error` derived from `std::runtime_error` for anticipated
storage failures, carrying a category, operation, origin/destination, field path
or parse byte offset, and an OS error when applicable. Proposed categories are
`Open`, `Read`, `Malformed`, `UnsupportedFormat`, `UnsupportedVersion`,
`InvalidValue`, `LimitExceeded`, `Write`, `Replace`, and `UnsupportedPlatform`.
Messages distinguish malformed bytes from an unsupported version and identify
the failing field. Unexpected allocation errors can propagate normally. Cleanup
must not replace the primary failure or throw from a destructor.

Loads have no effects on existing snapshots. Failed saves preserve the input
value and, under MS-006's supported file profile, the prior destination bytes.
An absent input file is an explicit open failure, not a request to create defaults.
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

## MS-006 Failed-write preservation

Define the commit point as successful replacement of the destination directory
entry. The first executable profile is a single writer to a regular local Linux
file, or an absent file, in an existing caller-owned directory. Reject a symlink or
nonregular destination. Assume the caller controls the directory and does not
concurrently change paths. A future platform adapter must establish equivalent
replacement behavior; unsupported platforms reject before touching the target.

The proposed file adapter must:

1. Resolve the destination once, validate the snapshot, and encode the complete
   bounded document. Check the parent/target profile without changing either.
2. Exclusively create a uniquely named staging file in the same directory, retaining
   its handle and ownership. A Linux exclusive-create facility such as `mkstemp`
   is the initial mechanism; never open a guessed existing temporary path for
   truncation. A private staging file may become the new file with mode 0600;
   preserving old ACLs/permissions is outside this first consumer.
3. Write every byte, handle short writes, and check write and close errors,
   including flush errors if buffered output is used.
   Close the staging file successfully before publication. Every preceding error
   discards only this operation's staging file, best effort.
4. Perform one same-directory native replacement (`rename` for the initial Linux
   profile). Never delete/truncate the old target or use remove-then-rename as a
   fallback. Replacement failure must retain the old valid file; exercise this on
   the supported filesystem rather than inferring it from mocked operations.
5. After replacement, return success without a later fallible validation, logging,
   or reopen step that could report an unchanged-target failure after commitment.

Before the commit point, failure leaves an existing valid file byte-for-byte
unchanged, or leaves an absent target absent. Report the primary phase/error;
on cleanup failure retain that failure and identify the owned staging path when
possible. Do not sweep other temporary files or automatically retry. After a
successful call, a new load reads the new complete document. A later CLI comparison
failure is a separate read/verification outcome, not a claim that save rolled back.

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
sequence; medium in the initial platform choice, unverified by execution.

## MS-007 Integration and implementation placement

Keep the first implementation private under `projects/tests/map-storage/` with
namespace `MapFixture`. Its CPU targets should need only the standard library
and the repository-declared JSON headers, without linking Engine or a native
module. The current Engine target has additional dependency edges (E08), so an
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
and deferred lifecycle policy; medium in this private placement. Final public
names, integration ownership, and save-game composition remain unselected.

## Source evidence

Current-source references E01-E09 describe inspection at
`3542f883391d4225329dab8d5d61a5987e69c8f0`. They are compatibility facts, not
evidence of personal preference, even where Git lists the established author
identity. No tests cited here were executed.

| ID | Exact source locations and observed contract |
| --- | --- |
| E01 | [FileIndex declaration](../projects/engine/include/cheryl/core/resources/fileio/file-mgr.h), lines 17-38, 43-64; [implementation](../projects/engine/src/core/resources/fileio/file-mgr.cpp), lines 13-45. Constructor scans recursively; visited roots do not refresh; lowercased dotted-extension buckets borrow index storage. No content I/O or writer. Errors can leave a partial index. |
| E02 | [FileRegistry declaration](../projects/engine/include/cheryl/core/resources/asset-management/file-registry.h), lines 16-48; [implementation](../projects/engine/src/core/resources/asset-management/file-registry.cpp), lines 7-58. Normalized absolute paths grouped by basename, deduplicated and sorted; owned query results; exact-path lookup separate from first-basename selection. No content open or persistence. |
| E03 | [Reader declaration](../projects/engine/include/cheryl/core/resources/asset-management/manifest-loader.h), lines 16-39; [index reader](../projects/engine/src/core/resources/asset-management/graphics-manifest-loader.cpp), lines 33-57, 60-107, 114-145; [parser](../projects/engine/src/core/resources/asset-management/manifest-loader.cpp), lines 31-70, 97-124, 700-720, 920-933. Explicit selected indexes, graphics-relative exact registered references, version `"2.0"`, owned CPU definitions, field/type checks and exceptions. Unlisted JSON is not parsed. There is no map encoder or replacement protocol. |
| E04 | [Loader declaration](../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h), lines 26-41, 81-115, 149-200; [implementation](../projects/engine/src/core/resources/asset-management/asset-loader.cpp), lines 27-42, 163-208, 368-417. Fresh discovery/private registry, CPU parsing/source snapshots/PNG decode, then provider-verified registry and shader/image/grid publication. Earlier cache effects survive upload failure; retained definition snapshots publish only at the end. `register_files()` publishes unmanaged paths without decoding/parsing. |
| E05 | [Tileset definition](../projects/engine/include/cheryl/assets/definitions/tileset.h), lines 35-55, 87-101; [ID implementation](../projects/engine/src/assets/definitions/tileset.cpp), lines 4-14; [manager](../projects/engine/src/core/resources/asset-management/tileset-mgr.cpp), lines 15-60. Definition IDs are `namespace:name`; cells belong to a grid; Wang hashes are lookup helpers. Manager resolves uploaded textures/materials, builds geometry and publishes absent cache IDs, so it is not a CPU map loader. |
| E06 | [CPU selector](../projects/engine/include/cheryl/assets/selection/tile-selection.h), lines 18-43, 55-79; [runtime tileset](../projects/engine/include/cheryl/assets/types/2d/tileset.h), lines 19-24, 63-98. Application owns world coordinates/sampler and time; selector returns a source-cell result or an ordinary unresolved result. Runtime tile values retain uploaded resources. These supply neither world storage nor persistent instance IDs. |
| E07 | [First-game consumer](../projects/apps/first-game/src/first-game.cpp), lines 35-56; [tutorial consumer](../projects/apps/tutorial-game/src/game.cpp), lines 37-54; [asset demo](../projects/apps/demo/src/asset-demo.cpp), lines 73-100; [audio demo](../projects/apps/demo/src/audio-demo.cpp), lines 32-44. Games upload then retrieve named assets; demo can parse an explicit manifest, then load resources. Audio opens/streams a discovered path separately. Discovery, parsing, and native realization have different owners. |
| E08 | [Engine build declarations](../projects/engine/CMakeLists.txt), lines 21-45; [.gitmodules](../.gitmodules), lines 28-30; baseline tree gitlink `extern/json` at `b36f4c477c40356a0ae1204b567cca3c2a57d201`. Engine uses C++23 and private JSON includes, with other dependency links. The JSON header was checked and is absent. This is configuration inspection only; no configure, retrieval or build ran. |
| E09 | [Status](../docs/STATUS.md), lines 100-103; [manifest metadata gaps](../docs/planning/long-term/asset-manifest-metadata.md); [discovery guide](../docs/site/assets/file-and-font-discovery.md), lines 16-62. Documented intent distinguishes save serialization from manifest parsing and defers world storage. Missing semantic artwork metadata must not be invented. The current task selects this narrow experiment without reconciling published roadmaps. |

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

These are proposed future units, not authorization to start them in this task.

1. **U1 — CPU value and codec.** Implement private snapshot types, equality,
   validation, bounded JSON parse/encode, and focused memory-stream checks under
   `projects/tests/map-storage/`. Use only the declared JSON dependency and the
   standard library. [next.md](next.md) owns its detailed scope and checks.
2. **U2 — File publication adapter.** Add explicit-path load/save, same-directory
   exclusive staging and Linux replacement, with a narrow private file-operations
   seam for deterministic failures. Check real filesystem publication and target
   preservation as well as injected faults. Do not expand to general save slots,
   recovery scanning, background services or cross-platform guarantees.
3. **U3 — CLI acceptance.** Add the literal two-layer fixture and round-trip/verify
   commands, compare every reconstructed field in a fresh process, exercise the
   rejection and preservation matrix below, then review the private API for
   extraction. No rendering, game startup, sibling merge, or artwork is needed.

Each unit keeps the decision summary and next action consistent with actual
evidence. Future executable work must use its selected assignment's resource
authority and the shared build lock; this preparation performs no builds,
dependency retrieval, installs, native tests, or documentation generation.

## Observable acceptance

Every row is **proposed and unexecuted**. Source inspection and a record review
cannot complete these executable requirements.

| Check | Required observation |
| --- | --- |
| A1 — Content | `round-trip` exits 0 and reports the map, two layers, and `entry`. Compare ID, 3x2 dimensions, both ordered IDs, all twelve cell values including empties, and spawn name/layer/(1,1). The asymmetric fixture distinguishes transposition and layer swaps. |
| A2 — Fresh read | A separate `verify` invocation opens the produced bytes and compares to the literal expected snapshot. A second independent snapshot remains unchanged when the first is edited/destroyed. No hidden cache can supply the result. |
| A3 — Format failures | Empty input, invalid JSON, truncation inside a token/container and trailing garbage produce a malformed diagnostic. A valid object with another format or integer version 0/2 produces the corresponding unsupported diagnostic. Loading never rewrites input or substitutes a default. |
| A4 — Validation | Missing/unknown/duplicate members, wrong numeric types, overflow/limit cases, mismatched cell counts, bad/duplicate IDs, missing spawn layer and out-of-bounds coordinates reject with origin and field/offset. Boundary-valid values pass. The previous owned snapshot stays equal to its saved comparison copy. |
| A5 — Failed writes | Seed a real file with valid fixture A and save distinct valid B. Exercise invalid candidate, encoding/size rejection, staging-create failure, partial write then failure, close failure (also flush failure if buffered), and replacement failure. For each, check the primary category/phase, byte-for-byte equality with A, and successful reload of A. Include an actual OS replacement error against an existing valid file; mocked success/failure alone is insufficient. |
| A6 — Successful replacement | Saving B succeeds, then a new load reads B with no mixture of A/B. Repeat with an absent target; precommit failures leave it absent. Temp-name collisions never overwrite another file; cleanup touches only the operation's own staging file. Cleanup failure retains the primary diagnostic and does not damage A. |
| A7 — CPU isolation | Codec and CLI target dependency/source inspection plus actual headless execution show no Engine startup, graphics/platform module, provider, global asset manager, renderer, active game or sibling branch requirement. An Engine-linked stub or mocked CLI is insufficient evidence. |

Material open points are the JSON header/toolchain prerequisites, duplicate-key
and bounded-input parser behavior at the declared pin, local filesystem
replacement/error semantics, future Windows/durability support, and future
tilemap/asset binding and level adoption contracts. The API and budgets are
deliberately private so findings can revise them before public callers accumulate.

The preparation's verification is limited to source/history inspection, static
caller/contract review, and checks of these three records and their Git diff.
No CLI exists yet and no acceptance row has passed by execution.
