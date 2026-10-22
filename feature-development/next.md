# U1 checkpoint and next action

U1 source is committed in `25836ffb48fb25e9ed5e89d2805daf6e093ed5fc`, from design
checkpoint `716ec60984e6764a03ed375ac37826e5d92e6978`. Native verification remains
**incomplete**, not accepted. The font prerequisite is resolved: the coordinator
initialized assets at the unchanged pin
`98453b2a6e0edfcfba22eea3fde801526d72b861`, and
`assets/fonts/DejaVuSans.ttf` is present. The prescribed configuration now succeeds.
The resumed serial build compiled its remaining objects and linked `tests-engine`,
but failed during post-link GoogleTest discovery. No dialogue assertions have run.
No repository build configuration, dependency pin or U1 source changed on resume.

Contracts: [agent-notes.md](agent-notes.md); compact choices:
[decisions.md](decisions.md), ST-001/002/003/004/006. ST-005/007 remain proposals
for later units. The header/source implement owned authored values, private
deep-copy freezing, ordered inspection, scoped lookup and owned diagnostics.
The four-node gate remains test data; no session, callback dispatch or save exists.

## Actual checks and registration

The new test translation unit includes the public header first. It uses the
existing unit `src/*.cpp` glob: `projects/engine/tests/unit/CMakeLists.txt` feeds
`tests-engine-cases` through `cheryl_add_test_suite`, and
`cmake/CherylTests.cmake` calls GoogleTest discovery with prefix `tests-engine.`.
There are 16 `TEST(dialogue_definition, ...)` declarations in the source. The
generated `cmake_test_discovery_fc8c995a1d.json` under the unit build directory
contains exactly those 16 names in source order. That listing metadata is a
nonempty discovery output, not assertion execution. The corresponding
`tests_engine_6c1488ca_tests.cmake` was not produced, so CTest registration did not
complete and no nonempty CTest selection has been verified.

| Compiled case(s) | Observable boundary covered when native execution is available |
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

The following commands ran from this clone through the shared-lock wrapper.
The initial configure at the source checkpoint failed with the missing-font error
at `projects/engine/cmake/FontFallback.cmake:4`; its CTest attempt exited **8** with
`No tests were found!!!`. Those are historical failed checks, superseded by the
restored pinned input and successful configure below, not acceptance results.

Clone-local Debug configuration (GNU C/C++ 16.2.1), retry after the font was
restored, exit **0**:

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

The cache retains all requested OFF settings, tests ON, Debug and policy minimum
3.5. Debug uses the existing address/undefined sanitizers. An earlier parallel
build was interrupted by the coordinator's memory stop; its outputs were retained.
The current resume used only one compilation job:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine --parallel 1
```

This exited **1** at post-link discovery after the remaining compilation steps
and executable link. The final exact error was
`ninja: build stopped: subcommand failed.` CMake's discovery call stack was shown,
but its causal result/error was truncated from the returned tool output. A timeout
or sanitizer failure is therefore not established. The runner binary and generated
listing JSON remain clone-local; no generated registration was manually substituted.

Two focused diagnostic commands were then attempted:

```sh
/mnt/projects/cheryl-features/scripts/build -- builds/storytelling-u1-debug/tests-engine \
  --gtest_filter=dialogue_definition.* --gtest_list_tests
/mnt/projects/cheryl-features/scripts/build -- cmake -P \
  builds/storytelling-u1-debug/projects/engine/tests/unit/tests_engine_6c1488ca_discovery.cmake \
  > builds/storytelling-u1-debug/u1-discovery.log 2>&1
```

Both remained pending without output and were interrupted, exit **130** each.
The second log is empty. A nonblocking lock probe confirmed the shared native-work
lock was occupied; no native command ran through that probe. The exact cause of
the earlier post-link failure remains unresolved. No admission exit **75** was
observed; this is an occupied-lock checkpoint, not a claimed memory-admission
rejection. No further native attempt was made under unchanged conditions.

The independent focused syntax/header check at source checkpoint `25836ff` exited
**0**, with no diagnostics, after final C++ edits/formatting. This checks the
implementation and all 16 cases for C++ syntax/types; it cannot establish runtime
ownership, sanitizer behavior or assertion outcomes:

```sh
/mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -Wall -Wextra -Wpedantic -fsyntax-only \
  -Iprojects/engine/include -Iprojects/engine/include/cheryl -Iextern/googletest/googletest/include \
  projects/engine/src/core/game-framework/dialogue.cpp projects/engine/tests/unit/src/dialogue-definition.cpp
```

The three new C++ files also passed `clang-format --dry-run --Werror` at that
checkpoint using the repository style. Record-link/ID/decision-length and staged
whitespace checks cover the follow-up internal records. No broader test campaign,
documentation build or console acceptance ran. Unit acceptance and the later
console milestone remain unexecuted.

## Next action: finish U1 verification

Current blocker: the shared native-work lock was occupied when probed while the
captured diagnostic was pending, and the post-link discovery cause is still
unknown. Checkpoint idle;
no user response is expected. When admission and lock availability permit a later
resume, first run the captured discovery command above to retain its exact error.
The generated script already bounds discovery to five seconds. Diagnose that
result before changing any U1 source or runner behavior. Do not substitute fonts,
disable sanitizer checks or alter shared resource controls to get a green result.

After resolving that failure, complete the existing incremental target and inspect
the actual CTest registration before running assertions. These are **pending**
commands, not successful checks:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^tests-engine\.dialogue_definition\.'
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 -R '^tests-engine\.dialogue_definition\.'
```

Require exactly the 16 authored dialogue cases in a nonempty selection and actual
passing assertions. The header-first caller, ownership/mutation/destruction checks,
lookup misses, all validation cases and deterministic owning diagnostics must run.
Use the current one-job wrapper and clone-local outputs under its verified memory
limits. On exit **75**, record the exact resource error and checkpoint idle without
retrying unchanged conditions. A successful runner exit or commit alone is not
feature acceptance.

The next useful **implementation** unit is U2, the explicit session over these
frozen definitions, subject to a later coordinator assignment after reviewing
U1 and its native results. It owns the proposed views, scoped selection,
application handlers, failure state and value resume boundary (ST-005/007).
Review this header's borrowed observations and add the required lifetime and
no-effect-replay checks there. Do not start U2 or recurring work automatically.
