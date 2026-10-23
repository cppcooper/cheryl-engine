# U2 checkpoint and next unit

TILEMAP-U2 starts at coordinator-accepted U1 commit
`dec1ec55b1c8d55cfed22ffcf2003238a9878a09`, with a clean index/tree. It implements
retained logical bindings and ordered CPU packet vectors only. The 15 U2 cases
and all 15 U1 regressions passed together (30/30, no skips) in the preserved
Debug ASan/UBSan configuration. Leak detection remains disabled only for the
previously diagnosed environment limitation. There is no unresolved U2 build or
assertion failure. Pass insertion and graphical acceptance remain pending.

## Implemented boundary

- [Submission header](../projects/engine/include/cheryl/assets/submission/tilemap2d.h):
  `Rendering::Submission::{TilemapBindings,TilemapView,resolve_tilemap_layer}`.
  The small first-header caller is in agent-notes and `tilemap_submission.caller`.
- [Implementation](../projects/engine/src/assets/submission/tilemap2d.cpp): checked
  source metadata on insertion, original-target animation once per emitted cell,
  pivot-aware placement and one function-local owned result vector.
- [Synthetic cases](../projects/engine/tests/unit/src/tilemap-submission.cpp):
  15 cases, no asset files, GPU/native work, pass writer or live runtime.
- U1's [CPU header](../projects/engine/include/cheryl/maps/tilemap.h), implementation
  and 15 cases are unchanged. No build configuration, dependency pins or existing
  submission/frame/render/backend contracts change.

Matching detailed/reader decisions are TM-002/003/004/005/006/008 in
[agent-notes.md](agent-notes.md) and [decisions.md](decisions.md). The future
picking proposal now requires agreement with computed grid edges; bare floor
at decimal boundaries is explicitly insufficient. No picking API is added.

## Contracts, assumptions and reversal cost

Logical 7 bound to original atlas target 0 draws; logical 0 emits nothing.
Binding setup rejects zero/duplicate IDs, null assets/images, empty/overflowing
source grids, zero frames, nonnormalized/nonfinite pivots, image-bound violations,
invalid targets and every malformed animation timeline/frame. Validation happens
before insertion and preserves earlier entries. Named views/autotile metadata are
not consumed or validated. Requiring an atlas image is a deliberate restriction
of this adapter; existing manual sampler insertion still works with that image
retained. Geometry/material/range/parameter compatibility is checked by existing
`resolve_tile` only when a nonempty visible cell uses the binding.

View origin is a double top-left world point, X-right/Y-up. Culling stays in
map-local X-right/Y-down coordinates and delegates to U1's computed-edge helper.
Source pivots and independent X/Y source-to-cell scales support non-square
frames. The base model must compare equal to identity and style scale to one;
rotation, negative sizing, arbitrary base transforms and overhanging cells are
unsupported. Origins, cell sizes and whole-map far edges must remain finite floats,
even for an empty visible range; emitted scales must remain positive after float
conversion. Source-corner results and pass matrices are checked for finiteness.
Float precision at extreme magnitudes and clip-space results are not guaranteed.
An uploaded buffer is assumed to match its source frame/pivot metadata; Buffer2D
cannot inspect a producer's uploaded vertices.

Only visible nonzero IDs require bindings; missing bindings throw `bad_request`
with logical ID, layer and `(column,row)`. Invalid layers, including a moved-from
map, also reject safely. Global view/transform checks still run for empty output.
Traversal is increasing row then column. Every packet retains default
`authored_order == 0`; a future pass writer assigns insertion order. Animation
uses the original target once at caller-owned nonnegative milliseconds. Independent
times, edits and erases leave shared assets, another map and earlier packets intact.
Packets retain resources and copy matrices independently of map/binding/metadata
lifetime. Failure after an earlier validated packet destroys only the local result;
no partial vector is exposed and no whole-frame transaction is promised.

These are engineering choices with high compatibility confidence from source and
synthetic checks; exact spelling, glm::dvec2 origin, image requirement and palette
ergonomics have low user-taste confidence. H1 supplies only weak caller-setup
intent; H3 is legacy-associated mutable selection, not a preference to copy.
Reversal is local to the new adapter/cases until a real consumer exists. Changing
exposed STL/GLM layout requires recompilation; CPU maps and saved formats do not
depend on that layout. No stable cross-toolchain ABI is promised.

## Actual verification

All configure/build/native commands run through
`/mnt/projects/cheryl-features/scripts/build --` with one job and the existing
shared lock. The wrapper enforces the unchanged memory/admission policy: aggregate
4 GiB, 3 GiB throttling, 2 GiB per worker, no swap and at least 4 GiB host
MemAvailable. The initial build queued behind the normal lock and its original
tool session was kept until completion. No exit-75 refusal occurred. A future
refusal must be recorded exactly and checkpointed idle without unchanged retries.

Reused `build/tilemap-u1-debug` without replacing outputs/caches. Its CMake cache
retains Debug, tests ON, policy minimum 3.5, shared Engine ON, WARN ON, all previously
assigned optional native/render/runtime/app/docs/test families OFF and
CHERYL_STARTUP_OPENGL OFF. Existing source/test globs caused an automatic CMake
regeneration on the first incremental build; no build-file edit was necessary.
The first configure/build prerequisites were already prepared by the coordinator;
U2 performed no retrieval, font substitution or pin change. Source/test compile
database entries confirm `-g -Wall -fsanitize=address,undefined` with GNU 16.2.1.

Commands, from this clone:

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug --target tests-engine --parallel 1
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug -N \
  -R '^tests-engine\.(tilemap|tilemap_submission)\.'
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error -R '^tests-engine\.(tilemap|tilemap_submission)\.'
```

The first build exited 0. Discovery exited 0 and selected exactly 30 cases:
`tests-engine.tilemap_submission.*` numbered 200–214 and all existing
`tests-engine.tilemap.*` numbered 215–229. Source suite names match registration.
The first run exited 8: all 15 U1 regressions and three U2 cases passed, while
12 U2 cases reported `Required parameter is missing: weight`. The synthetic
pipeline had accidentally inherited `Definition::required == true`; the fixture's
custom weight is optional. Correcting that one fixture flag preserved production
parameter validation. The same build command then exited 0, and repeating the
same nonempty test selection passed 30/30, no skips, in 26.06 seconds (exit 0).
Final review then isolated the negative origin/axis fixtures: each origin probe
starts with a valid other axis, and the synthetic image now fits an ordinary
second grid row so only the intended invalid Y origin/spacing causes rejection.
The out-of-image frame case was adjusted to that image size. Rebuilding and
rerunning the same selection again passed 30/30 in 25.88 seconds (both exit 0).
No address/undefined sanitizer diagnostic appears in the final full test log.

Complete clone-local logs, in the build directory:

- `u2-build.log`, `u2-registration.log`, `u2-ctest.log` (initial fixture failure).
- `u2-build-fixture-fix.log`, `u2-ctest-final.log` and final
  `Testing/Temporary/LastTest.log` (successful corrected run).
- `u2-build-final.log` and `u2-ctest-isolated.log` record the final isolated-fixture
  rebuild/run; `Testing/Temporary/LastTest.log` now contains this last successful run.
- `u2-caller.cpp` and `u2-caller-check.log` for the separate small header-first
  caller check below, using only public Engine and GLM include roots.

```sh
/mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -Iprojects/engine/include -Iprojects/engine/include/cheryl -Iextern/glm -Iextern/glm/glm \
  -fsyntax-only build/tilemap-u1-debug/u2-caller.cpp
```

The caller compile check exited 0 with no diagnostics. It returns the vector
from the small sequence in agent-notes; it does not claim standalone linking or
SDK installation acceptance. The new files pass clang-format's dry-run/Werror
and Git whitespace checks. The U2 build emitted no new source warnings; CMake's
optional `Could NOT find libdwarf` diagnostic did not prevent compilation/testing.
No unrelated source was changed to alter that optional diagnostic.

U1's earlier 15/15 Debug result and exact fresh configuration/compiler commands
remain in committed history at `dec1ec55:feature-development/next.md`, with local
`u1-*` logs preserved. Its initial LeakSanitizer failure was:

```text
LeakSanitizer has encountered a fatal error.
HINT: LeakSanitizer does not work under ptrace (strace, gdb, etc)
```

U2 uses the authorized invocation-local `detect_leaks=0`, while address and
undefined instrumentation stay enabled with halt_on_error. Leak checking remains
unverified; no global sanitizer/build configuration changed.

## Acceptance limits and next action

The executed selection establishes binding rejection/preservation, visible-only
lookup, row/column order, decimal-edge culling, positive non-square pivot placement,
style/context copying, independent clocks/edits, once-only animation, resource
retention and failure without a partial vector. Synthetic bind/draw counters stay
zero. No backend upload or native renderer is present in the cases. Both suites
ran without skips, deliberate OOM probes or a broad test campaign.

Linux GNU shared-source compilation/linkage/use is exercised. Installed SDK,
Windows DLL exports, other compiler/STL/GLM ABIs, Release, native-domain validity,
shader execution, performance, pass order integration and graphical display/edit
feedback remain unverified. The public adapter depends on existing assets,
submission types, STL and GLM; the CPU Map header stays independent of them.

The next proposed bounded action is **U3's pass-consumer step only**, subject to a
later coordinator assignment: insert ground/detail vectors from two map instances
through the existing pass writer in a synthetic consumer. Assert assigned
`authored_order`, caller-controlled layer/instance order and failed-group
preservation without changing the pass API or backend. The proposed demo/editor
consumer remains later work; no tile artwork, controls, picking, persistence,
auto-tiling, resize or chunking is prerequisite to that synthetic step. Stop this
worker after U2's reviewed local commit; no recurring scheduling or U3 work starts.
