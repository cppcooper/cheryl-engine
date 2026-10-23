# Map-storage U5 checkpoint and next action

U5 starts clean at accepted U4 `28617d0ab0789fe1309162319bc5e2871555f2e6`.
The existing garden consumer now has an opt-in normal shared-Engine mode;
its default standalone CPU mode is retained. **All three shared-Engine cases
passed**, and all three shared import-mode header probes built. Revised static
CPU probes also built in the preserved U4 configuration. Initial native failures
came from LeakSanitizer's fatal runner diagnostic at process exit; passing checks
use only `ASAN_OPTIONS=detect_leaks=0`, leaving normal Engine instrumentation and
resource limits intact. Leak checking remains unverified.

Only consumer CMake wiring/probe selection and these internal records change.
No production source/header, garden main/harness, private CLI, format/limits,
validation order, native path or publication behavior changed. No export/ABI
implementation defect was exposed. The accepted 136 regressions and prior CPU
native cases were therefore preserved, not repeated. Coordinator review of this
coherent local checkpoint is next; publisher transport and later units remain
with the coordinator.

## Same public-header caller and producer choice

```cpp
#include <cheryl/core/resources/map-storage/file-storage.h>
// Plus <sstream>; authored() is the existing complete garden:north literal.
auto source = authored(); // 2x3, soil/markers, twelve cells/empties, two spawns
try {
    auto bytes = CE::Maps::encode(source);
    std::istringstream input(bytes);
    auto decoded = CE::Maps::parse(input, "memory:garden");
    CE::Maps::save(path, decoded); // explicit path in a controlled directory
    auto restored = CE::Maps::load(path);
    const bool equal = restored == source;
    static_cast<void>(equal);
} catch (const CE::Maps::Error& error) {
    auto owned = error; // code, operation, origin/location, offset, I/O/cleanup
    static_cast<void>(owned);
}
```

`MAP_STORAGE_USE_SHARED_ENGINE=ON` includes the existing CherylModules.cmake and
calls cheryl_prepare_module with module test setup off. Its existing scoped
Engine-only bootstrap disables docs/SDK, startup/workers/native/graphics/UI/audio,
apps and broad runners. The mode rejects static/imported/other-source Engine
producers and an alternate CPU target, then links only Cheryl::Engine. Default
OFF still links Cheryl::MapStorageCPU. No consumer visibility/import definitions,
private includes or implementation source copies were added. MS-005/007/009/010
in [both decision views](decisions.md) distinguish source ownership, current
verification and deferred contexts; [agent notes](agent-notes.md) retain evidence,
alternatives, confidence and reversal costs.

## Actual prerequisite preparation

Normal Engine configuration unconditionally embeds assets/fonts/DejaVuSans.ttf.
This clone's assets were uninitialized. Following the coordinator's documented
read-only object-transport method, preparation ran once from this clone:

```bash
git init --quiet assets
git submodule init -- assets
git -C assets -c protocol.file.allow=always fetch --depth=1 file:///mnt/projects/cheryl-engine/assets/.git 98453b2a6e0edfcfba22eea3fde801526d72b861
git -C assets checkout --quiet --detach 98453b2a6e0edfcfba22eea3fde801526d72b861
git submodule absorbgitdirs -- assets
```

Init/fetch/checkout exited 0. Optional absorption exited 128 with exactly:
`fatal: could not migrate git directory from '/mnt/projects/cheryl-features/projects/map-storage/assets/.git' to '/mnt/projects/cheryl-features/projects/map-storage/.git/modules/assets': Invalid cross-device link`.
No migration retry or permission workaround followed: the clean pinned checkout
keeps an independent nested .git and was immediately usable for Engine configure.
The existing gitlink is unchanged. No remote was added/altered, dirty source
worktree copied or output/cache borrowed. File transport used GIT_NO_LAZY_FETCH=1
and GIT_OPTIONAL_LOCKS=0. There are no alternates, symlinked/shared objects or
metadata references outside the owning clone. Protected asset HEAD/refs/config
hashes match before/after. The fallback hash is
`4a2438171a9144a2c698a36a64b10116c4c9db9112cd12361296f9eb0bd8cbed`.

`build/map-storage-u5-shared-debug/u5-logs/assets-preparation.log` retains exact
argv/output/per-command statuses; `assets-preparation.exit` is the genuine 128
optional-migration outcome. `assets-verification.json` and before/after metadata
hashes establish the usable checkout, rather than treating the preparation
script's nonzero exit as a missing font. Existing ctti/backward/spdlog/glm/stb/JSON
pins and system Freetype/Harfbuzz/ICU were reused; no network retrieval, system
installation, pin change or source worktree modification was needed. Map codec
and path loading perform no runtime asset lookup.

## Actual wrapped configure/build/native commands

Every configure/build/native check ran from this clone through the shared wrapper,
one job. Existing outputs/logs remain intact. The new focused shared configuration
is necessary to exercise the real producer/import boundary unavailable in the
alternate CPU build. GNU C++ 16.2.1, Ninja, Debug and the normal Engine's sanitizer
configuration were used.

```bash
/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/engine/tests/map-storage-consumer -B build/map-storage-u5-shared-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DMAP_STORAGE_BUILD_TESTS=OFF -DMAP_STORAGE_USE_SHARED_ENGINE=ON -DCHERYL_BUILD_SHARED_LIBS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u5-shared-debug --target map_storage_consumer map_storage_header_probes --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u5-shared-debug -R '^map_storage\.consumer\.' --show-only=json-v1 --no-tests=error --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u5-shared-debug -R '^map_storage\.consumer\.' --output-on-failure --no-tests=error --parallel 1
/mnt/projects/cheryl-features/scripts/build -- nm -D --defined-only --demangle build/map-storage-u5-shared-debug/engine/libcheryl-engine.so.4.0.0
ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0" /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u5-shared-debug -R '^map_storage\.consumer\.' --output-on-failure --no-tests=error --parallel 1
/mnt/projects/cheryl-features/scripts/build -- readelf --dyn-syms --wide --demangle build/map-storage-u5-shared-debug/engine/libcheryl-engine.so.4.0.0
/mnt/projects/cheryl-features/scripts/build -- readelf --dynamic --wide build/map-storage-u5-shared-debug/map_storage_consumer
/mnt/projects/cheryl-features/scripts/build -- nm -D --demangle build/map-storage-u5-shared-debug/map_storage_consumer
/mnt/projects/cheryl-features/scripts/build -- ldd build/map-storage-u5-shared-debug/map_storage_consumer
/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/engine/tests/map-storage-consumer -B build/map-storage-u4-consumer-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMAP_STORAGE_BUILD_TESTS=OFF -DMAP_STORAGE_USE_SHARED_ENGINE=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u4-consumer-debug --target map_storage_header_probes --parallel 1
```

Complete stdout/stderr and immediate returned statuses are retained under each
build's `u5-logs/`, using configure/build/discovery, engine-nm/engine-dynsym,
consumer-dynamic/consumer-nm/consumer-ldd, and CPU configure/headers names with
matching `.exit`. Discovery has separate JSON/stderr. All of these exited 0
except the first native CTest (exit 8). Its original all-tests log/exit and
initial-lsan copies remain; the passing rerun is no-lsan-all-tests.log/exit.
No truncated tool output prevents reviewing the full errors. There were no
compiler warnings or wrapper exit 75. Live sessions waiting behind the normal
shared lock were retained to completion, not cancelled/relaunched.

## Observed source/link/export and runtime results

- Configure identifies this clone's normal cengine target, alias Cheryl::Engine,
  SHARED_LIBRARY, source projects/engine, and actual libcheryl-engine.so.4.0.0.
  The producer uses its normal CE_BUILDING_ENGINE and hidden visibility defaults.
  `map-storage-producer.txt`, `compile-inputs.json` and `link-inputs.txt` retain
  evidence. The graph has one Engine compile entry for each map .cpp, no alternate
  CPU archive, no selected backend/module/app/test runner and no consumer source
  copy or private JSON/FileOps/test include. Declared Engine public glm/ctti and
  logging requirements are inherited normally. Necessary neutral Engine/text
  sources and shared dependencies are built; this is the real Engine, not the
  smaller CPU-only dependency graph or an unused Engine-linked stub.
- All three self-contained shared header probes built with imports, CE_ENGINE_STATIC
  and CE_BUILDING_ENGINE absent, and nonempty CE_API on this toolchain. Revised
  CPU probes built with inherited CE_ENGINE_STATIC and empty CE_API, without a
  consumer definition patch. CPU producer metadata remains STATIC_LIBRARY with
  only public Engine includes and its declared static requirement. No CPU native
  cases or the unchanged 136 accepted regressions were repeated for wiring alone.
- Engine nm/readelf show nine dynamic map entries: four GLOBAL/DEFAULT functions,
  two GLOBAL/DEFAULT ABI constructor aliases, and DEFAULT Error typeinfo,
  typeinfo-name and vtable. Error data symbols are weak, as expected for this
  emitted C++ type. Private CE::Maps::Detail does not appear in dynamic exports.
  Consumer nm has undefined load/save/parse/encode references; link/NEEDED/RUNPATH
  and ldd resolve libcheryl-engine.so.4 to this exact clone-build Engine. ASan and
  UBSan runtimes also resolve normally. `map-exports.txt` preserves selected lines;
  complete symbol/dependency dumps remain alongside it.
- Nonempty discovery contains exactly memory, round_trip and preservation.
  Initial execution was 0/3 passing: each first child completed its application
  assertions/output but exited 1 on `LeakSanitizer has encountered a fatal error`,
  with the hint `LeakSanitizer does not work under ptrace (strace, gdb, etc)`.
  These are genuine failed checks, not deliberate rejection fixtures or evidence
  of a detected leak. Restricted-runner incompatibility is the supported inference;
  no source/export defect is indicated. Initial output/LastTest logs are saved.
- Existing ASAN_OPTIONS was unset. The changed-condition rerun disables only
  end-of-process leak detection for this invocation, without changing CMake,
  Engine compilation/visibility, source or resource limits. All 3/3 cases then
  passed, no skips, 0.34 seconds real/summed. Engine retains normal Debug ASan/UBSan
  instrumentation and the consumer links their runtimes via Engine's interface.
  Leak checking is excluded; this is not an all-source sanitizer or broad Engine
  acceptance claim. No permanent sanitizer environment default was added.
- Five successful independent executable invocations cover memory/error,
  author/fresh-verify and preserve/fresh-predecessor. Source values/streams/bytes
  are mutated/destroyed; independent decoded values and copies remain completely
  equal. Engine throws CE::Maps::Error across the DSO; the consumer catches/copies
  it and verifies owned code, parse operation, discarded origin, location/message
  and known byte offset after original inputs disappear. Actual retained output
  reports Malformed / parse / memory:discarded-origin / $[""] / byte 2.
- Fresh verification compares every field, ordered layer, cell/empty and both
  spawns against the same garden literal. Two load results remain independent.
  The harness checks exact bytes around read-only verification. Saving invalid B
  catches InvalidValue / encode / <snapshot> / $.layers[0].cells before staging,
  with no I/O/cleanup/leftover, then compares exact predecessor bytes and full
  reload; a separate process verifies literal A again. No native write fault is
  newly claimed here; accepted U4 tests retain that coverage.
- Exact bytes match U4: garden:north is 509 bytes, SHA-256
  `ce8f5a705167332edf2b1791be94c314a59ba43647874b6d5ec3b37efbfe0308`;
  garden:predecessor is 514 bytes, SHA-256
  `035e663fd38c8115cafe667aff8ba3249e67e9bc12175f051672c4e605e2ea94`.
  `published-files.json` retains actual fixture paths, identities, sizes/hashes.

`consumer-checks/` contains only unique clone-build children, retaining each
command/stdout/stderr/immediate exit. process-summary.json inventories eight
actual invocations: the three initial LeakSanitizer exit-1 failures, then five
successful exit-0 calls. Separate initial-lsan and no-lsan LastTest logs preserve
both outcomes. Public/production files, the garden main and process harness were
also compared unchanged against accepted U4. No implementation/ABI fix or
regression-threshold weakening was necessary.

## Limits and next concrete action

Confidence is high for this Linux/GNU same-toolchain shared execution and exported
boundary; medium for provisional CMake selector/target names, whose reversal
changes only consumer wiring/probes and records. These are engineering choices,
not inferred user taste. Full static Engine, non-Linux/toolchain/CRT ABI,
installed SDK/relocation, leak checking, broader Engine feature acceptance and
GUI/device behavior remain unverified. The existing Linux caller-controlled,
stable-tree, regular-or-absent, single-writer profile, native ancestor-symlink/..
meaning, leaf-link rejection, checked I/O/one-time close, owned cleanup and single
rename commit remain unchanged. No fsync/durability, hostile-race, network-filesystem,
multiwriter, slot/lifecycle or universal save-game guarantee is added.

There is no unresolved U5 implementation/prerequisite/resource blocker. The
optional absorption error leaves usable independent assets; LeakSanitizer's
runner limitation is explicitly excluded from passing evidence. All native work
used the wrapper's adaptive shared memory/worker limits, no swap and 4 GiB host
admission floor, one job. The monitor controls caps/worker count; this worker
changed neither. No subagent/external agent, OOM test, network fetch, system
installation, protected worktree/cache change, pin/remote change, push or sibling
integration occurred. U4's full handoff remains in accepted-u4-next.md and Git.

**Next concrete action:** coordinator reviews this U5 shared-Engine checkpoint
and saved exports/native evidence, including the leak-check limitation. Any full
static Engine, other-platform or installed SDK work needs a later bounded
assignment. Stop here; no later milestone, tutorial or integration is started.
