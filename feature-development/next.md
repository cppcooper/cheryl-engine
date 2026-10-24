# WORLD-U4 independent bounded document history

Status: implemented and verified, ready for coordinator source review. Exact clean
starting parent on feat/tilemap is `b6658f24df2739adce8d672cabb9407f8b28f40b`.
Its committed U3 ready/true receipt, nonempty saved native logs and 104 passing XML
entries were inspected before dependent work. No geography import or protected /
sibling edit occurs. This is the second map milestone of feature-round-20261024-0500;
the controller alone selects a later exact assignment after this run drains.
The [receipt](queue/feature-round-20261024-0500/WORLD-U4.json) reports executed
behavior, not automatic coordinator/graphical/platform acceptance.

WINT-011/012 match in [compact](decisions.md) and [detailed](agent-notes.md) views.
Prior archives, provenance and receipts are unchanged. Source checks preserve 375
public Engine/production/replay/archive/unchanged-suite paths; the old unit consumer
source remains an exact prefix, preserving every prior body. All 104 registered
predecessor names remain selected. Standalone unchanged storage/procedural suites
are unaffected and not repeated.

## Small real caller

```cpp
#include "tilemap-editor.h"
DemoTilemapEditor editor(existing_directory); // default history: 32 commands / 256 KiB per map/mode
// press()/paint() construct actual admitted records in world-consumer/main.cpp.
const std::array literal{press(Key::Digit6), paint(72,144), press(Key::F6),
                         press(Key::F7), press(Key::F8), press(Key::F9)};
editor.consume(literal, {{400,200},{800,600}});
const std::array semantic{press(Key::B), press(Key::G), paint(40,128),
                          press(Key::T), paint(72,144), press(Key::F6),
                          press(Key::F7), press(Key::F8), press(Key::F9)};
editor.consume(semantic, {{400,200},{800,600}});
const auto available = editor.history(0, DemoTilemapEditor::Mode::Terrain);
editor.write(frame, bindings, style);
```

| Action | Exact private behavior |
| --- | --- |
| F6 / F7 | Undo / redo selected map+mode document; empty history throws owned Refusal::History |
| history(index,mode) | Owned undo/redo counts and retained canonical byte charge |
| Literal paint/erase | One changed cell is one command; same-ID/empty erase is no-op |
| Semantic input batch | One completed changing run is one command, using Engine last-write-wins final semantics |
| New accepted change | Discard redo, then oldest-first eviction to fit; no current-command undo loss |
| No-op / refused change | Preserve both stacks, byte count and current owner; semantic requests still validate fully |
| F8 / failed SAVE | Preserve history and authored runtime edits; checked file publication is separate |
| Successful F9 | Adopt complete owner and clear history ONLY for the selected map/mode, even if content is identical |
| Failed F9 | Preserve current content and both stacks at the file-action boundary |

All hotkeys act only on admitted keyboard Press. Release/Repeat/unadmitted and
invalid/minimized sizes remain ignored. The existing keyboard barrier flushes an
earlier semantic run before undo/redo/file actions. Each pointer uses its own
captured event position; framebuffer scales, Y conversion, computed edges and
pan/zoom stay unchanged. Capture still uses the established Events lease and
TickContext sizes. Camera/selection/brush/mode are NOT part of document history.
The same helpers compile into actual demo, unit cases and world process caller.

## Ownership, budgets and atomic boundaries

Private DocumentHistory in world-document.* owns full before/after Snapshots, not
asset IDs, selected visuals, pointers into maps or event spans. A cursor represents
both stacks in one bounded timeline. There are four independent timelines for
west/east x literal/terrain; both named layers belong to their document timeline.
ID, extent, ordered layer names/spawns and every cell participate in restoration.
Semantic keys distinguish labels that render identically. Existing explicit tables
remain labels 1..4, meaning IDs 101/203/307/409, visual IDs 7..10, atlas targets 0..3;
null alone is empty. No reverse color lookup reconstructs meaning.

The current document epoch owns immutable copied tables/rule/targets. Every
replacement copies them forward; history snapshots need no borrowed reconstruction
inputs. LOAD resets that epoch's timeline. No configuration mutation/replay across
epochs is exposed. Undo/redo reconstructs and validates BOTH nonmovable TerrainMap
owners before publishing a replacement. Literal edits also stage a whole owner.
A candidate timeline and full before/after data exist before content/history swap;
unique pointer and timeline swaps are nonallocating/noexcept. Serialized update /
preparation excludes concurrent observation/recycling. This is a strong refusal
boundary under that ownership model, not a hardware atomic or whole-tick promise.

HistoryLimits default to32 commands/256 KiB combined undo+redo CANONICAL bytes per
map/mode, with constructor maxima256 commands/8 MiB. Zero caps admit no changing
command but allow valid no-ops. Charge per entry is exactly the unchanged encoder's
encode(before).size()+encode(after).size(); full cell/key/metadata/escaping/spawn
representation is counted, with checked size_t addition/accumulation. Adjacent
entries intentionally repeat their shared snapshot. A single unrepresentable or
over-budget command refuses BEFORE mutation or redo invalidation/eviction. Otherwise
only the prepared candidate discards redo and evicts oldest undo entries until it
fits. Newest accepted command always keeps its complete undo contract. Undo/redo
moves only the candidate cursor, so retained entry/byte totals stay constant.

Logical content is bounded, not exact allocator/RSS usage. Object/vector capacity
overhead, current owners/configuration, transient canonical strings and old/new
owner/timeline copies coexist. Fixed geometry is 3x2/two layers; public storage
still caps encoding/input at 8 MiB, cells 262144, layers 16, spawns 256 and keys 128 bytes;
bridge admission remains 65536 cells. Actual editor uses four targets/16 cases and
Engine run caps 32 requests/6 affected/256 work. Maximum configured retained charge
is 32 MiB across four histories, default 1 MiB; baseline/overhead is additional.
No huge allocation/OOM probe. Allocation/programming exceptions propagate, while
all preparation precedes live transitions. Synchronous copies/encoding/derivation
and file I/O may stall simulation; no async service or compact history is claimed.

Current content must match the exact history witness before undo/redo or another
changing command. A negative-only const-facade injection proves mismatch/reverse-ID
refusal without changing content or stacks; such alias writes remain outside the
ordinary read-only/serialized caller contract. Changing edits, undo/redo and LOAD
invalidate borrowed owner/map/terrain references. No-ops preserve those references;
owned snapshots and prepared frames/resources remain independent.

Actual DemoTilemaps::update logs history errors distinctly and drops remaining
current-tick records; a later update continues. Earlier completed commands stay
applied, including the keyboard-boundary terrain batch. Runtime transitions and
checked rename file publication remain separate. Controlled existing directory,
single writer/stable cwd, no-fsync/power-loss/hostile-race/multiwriter limitations
are unchanged. Mode-specific filenames/kind checks and semantic table policies
remain as in U3. No history data/schema is saved: fresh processes start empty.

## Executed evidence

One accumulated Debug/shared ASan/UBSan invocation passes **112/112**, zero failed
or skipped, 54.58s, retaining ALL 104 prior names and adding seven unit cases plus
world.document_history. No check failed or reran; both builds are warning-free.

- history_literal: complete custom metadata/cells after paint/erase/undo/redo,
  no-op redo preservation, layers, map/mode isolation and accepted branch invalidation.
- history_terrain: repeated-address multi-request commands, cancelling/no-op runs,
  multiple layers, aliased labels and independent full cardinal recomputation after
  each transition; literal content stays independent.
- history_limits: exact/one-short entry and canonical-byte caps for both modes,
  single-command refusal preserving redo, oldest-first entry/byte eviction, zero
  cap/no-op behavior and oversized limits rejected without allocation probes.
- history_files: successful SAVE preserves history; failed LOAD preserves content /
  stacks; successful LOAD clears only selected map/mode; failed SAVE retains the
  predecessor bytes, current edit and undo. Sibling snapshots/histories survive.
- history_refusals: late Engine batch failure before publication preserves redo /
  full semantic/visual content; undo/redo witness and reverse-conversion failures
  preserve both stacks. Actual demo update rejects empty redo after earlier valid
  paint, drops following records and permits a later save of that authored edit.
- history_filters: admitted Press-only undo/redo, minimized suspension, unchanged
  selection/view/brush, no-op history, and destroyed temporary event records.
- history_frames: complete semantic undo/redo rederives visuals; copied metadata,
  old/new packets/matrices/ranges/resources remain independent through owner
  destruction and release only with the final frame/vector. Synthetic native
  bind/draw/upload counters stay zero.
- world.document_history: actual caller edits/undoes/redoes/saves/reloads both
  literal and semantic documents; west semantic persists UNDONE content, east
  persists REDONE content. Fresh verifier independently compares all full metadata,
  24 literal numeric cells, 24 labels, 24 meaning IDs and 24 cardinal-derived visuals.
  All four file byte sequences remain unchanged during verification, with no
  history inherited by the fresh process.

All six world scenarios execute 12 child invocations, all exit 0. Exact argv,
stdout/stderr/exits and owned work paths are indexed/copied under
build/world-u4-logs/world-processes.json. The two new history children are separately
saved as document_history-history-author.log and document_history-fresh-history-verify.log.
U3's accepted source/receipt/logs remain preserved, including its separate 104-pass run.

## Commands, logs and ownership

Every configure/build/native/format/symbol command uses scripts/build, one job,
existing clone-local incremental Debug outputs and adaptive/no-swap/admission
limits. No resource refusal, unchanged retry, new configuration/dependency/pin,
install/download, protected/sibling/cache work or display execution occurred.
Commands/environment/immediate exits are in build/world-u4-logs/commands.jsonl.

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer map_storage_consumer map_storage_header_probes tests-world-consumer --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug --target demo --parallel 1
# Saves exact ctest --show-only=json-v1 output and checks all 104 prior names.
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u4-logs/discover.py
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --verbose --parallel 1 \
  --output-junit /mnt/projects/cheryl-features/projects/tilemap/build/world-u4-logs/results.xml \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.|map_storage\.consumer\.|world\.)'
/mnt/projects/cheryl-features/scripts/build -- python3 build/world-u4-logs/check-ownership.py
```

| Operation | Immediate exit / observed result |
| --- | --- |
| Shared build |0; actual runner/Engine consumers/header probes compile/link, no warning |
| Demo build |0; actual private history/editor/update wiring compiles and links, no warning |
| Discovery |0; 112 unique entries, all 104 predecessor names plus eight new |
| Accumulated execution ONCE |0; 112 passed, 0 failed/skipped, 54.58s |
| Ownership |0; same private adapter/editor in three callers, actual one shared Engine |
| Format and dry-run |0 each; touched C++ conforms, no post-test code change |

Saved compile/source/symbol/loader checks establish the SAME world-document.cpp
and tilemap-editor.cpp in demo/unit/world, including DocumentHistory operation
symbols. All 14 previous Engine export/import operations remain, one actual storage
producer per source and one clone-local Engine DSO per binary. World link retains
only actual generator-specific cave objects in its private archive, no alternate
Engine implementation archive or macro workaround. Existing Engine public header
probes remain normal usage. Full logs/XML/LastTest, registration correspondence,
source hashes/commands/exits and child diagnostics live under ignored
build/world-u4-logs/. Scratch files and prior outputs are preserved locally.

ASan/UBSan/halt_on_error remain enabled with no diagnostics. Only command-local
detect_leaks=0 accommodates the recorded tracing issue; no global sanitizer/build
selection changes. Standalone unchanged storage/procedural suites are not rerun.

## Limits and next action

No unresolved U4 CPU/build prerequisite remains. Real display/input, atlas decode /
upload, capture, graphical teardown, visual correctness/fun and leaks remain
unverified. No unchanged X11 :1 retry. Native durability/race/multiwriter,
synchronous allocation/latency and GNU/Linux Debug/shared source-tree vs installed
SDK/static/Release/other-platform/toolchain ABI limits remain. No history disk
format, command bus, undo service, variable geometry or sibling import is included.

Next action: coordinator reviews this exact checkpoint, passing U4 receipt and
saved evidence after the run drains; the finite controller alone may select the
next exact task through its gates. Worker2 remains idle. Publisher owns assigned
branch transport; no later unit, background scheduling or automatic integration
starts here.
