# U1 checkpoint and next unit

U1 source is implemented from design checkpoint
`716ec60984e6764a03ed375ac37826e5d92e6978`. Native verification is **blocked**, not
accepted. The prescribed Debug configure failed because the baseline Engine
unconditionally reads the absent `assets/fonts/DejaVuSans.ttf` at
`projects/engine/cmake/FontFallback.cmake:4`. The asset submodule is uninitialized.
No asset substitution, pin change, protected cache, installation, download or
alternate native test runner was used. No repository build configuration was edited.

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
There are 16 `TEST(dialogue_definition, ...)` declarations in the source. Failed
configuration prevented generated discovery and linking; these are **authored
cases**, not a nonempty executed selection.

| Authored case(s) | Observable boundary covered when native execution is available |
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

The configuration, CTest attempt and syntax checks below ran from this clone
through the shared-lock wrapper.
Fresh clone-local configuration (GNU C/C++ 16.2.1), exit **1**:

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

Exact blocking error:

```text
CMake Error at projects/engine/cmake/FontFallback.cmake:4 (file):
  file failed to open for reading (No such file or directory):

    /mnt/projects/cheryl-features/projects/storytelling/assets/fonts/DejaVuSans.ttf
```

The build was not attempted after this failure; no `build.ninja` was generated.
The requested CTest command ran once, exit **8**, reporting `No tests were found!!!`:

```sh
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error -R '^tests-engine\.dialogue_definition\.'
```

Independent focused syntax/header check below exited **0**, with no diagnostics.
It was repeated successfully after final C++ edits/formatting. This checks the
definition implementation and all 16 cases for C++ syntax/types, not linking,
runtime ownership, sanitizer behavior or assertion outcomes:

```sh
/mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -Wall -Wextra -Wpedantic -fsyntax-only \
  -Iprojects/engine/include -Iprojects/engine/include/cheryl -Iextern/googletest/googletest/include \
  projects/engine/src/core/game-framework/dialogue.cpp projects/engine/tests/unit/src/dialogue-definition.cpp
```

The three new C++ files also pass `clang-format --dry-run --Werror` using the
repository style. Record-link/ID/decision-length and staged whitespace checks
cover the internal records. No full documentation or broader/native test campaign
ran. Unit acceptance and the later console milestone remain unexecuted.

## Recovery before the next implementation unit

Make the exact fallback-font input available from the recorded asset revision in
this clone without changing pins or using protected workspaces. Then rerun the
configuration above; only after it succeeds, build the assigned target:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine --parallel 4
```

Inspect generated discovery and require a nonempty `tests-engine.dialogue_definition.*`
selection, then rerun the exact CTest filter above. All 16 cases must actually run;
an empty selection or successful syntax check cannot close this gap. No unchanged
configure failure or missing-input download is being retried in this task.

The next useful **implementation** unit is U2, the explicit session over these
frozen definitions, subject to a later coordinator assignment after reviewing
U1 and its native results. It owns the proposed views, scoped selection,
application handlers, failure state and value resume boundary (ST-005/007).
Review this header's borrowed observations and add the required lifetime and
no-effect-replay checks there. Do not start U2 or recurring work automatically.
