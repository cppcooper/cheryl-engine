# SETTINGS-U2 handoff and next unit

U2 adds only immutable snapshot context presence and publication-time device
admission for SET-003/005, based on accepted U1 commit
`e4529aa9229334433a135b51598ebb50c11b8f4f`. The small future caller in
[agent notes](agent-notes.md) retains the candidate lease and editor generation,
checks membership in a newly consumed sample, and separately requires a genuinely
new admitted poll beyond the release watermark. No editor, score-context
integration, runtime Apply, modal gate, Workers, or sibling integration is added.

U1 behavior is unchanged. The clarified contract is that loading rejects symlink
leaves, while saving replaces the leaf link itself by rename without writing its
referent. A Linux disposable-file case now exercises both operations and exact
referent-byte preservation.

The focused Linux Release selection passed **82/82 with no skips or failures**:
55 Engine cases and 27 tutorial cases. The 26 previously accepted tutorial cases
remain selected; the additional case is `tutorial_settings.symlink_leaf`.

## U2 executed commands and results

Reuse `build/settings-u1-release` without changing its configuration or dependency
pins. This is Linux Release with GNU C++ 16.2.1 and the accepted U1 configuration
recorded below. All native work uses the existing shared-lock/resource wrapper and
one job. A queued command retains its tool session; exit 75 instead means record
the exact refusal and checkpoint idle without retrying unchanged conditions.
Complete output is retained in clone-local, ignored
`build/settings-u1-release/settings-u2-logs/`.

Build command, exited 0:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/settings-u1-release \
  --target tests-engine tests-tutorial-game --parallel 1 \
  > build/settings-u1-release/settings-u2-logs/build.log 2>&1
```

Only these runners and their required Engine/test dependencies were built. TGUI
and the graphical tutorial app were neither built nor run. No fresh configuration,
download, system install, pin change, subagent, or broad test campaign was needed.

Discovery command, followed by explicit case/group-count validation:

```sh
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/settings-u1-release -N \
  -R '^(tests-engine\.(action_contexts|input_bindings|tick_input|input_state|input_contract|input_routing|input_capture|polling_backlog)\.|tests-tutorial-game\.(tutorial_settings|tutorial_game)\.)' \
  --no-tests=error --parallel 1 \
  > build/settings-u1-release/settings-u2-logs/discovery.log 2>&1
```

Discovery exited 0. Inspection/count validation established the actual
`tests-engine.` and `tests-tutorial-game.` prefixes and all required new/extended
cases, then checked that every original tutorial test was present:

| Group | Selected / passed |
| --- | --- |
| `tests-engine.action_contexts` | 16 / 16 |
| `tests-engine.input_bindings` | 8 / 8 |
| `tests-engine.input_capture` | 6 / 6 |
| `tests-engine.input_contract` | 3 / 3 |
| `tests-engine.input_routing` | 5 / 5 |
| `tests-engine.input_state` | 8 / 8 |
| `tests-engine.polling_backlog` | 7 / 7 |
| `tests-engine.tick_input` | 2 / 2 |
| `tests-tutorial-game.tutorial_settings` | 22 / 22 |
| `tests-tutorial-game.tutorial_game` | 5 / 5 |

Executed with the same nonempty selection:

```sh
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/settings-u1-release \
  --output-on-failure \
  -R '^(tests-engine\.(action_contexts|input_bindings|tick_input|input_state|input_contract|input_routing|input_capture|polling_backlog)\.|tests-tutorial-game\.(tutorial_settings|tutorial_game)\.)' \
  --no-tests=error --parallel 1 \
  > build/settings-u1-release/settings-u2-logs/tests.log 2>&1
```

Execution exited 0 and reported `100% tests passed out of 82`, real time 0.58 s.
A read-only Python check compared every discovered name with every executed
`Passed` name, rather than relying on runner exit alone. It also required a nonzero
count for every selected group and the preserved original 26 tutorial names.
`discovery-counts.json` and `verification-summary.json` retain those counts beside
the full logs. The build log has no warning/error diagnostics. Formatting of the
changed code, local-link/decision consistency and staged whitespace are reviewed
separately; documentation generation and native hardware/GUI acceptance did not run.

No U2 blocker remains. These checks establish the bounded snapshot contracts,
including metadata-only backlog delivery and failure/retry. They do not establish
the future editor, runtime Apply or rearming workflow.

## U2 contracts, confidence, and limits

- `has_context` reads the existing immutable namespace map: zero, pending,
  cancelled-before-installation and subsequently retired IDs are absent. Empty
  and inactive installed contexts are present. Departure includes one final
  release/zero sample. Clear/shutdown invalidate leases without changing retained
  history, so presence cannot prove live ownership.
- `device_enabled` copies the publishing mapper's gate set before any snapshot
  commit. Unlisted IDs are enabled even without hardware. Later live changes
  cannot rewrite a retained handle or value copy. Metadata-only polls can have
  `has_changes() == false`; backlog and accumulator must preserve them.
- Failure/retry coverage reuses controlled finite-axis overflow after another
  context candidate is prepared. It checks the complete prior published handle,
  old gate policy, and retained transitions, then checks both contexts and new
  metadata after retry. This is a bounded evaluation-failure case, not RAM
  exhaustion or allocation-failure injection.
- Engineering confidence is high in these narrow, source-additive queries;
  membership uses existing namespace ownership and admission adds one owned set
  copy per mapper publication. This changes `Snapshot` layout and requires Engine
  and consumers to rebuild together. Reversal is inexpensive before application
  integration. No v3 evidence is being promoted into a user preference.
- Future Apply must retain a valid candidate and matching editor generation as
  well as consumed membership. Neutral rearming additionally needs new consumed
  polls beyond its cycle's release watermark; `latest_poll()` on an empty batch
  is a baseline. UI/input isolation, runtime scheduling integration, native
  hardware, cross-platform file replacement and power-loss durability remain
  outside this unit's evidence.

## Accepted U1 verification history

The following records the earlier accepted value/file checks, not new U2 runs.
Current regression counts and diagnostics belong to the U2 evidence above.

U1 implements the app-private `GameSettings`, shared validator, and `SettingsFile`
boundary in SET-002/006/007. The tested working tree is based on design checkpoint
`157c75736bf5d325c8ea5174430b5dbbd75a2ae4`; source, tests, registration, and these
records belong to its coherent local checkpoint. JSON stays private. UI, input
contexts/queries, Workers, and runtime Apply remain later work.

The Linux Release runner passed **26 selected cases: 21 `tutorial_settings.*`
and five existing `tutorial_game.*`, with no skipped or failed cases**. CTest
listing established a nonempty selection with the actual prefix
`tests-tutorial-game.tutorial_settings.` / `tests-tutorial-game.tutorial_game.`;
execution used the same regex and `--no-tests=error`. This is acceptance for the
bounded value/file unit, not the full settings milestone.

### Executed configuration and checks

Commands run from this clone. The existing Release directory is
`build/settings-u1-release`, using GNU C++ 16.2.1 and the pinned local dependencies.
All configure/compiler/build/CTest commands went through the shared-lock wrapper.
The memory continuation now enforces one job and verified limits; exit 75 must
be recorded and left idle without an unchanged retry.

The initial configure stopped at `projects/engine/cmake/FontFallback.cmake:4`:
`file failed to open for reading (No such file or directory)` for
`assets/fonts/DejaVuSans.ttf`. The coordinator initialized assets at unchanged pin
`98453b2a6e0edfcfba22eea3fde801526d72b861`; retrying the same configuration then
succeeded. JSON is populated at unchanged pin
`b36f4c477c40356a0ae1204b567cca3c2a57d201`. No substitution or pin change was used.

Successful configure command (line wrapping only):

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake -S . -B build/settings-u1-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_TUTORIAL_GAME=ON \
  -DCHERYL_BUILD_STARTUP=ON -DCHERYL_STARTUP_OPENGL=ON \
  -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_NATIVE_INPUT=ON -DCHERYL_NATIVE_NULL_PLATFORM=OFF \
  -DGAINPUT_ENABLE_HID=ON -DGLFW_BUILD_X11=ON -DGLFW_BUILD_WAYLAND=OFF \
  -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF \
  -DCHERYL_BUILD_OPENGL=ON -DCHERYL_BUILD_UI_TGUI=ON \
  -DCHERYL_BUILD_WORKERS=OFF -DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF \
  -DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=OFF -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
  -DCHERYL_BUILD_UI_RMLUI=OFF -DCHERYL_BUILD_DOCS=OFF -DCHERYL_BUILD_SDK=OFF \
  -DCHERYL_BUILD_DEMO=OFF -DCHERYL_BUILD_FIRST_GAME=OFF \
  -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF -DCHERYL_BUILD_CONSUMER_TESTS=OFF \
  -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF
```

Discovery established the host OpenGL/X11/HIDAPI configuration; optional libdwarf
was absent while Backward had libdw/libbfd. Gainput's disconnected FetchContent
emitted a warning for the unpopulated build-local `_deps/hidapi-src`, with host
`hidapi_DIR=/usr/lib/cmake/hidapi` already discovered. Configuration/generation
succeeded. This does not claim compilation of Native GLFW, OpenGL, or TGUI.

The original `tests-tutorial-game` build completed before the memory continuation
with the then-authorized `--parallel 4`; its outputs were preserved. Current
incremental check used one job:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/settings-u1-release \
  --target tests-tutorial-game --parallel 1
```

It exited 0 after acquiring the shared lock, reporting `ninja: no work to do.`
The preserved runner and sources required no further rebuild or source fixes.

The new settings source is registered with private pinned JSON includes in both
app and test-case targets; `compile_commands.json` confirms both registrations.
Only the test runner and its Engine/GoogleTest dependencies were built. The
`build/settings-u1-release/tutorial-game` executable and TGUI shared library
were not produced. No device, GUI, broad acceptance, or documentation run occurred.

Actual nonempty discovery and native execution commands:

```sh
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/settings-u1-release \
  -N -R '^tests-tutorial-game\.(tutorial_settings|tutorial_game)\.' --no-tests=error --parallel 1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/settings-u1-release \
  --output-on-failure -R '^tests-tutorial-game\.(tutorial_settings|tutorial_game)\.' --no-tests=error --parallel 1
```

Both exited 0; listing reported 26 tests and execution reported 26/26 passed.
The final warning-free syntax check also exited 0:

```sh
/mnt/projects/cheryl-features/scripts/build -- /usr/bin/c++ -std=c++23 -O3 -DNDEBUG \
  -Wall -Wextra -Wpedantic -Werror -fsyntax-only -Iprojects/engine/include \
  -Iextern/json/single_include -Iextern/googletest/googletest/include \
  projects/apps/tutorial-game/src/settings.cpp projects/apps/tutorial-game/tests/settings.cpp
```

That earlier syntax result alone was not native acceptance; the CTest results
above now supply the executed evidence. Internal link/decision consistency,
formatting, and staged-whitespace review supplement those checks.

### Observed coverage and limits

The cases exercise defaults and typed boundaries; the permitted portable-key
whitelist; complete round trips; missing versus operational read outcomes; strict
schema/types/versions/duplicates, including escaped equivalent names; numeric
overflow/range rejection; malformed/trailing input; all three required raw-NUL
suffixes; exact 64 KiB and oversized files; shallow rejection of deeply nested
input; and complete defaults without rewriting rejected source bytes.

Save cases exercise validation before any temporary open; exclusive collision
preserving another attempt's exact bytes; open, partial-write, flush, close, and
rename failures; one close and only owning cleanup; cleanup failure diagnostics;
propagation of allocation/programming exceptions; and real replacement/reload.
The post-rename case confirms successful commit performs no cleanup that could
turn success into failure. Relative paths remain fixed across later cwd changes.

File evidence is limited to this Linux host and its clone-local filesystem.
There is no fsync/power-loss guarantee, network-filesystem guarantee, multi-process
coordination, hot reload, or proven Windows behavior. Loading rejects symlink leaves; saving replaces the leaf link itself by rename
without writing its referent.
`.pending` collisions require resolving the stale/conflicting file before another
save. Allocation/programming failures are not ordinary recovered-defaults states.
No unresolved U1 prerequisite remains after the passed focused checks.


## One proposed next implementation unit, not started

U3 should introduce the tutorial's app-private score-context owner and retained
candidate lifecycle using the U2 queries. Keep old/candidate leases independent,
track editor generation, and make consumed publication, cancellation, clear and
shutdown outcomes explicit before any editor or Apply integration. Preserve
existing controller and non-score mappings. Show its small caller before adding
helpers; no UI, persistence transaction, focus gate, or recurring work is activated
by this handoff.

Its focused checks should cover an inactive candidate, attachment during a poll,
held seeding, no new polls, stale generation, cancellation before observation,
clear/shutdown invalidation, and final old-context release/retirement. The
coordinator selects that bounded unit separately. Stop after U2 and its checkpoint.
