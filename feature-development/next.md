# U2 session acceptance and next unit

U2 implements the CPU session/value unit on top of preserved checkpoint
`f12adbbf9a3edda5c6ad83115deabc502d296f4b`: retained frozen definition, owned views,
opaque live incarnation, checked explicit choices, synchronous borrowed handlers,
failure state and owned `Snapshot` resume. **18/18 session cases and all 16
definition regressions passed** (34/34, no skips/unrun cases). A standalone
allocation-rejection probe compiled and ran successfully. This accepts the assigned
U2 scope only. Console/JSON, filesystem I/O and runtime integration remain U3.

Both contract views are current: [agent-notes.md](agent-notes.md) and
[decisions.md](decisions.md), especially ST-002/003/004/005/007. The private shared
incarnation renews on every start/resume, survives idle moves and is absent from
snapshots; equal persisted fields cannot resurrect an old token. The tiny gate
caller captures/adopts the flag and cursor on one idle serialized owner, resumes,
then chooses pass with the effect counter still one. This is an engineering
implementation of the coordinator's correction, not evidence of personal API taste.

## Actual U2 checks

All configure/build/native checks used the existing wrapper, one job and
clone-local outputs. The existing Debug configuration/pins were retained. The
incremental build automatically reconfigured for the new source/test globs;
no repository build configuration was edited. ASan/UBSan instrumentation and both
halt-on-error settings remain active, with command-local leak disabling under
ST-008. No resource refusal/exit 75 occurred. Normal shared-lock queueing was
allowed to complete in the existing tool session without cancellation/probes.

| Session case(s), all passed | Observable boundary |
| --- | --- |
| `consumer`, `routes`, `boundaries` | Header-first tiny caller; one flag; both endings; initial/terminal snapshots; paired resume without repeating the effect. |
| `conditions`, `scope`, `missing_handlers` | Recheck availability in both directions after presentation; foreign/stale/unissued/unknown selections and missing handlers reject without effects/progress. |
| `cycles`, `incarnations`, `independent` | Explicit cycle progression invalidates old steps; identical-field resume/rewind and repeated logical IDs get new markers; independent cursors reject foreign commands. |
| `ownership`, `moves`, `handlers` | Retained definition lifetime; view/snapshot ownership after definition/session release; marker survives idle moves; moved-from state rejects; no handler retention. |
| `guard_failure`, `effect_failure`, `reentrant` | Predicate failure preserves healthy progress/no effects; original effect exception rethrows after partial app mutation and faults; callback view/choose/snapshot return Busy. |
| `lifecycle`, `exhaustion`, `resume_rejects` | Active move/replacement/destruction terminate in death tests; maximum step rejects before callbacks; schema/content/revision/session/node failures preserve an existing live value. |

The focused allocation probe is isolated from `tests-engine`; only its own
executable replaces scalar new/delete. It first proves that a one-byte request
is rejected, then simulates owned view/snapshot copy failure with 128-byte strings
and verifies unchanged healthy state. Its effect verifies the key borrows frozen
storage, then enables allocation rejection until `choose` returns. Success,
unchanged rejection count, one effect and destination/step checks passed. The
engine's prepared `expected<void, SessionError>` success is statically nonthrowing
to copy; the source after effect return contains only numeric assignments, return
and the nonthrowing busy-guard reset. No memory exhaustion/OOM was attempted.

Actual U2 commands (from clone root), all exit **0**; complete stdout/stderr and
immediate exit statuses are saved under `builds/storytelling-u1-debug/`:

```sh
/mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -Wall -Wextra -Wpedantic -fsyntax-only \
  -Iprojects/engine/include -Iprojects/engine/include/cheryl -Iextern/googletest/googletest/include \
  projects/engine/src/core/game-framework/dialogue-session.cpp projects/engine/tests/unit/src/dialogue-session.cpp
# Above checked the initial tiny caller before the other 17 session cases were added.

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^tests-engine\.(dialogue_definition|dialogue_session)\.'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 -R '^tests-engine\.(dialogue_definition|dialogue_session)\.'

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -O0 -g -Wall -Wextra -Wpedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Iprojects/engine/include -Iprojects/engine/include/cheryl \
  projects/engine/src/core/game-framework/dialogue.cpp projects/engine/src/core/game-framework/dialogue-session.cpp \
  projects/engine/tests/probes/dialogue-session-allocation.cpp -o builds/storytelling-u1-debug/dialogue-session-allocation-probe
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- builds/storytelling-u1-debug/dialogue-session-allocation-probe
```

| Complete clone-local log / matching `.exit` | Actual result |
| --- | --- |
| `u2-caller-syntax.log` | Exit 0, initial small caller and implementation syntax/type check, no diagnostics. |
| `u2-build-1.log` | Exit 0, automatic glob reconfiguration, Engine/source/test compilation, linking and discovery. |
| `u2-registration.log` | Exit 0, exactly 34 cases: 16 definition and 18 session names, matched to source in order. |
| `u2-ctest-1.log` | Exit 0, `100% tests passed out of 34`, 4.64 seconds; no skips/unrun cases. |
| `u2-ctest-details.log` | Preserved complete `Testing/Temporary/LastTest.log`; all 34 GoogleTest RUN/OK records and CTest passes checked against source. |
| `u2-allocation-build.log` | Exit 0, isolated warning-enabled Debug ASan/UBSan source probe compilation. |
| `u2-allocation-run.log` | Exit 0, allocation rejection and post-effect commit checks passed, no sanitizer diagnostics. |

The four changed/new C++ files passed repository clang-format and whitespace
checks. The 16 definition tests/source remain unchanged; only the public header
adds session declarations and the standard functional include. No broad suite,
SDK export, GUI or documentation generation ran. Full logs remain clone-local
ignored outputs, outside the coherent source/records commit.

## U1/U1V evidence retained

U1 source is committed in `25836ffb48fb25e9ed5e89d2805daf6e093ed5fc`, from design
checkpoint `716ec60984e6764a03ed375ac37826e5d92e6978`. U1V started at clean checkpoint
`85b2280c512cd6638c1e67819a9539d2a44216e5` and completed definition acceptance:
**16/16 focused native cases passed**, with no skipped or unrun cases. Debug
ASan/UBSan instrumentation and halt-on-error remained active. Leak detection is
unavailable under the reproduced LSan tracing limitation (ST-008), not passing.
The font prerequisite remains resolved: the coordinator
initialized assets at the unchanged pin
`98453b2a6e0edfcfba22eea3fde801526d72b861`, and
`assets/fonts/DejaVuSans.ttf` is present. The prescribed configuration now succeeds.
The incremental `tests-engine` target and its discovery now succeed with the
authorized invocation-local environment adjustment. No repository build
configuration, dependency pin, global setting or U1 source changed during U1V.

Contracts: [agent-notes.md](agent-notes.md); compact choices:
[decisions.md](decisions.md), ST-001/002/003/004/006 and verification decision ST-008.
ST-005/007 were proposals at U1V; U2 now implements the value boundary as recorded
above. The U1 header/source implement owned
authored values, private deep-copy freezing, ordered inspection, scoped lookup and
owned diagnostics.
The four-node gate remains test data; no session, callback dispatch or save exists.

## Actual checks and registration

The new test translation unit includes the public header first. It uses the
existing unit `src/*.cpp` glob: `projects/engine/tests/unit/CMakeLists.txt` feeds
`tests-engine-cases` through `cheryl_add_test_suite`, and
`cmake/CherylTests.cmake` calls GoogleTest discovery with prefix `tests-engine.`.
There are 16 `TEST(dialogue_definition, ...)` declarations in the source. The
generated `cmake_test_discovery_fc8c995a1d.json` and
`tests_engine_6c1488ca_tests.cmake` under the unit build directory contain those
16 cases. The actual filtered CTest listing selected exactly those names in source
order, and the subsequent run executed all 16. Per-case GoogleTest `[ RUN ]` /
`[ OK ]` output and CTest `Test Passed.` records were checked against the source.

| Passed case(s) | Observable boundary exercised by the native selection |
| --- | --- |
| `consumer`, `ownership`, `lookup` | Header-first caller; constructor/copy/move restrictions; draft aliases remain separate even through std::move; draft mutation/destruction and retained-handle lifetime; stable text/order/keys; exact lookup and misses. |
| `empty_ids`, `revision`, `duplicate_nodes`, `duplicate_choices`, `missing_references` | Empty definition/entry/node/choice/target, zero revision, scoped duplicates, unknown entry/target and empty node collection reject without a partial definition. |
| `unsupported_keys`, `supported_names` | Unsupported or empty optional keys and empty/duplicate declared names reject in the correct condition/effect domain, with owned index/field/value diagnostics. |
| `node_shape` | Prompt without choices, terminal with choices and unknown kind reject. |
| `independent_domains`, `cycles`, `entry_order` | Reused choice name across nodes, equal operation spelling across domains, explicit cycles, empty text, independent same-definition compilations, and nonfirst/terminal entry preserve their intended semantics. |
| `unreachable`, `diagnostics` | Invalid unreachable row rejects eagerly; multiple diagnostics have deterministic authored order, own location/offending values after input destruction, and expose no partial frozen output. |

Validation has no application-code parameter or stored callable; source review
confirms it only processes these concrete CPU values. A single `Draft` contains
one definition ID, so duplicate-definition checking across compilations is not
representable here and is deliberately not replaced with a global registry.

## Configuration retained

U1V reused the existing clone-local GNU C/C++ 16.2.1 Debug configuration and
incremental objects. The following configure already exited **0** after the
pinned font was restored; it was not rerun during U1V:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCHERYL_BUILD_TESTS=ON \
  -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_NATIVE_SDL3=OFF -DCHERYL_BUILD_OPENGL=OFF \
  -DCHERYL_BUILD_STARTUP=OFF -DCHERYL_BUILD_WORKERS=OFF -DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF \
  -DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=OFF -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
  -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
  -DCHERYL_BUILD_DOCS=OFF -DCHERYL_BUILD_SDK=OFF -DCHERYL_BUILD_DEMO=OFF -DCHERYL_BUILD_FIRST_GAME=OFF \
  -DCHERYL_BUILD_TUTORIAL_GAME=OFF -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF \
  -DCHERYL_BUILD_CONSUMER_TESTS=OFF -DCHERYL_STARTUP_OPENGL=OFF
```

The cache retains all assigned OFF settings, tests ON, Debug and policy minimum
3.5. Both dialogue object rules retain `-fsanitize=address,undefined`. The public
header still exposes standard CPU values and Engine visibility macros only, with
no JSON, UI or runtime dependency. The consumer includes that public header first.

## Discovery diagnosis and actual U1V commands

All commands below ran from this clone through the existing shared-lock wrapper,
with one compilation/test job and its fixed resource controls. A queued live
command is normal serialization: retain its tool session and wait with updates.
The previous checkpoint's cancelled queue waits are historical, not an unresolved
admission blocker. No admission exit **75** occurred during U1V. If a later command
returns **75**, record its exact refusal and checkpoint without an unchanged retry.

Discovery with leak detection still at its default setting and halt-on-error
explicit exited **1**:

```sh
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -P \
  builds/storytelling-u1-debug/projects/engine/tests/unit/tests_engine_6c1488ca_discovery.cmake \
  > builds/storytelling-u1-debug/u1v-discovery-default.log 2>&1
```

Complete output captured this clone's causal error:

```text
==5==LeakSanitizer has encountered a fatal error.
==5==HINT: For debugging, try setting environment variable LSAN_OPTIONS=verbosity=1:log_threads=1
==5==HINT: LeakSanitizer does not work under ptrace (strace, gdb, etc)
```

CMake reported `Error running test executable.` and `Result: 1`; this was not its
five-second timeout. The coordinator-authorized ST-008 accommodation below is
invocation-only. It disables only leak detection, retains ASan/UBSan and both
halt-on-error settings, and changes no timeout or build flag. Each command exited
**0**; the first completed incremental linking and discovery, the second listed
exactly 16 cases, and the third ran all 16:

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine --parallel 1 > builds/storytelling-u1-debug/u1v-build.log 2>&1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^tests-engine\.dialogue_definition\.' > builds/storytelling-u1-debug/u1v-registration.log 2>&1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 -R '^tests-engine\.dialogue_definition\.' \
  > builds/storytelling-u1-debug/u1v-ctest.log 2>&1
```

Complete clone-local logs live under `builds/storytelling-u1-debug/` and remain
outside the commit. Each command's status was saved immediately to the matching
`.exit` file; test details were copied before a later run could overwrite them.

| Log | Actual result |
| --- | --- |
| `u1v-discovery-default.log`, `.exit` | Exit 1, reproduced LSan tracing failure with complete CMake diagnostic/listing. |
| `u1v-build.log`, `.exit` | Exit 0, incremental `tests-engine` link and generated discovery completed. |
| `u1v-registration.log`, `.exit` | Exit 0, `Total Tests: 16`, exactly the authored names in source order. |
| `u1v-ctest.log`, `.exit` | Exit 0, `100% tests passed out of 16`, real time 1.81 seconds; no skips or unrun cases. |
| `u1v-ctest-details.log` | Complete copy of `Testing/Temporary/LastTest.log`; all 16 GoogleTest cases ran and passed, matching source and registration. |

The earlier syntax/header check at source checkpoint `25836ff` also exited **0**,
after final C++ edits/formatting. It is supporting compilation evidence, separate
from U1V's actual runtime checks:

```sh
/mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -Wall -Wextra -Wpedantic -fsyntax-only \
  -Iprojects/engine/include -Iprojects/engine/include/cheryl -Iextern/googletest/googletest/include \
  projects/engine/src/core/game-framework/dialogue.cpp projects/engine/tests/unit/src/dialogue-definition.cpp
```

The three new C++ files passed `clang-format --dry-run --Werror` at that checkpoint
using the repository style. U1V required no source fix or API change. Record links,
matching IDs, decision lengths, commands/results and staged whitespace are checked
for the follow-up records. No broader suite, SDK export, GUI or documentation
generation ran. The four-node gate remains definition test data only.

## Limits and next coordinator selection

No U1 execution blocker remains within the assigned definition scope. Confidence
is high in the reproduced LSan diagnosis and the behavior exercised by the 16
native cases. Leak coverage remains unavailable here; remove only `detect_leaks=0`
on an LSan-compatible executor to restore it while retaining both halt-on-error
settings, with no code/configuration reversal cost. Acceptance covers this
GNU/Linux Debug Engine-linked runner only.
API spelling remains an agent engineering choice with medium confidence, not
established personal user taste. U2's synchronous dispatch and value snapshots
are now implemented/executed as recorded above; JSON/console acceptance is pending.

U2 has no unresolved execution blocker. Confidence is high in the marker and
failure/commit boundaries from source review, the 34 native passes and isolated
probe. The hard active-lifecycle termination policy is a medium-confidence
engineering choice: it keeps idle adoption noexcept and prevents callback storage
invalidation, but does not recover programming misuse. Changing it is cheap while
only these initial callers exist. One serialized owner, stable handler lifetimes,
pure predicates and coherent application flag pairing remain caller obligations.
Leak checking is unavailable here; no durable external-effect/rollback guarantee
or migrated snapshot compatibility is claimed.

The next useful unit is **U3, console/JSON acceptance**, for coordinator selection.
Use the implemented gate caller plus a bounded versioned paired flag/Snapshot
codec and console commands; keep the schema proposal in agent-notes.md distinct
from current code. Required checks: both console routes, unavailable rejection,
malformed/oversized/duplicate-key/schema/content rejection before adoption,
unchanged active values after failed load, and save at gate/resume with unchanged
effect count. Review fresh-path I/O failure behavior when that unit is assigned.
Stop after U2; U3, publication transport and recurring work remain with the
coordinator. No next implementation unit has been started.
