# U1 checkpoint and next unit

TILEMAP-U1 implements finite CPU values and visible-cell ranges only, starting
from design checkpoint `2e74a09ccd56f55d6256699ed69668ba6dbd7c00`. The initial
working tree/index were clean. U1's 15 focused native cases pass in the assigned
Debug build under ASan/UBSan, with leak detection disabled as explained below.
The graphical tilemap milestone remains unimplemented. Later units require a new
coordinator assignment, with no user questionnaire or phase approval.

Implemented owners:

- [CPU header](../projects/engine/include/cheryl/maps/tilemap.h): `TileId`,
  signed `LayerIndex`/`Point`, `Extent`, `CellSize`, `LocalRect`, `CellRange`,
  `Map` and `visible_cells`. Its only nonstandard include is Cheryl's export
  decorator; it includes no asset, renderer, runtime or GLM headers.
- [Implementation](../projects/engine/src/maps/tilemap.cpp): checked dense cell
  storage, deep copy/whole-value assignment, noexcept moves leaving a safely
  rejected zero state, and computed-edge binary searches without an epsilon.
- [Focused cases](../projects/engine/tests/unit/src/tilemap.cpp): 15 cases in the
  `tilemap` suite, with the new public header included first and a small caller.

The matching decisions are [TM-001](agent-notes.md#tm-001),
[TM-002](agent-notes.md#tm-002), [TM-003](agent-notes.md#tm-003),
[TM-004](agent-notes.md#tm-004) and [TM-006](agent-notes.md#tm-006).
Integer widths, naming and dense storage are engineering choices, with low
personal-preference confidence. Changing their public layout requires consumer
recompilation; no saved format or downstream renderer depends on it yet.

## Verification commands and results

All commands below run from this clone. Configuration/build/native-test commands
use `/mnt/projects/cheryl-features/scripts/build --`, which holds the shared lock.
The current wrapper permits only one compilation/test job and requires verified
memory limits and at least 4 GiB MemAvailable. An admission failure (exit 75)
means record its exact error and checkpoint idle without unchanged retries.
The build directory is clone-local, disposable and ignored by Git. No protected
checkout/cache, device, GUI, documentation generation or broad test campaign is used.

The first configure stopped at `projects/engine/cmake/FontFallback.cmake:4` with
`file failed to open for reading (No such file or directory)` for this clone's
`assets/fonts/DejaVuSans.ttf`. The coordinator subsequently initialized assets at
the unchanged pinned commit `98453b2a6e0edfcfba22eea3fde801526d72b861`, supplying
the exact file. The same configuration was retried after verifying that changed
prerequisite; no substitute font or build-behavior change was introduced.

Fresh Debug configuration (retried in its original directory after the font fix):

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake -S . -B build/tilemap-u1-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DWARN=ON -DCHERYL_BUILD_TESTS=ON \
  -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_NATIVE_SDL3=OFF \
  -DCHERYL_BUILD_OPENGL=OFF -DCHERYL_BUILD_STARTUP=OFF -DCHERYL_BUILD_WORKERS=OFF \
  -DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF -DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=OFF \
  -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF -DCHERYL_BUILD_UI_TGUI=OFF \
  -DCHERYL_BUILD_UI_RMLUI=OFF -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
  -DCHERYL_BUILD_DOCS=OFF -DCHERYL_BUILD_SDK=OFF -DCHERYL_BUILD_DEMO=OFF \
  -DCHERYL_BUILD_FIRST_GAME=OFF -DCHERYL_BUILD_TUTORIAL_GAME=OFF \
  -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF \
  -DCHERYL_BUILD_CONSUMER_TESTS=OFF -DCHERYL_STARTUP_OPENGL=OFF
```

Configuration succeeded after the pinned-font prerequisite was supplied (exit 0).
The earlier `--parallel 4` build was interrupted by the coordinator's memory-policy
change before producing compilation output; it establishes no build/test result.
Continuation uses the preserved directory and only one job:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug --target tests-engine --parallel 1
```

The single-job build compiled and linked Engine and the unit collection, then
failed during GoogleTest discovery with exit 1:

```text
LeakSanitizer has encountered a fatal error.
HINT: LeakSanitizer does not work under ptrace (strace, gdb, etc)
GoogleTestAddTests.cmake:233: Error running test executable. Result: 1
```

The runtime's hint identifies a tracing limitation; the exact sandbox restriction
was not further diagnosed. A command-local `detect_leaks=0` setting allowed the
incremental build/discovery and selected tests to finish. ASan/UBSan instrumentation
remained enabled, and execution set both sanitizers to halt on errors. Memory
limits, the wrapper, source build behavior and dependency pins were unchanged.
Leak checking is unavailable in this environment and is not claimed as passing.
This reversible invocation-only choice has no source/API or saved-config cost.

Successful build, discovery and execution commands:

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug --target tests-engine --parallel 1
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug -N -R '^tests-engine\.tilemap\.'
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error -R '^tests-engine\.tilemap\.'
```

All three commands exited 0. Discovery selected exactly 15 cases, numbered
200–214, matching all 15 cases in the new source. Execution passed 15/15 with no
skips in 12.88 seconds; its full `Testing/Temporary/LastTest.log` contains no
address/undefined sanitizer diagnostics. These outcomes cover U1 only, not later
tile rendering/edit display. Local logs are `u1-build-serial.log` (initial LSan
failure), `u1-build-leak-disabled.log`, `u1-registration.log` and `u1-ctest.log`
inside the preserved build directory.

Registration comes from the existing `tests/unit/CMakeLists.txt` source
glob into `tests-engine`; `cmake/CherylTests.cmake` adds the `tests-engine.` prefix
through GoogleTest discovery. A source suite count is not an executed selection.
The generated compile database includes both new source files with Debug `-g`,
`-Wall`, and `-fsanitize=address,undefined`; the CPU header's include boundary was
checked directly. Neither observation is a substitute for the native suite run.

The shared unit collection emitted seven pre-existing `-Wdangling-else` warnings
in unchanged asset-catalogue, font-selection, runtime-contract, shader-assets and
math cases. No warning came from the new tilemap implementation or cases. Those
unrelated fixtures were preserved. New files also pass clang-format 22.1.8's
`--dry-run --Werror` check and Git whitespace checks.

Compiler checks already executed successfully with GNU C++ 16.2.1 (exit 0,
no warnings), through the same build wrapper:

```sh
/mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -Iprojects/engine/include -Iprojects/engine/include/cheryl \
  -Iextern/googletest/googletest/include -fsyntax-only \
  projects/engine/src/maps/tilemap.cpp projects/engine/tests/unit/src/tilemap.cpp
/mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -Iprojects/engine/include -x c++ -fsyntax-only -
```

The second command's stdin was the small first-header caller from agent-notes,
inside `void caller()`, explicitly consuming its `tile` and `visible` results.
These are compile checks, with no linking or native-test acceptance inferred.

## U1 acceptance cases

The selected suite covers:

- An asymmetric 3×2 two-layer map's 12 initial empties, arbitrary logical IDs,
  all corners/layers, read/paint/erase and harmless same-value edits.
- Signed point/layer rejection preserving all cells, zero dimensions/layers,
  multiplication overflow and vector storage limits rejected without OOM probes.
- Two independent instances, deep copies, destruction of an original, whole-value
  assignments replacing dimensions, self-assignment, moved-to data and safely
  rejected/reassignable moved-from values.
- All original TM-004 ranges; outside/touching edges and corners, fractions,
  containment, zero area, extreme finite rectangles, invalid/nonfinite inputs,
  nonpositive cell sizes and overflowing footprints.
- Exact computed decimal edges (including indices 43 and 3 at size 0.1), adjacent
  `nextafter` values, and an independent exhaustive positive-area intersection
  oracle using binary, decimal, fractional, subnormal and large finite cell sizes.

The root Debug configuration enables AddressSanitizer and UndefinedBehaviorSanitizer;
the nonempty selected run above passed under that instrumentation. LeakSanitizer
was disabled only for the documented environment limitation. No deliberate OOM
event, artificial allocation exhaustion, native GUI or broad test suite was run.

## Limits and next useful unit

The source is additive. `Map` exposes an STL-backed class layout and exported
methods using existing `CE_TYPE_API`/`CE_API` conventions. First-header compilation
uses only the public include root. Installed SDK packaging, Windows DLL export,
other compilers/standard-library ABIs, Release behavior and stable cross-toolchain
binary compatibility have not been established. The native run does establish
compilation/linkage/use against this Linux GNU 16.2.1 shared Engine source build.
Leak detection remains unverified; use a compatible untraced environment for a
future leak check. There is no unresolved build or U1 assertion failure.

With U1 complete, the next proposed unit is **U2 only**: retained logical-ID
bindings and CPU tilemap-layer packet resolution using existing tile submission.
It requires a later coordinator assignment. Its focused synthetic-resource checks
should establish empty ID versus atlas cell zero, invalid bindings/targets,
pivot placement, authored ordering, resource/snapshot retention, animation applied
once and failure without partial group insertion. U2 must not add demo controls,
persistence, auto-tiling, resizing or chunking. This worker stops after U1's
records and local checkpoint; it does not start U2.
