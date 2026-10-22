# SETTINGS-U1 handoff and next unit

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

## Executed configuration and checks

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

## Observed coverage and limits

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
coordination, hot reload, or proven Windows behavior. Symlink leaves reject;
`.pending` collisions require resolving the stale/conflicting file before another
save. Allocation/programming failures are not ordinary recovered-defaults states.
No unresolved U1 prerequisite remains after the passed focused checks.

## One proposed next unit, not started

U2 should make installed context membership and published device admission
observable in immutable input snapshots (SET-003/005). Add the proposed
`Snapshot::has_context(ContextId)` and `device_enabled(DeviceId)` with focused
owner cases for inactive installation, final release/retirement, retained gate
state, held seeding, failed publication/retry, and delayed/queued polls. Preserve
existing mapper ownership and controller behavior. Select actual runner cases
before executing, use the current one-job resource wrapper, and report skips or
unavailable coverage explicitly.

The coordinator selects U2 separately. Do not wire a settings editor, runtime
Apply transaction, broader configuration system, or recurring work as a side
effect of this handoff. The future score consumer still needs the publication
and focus-release watermark design; U1 file success does not establish it.
