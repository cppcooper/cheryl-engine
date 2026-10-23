# U4 checkpoint and next action

TILEMAP-U4 starts at coordinator-accepted U3 checkpoint
`f91e181b2eb2870f90baea61161ddd38e1803db6` with a clean index/tree. This unit
adds only a private fixed display helper, an application-local mode and necessary
app wiring. U1 CPU Map/ranges, U2 bindings/resolver, production frame/pass/backend
and reusable startup APIs, build files and dependency pins remain unchanged.
The demo configures/links successfully and all 36 CPU regressions pass. The
capability probe cannot open DISPLAY=:1, so no demo/capture is launched and U4
visual acceptance remains blocked. Build, native access and image observations
are separate checks; source completion or a successful link is not visual acceptance.

## Owners and small caller

- [Private helper](../projects/apps/demo/src/tilemap-demo.h) and
  [implementation](../projects/apps/demo/src/tilemap-demo.cpp): two independent
  3×2/two-layer maps, stable retained bindings, required atlas validation and four
  explicit layer submissions. The new header is included first by its source.
- [Demo options](../projects/apps/demo/src/demo-options.cpp) exposes
  `--tilemap-display`; [main](../projects/apps/demo/src/main.cpp) passes it to the
  [existing lifecycle owner](../projects/apps/demo/src/demo-game.cpp). The existing
  demo source glob includes the helper; there is no new target or scene service.
- Matching [reader decisions](decisions.md) and [detailed decisions](agent-notes.md)
  retain TM-001–010. TM-009 now describes the fixed display rather than the older
  8×6/Puny World editor proposal. TM-002 corrects the alias-contract attribution.

```cpp
// init_platform(): image_shader_ comes from existing main:images material loading.
tilemaps_.emplace();
tilemaps_->load(asset_root_, engine_.resources()); // required, never an optional skip
// prepare_render_frame(), using the existing runtime's borrowed writer:
auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
const CE::Rendering::Submission::Context2D context{
    pass.semantics(), pass.parameters(), pass.constraints()
};
CE::Rendering::DrawStyle2D images; // identity model, scale 1
images.material = image_shader_;
tilemaps_->write(pass, images, context); // west ground/detail, east ground/detail
// deinit_platform(), after runtime frame recycling:
tilemaps_.reset(); // before materials/backend teardown
```

The dedicated mode returns before optional sample loading, font/text/UI/audio
samples and controls. The existing material loader still discovers the asset
root for its explicit shader-only index; it loads no optional tile/sprite packages. Fixed updates only copy framebuffer size and honor
the existing finite stop callback. Required failures retain the atlas ID/source
path; hook failure follows existing runtime stop/cleanup. Partial initialization
is tolerated by optional helper reset. Existing startup/window-close behavior is
reused. There is no editing, picking, camera input, animation, persistence,
procedural generation, resizing/chunking or sibling integration.

TM-002 is a **coordinator-selected engineering contract under delegated authority**,
not the user's explicit bind-once API preference. Bindings retain handles rather
than freeze/deep-copy assets. Metadata must stay unchanged through every alias
for the lifetime of any retaining binding, including table copies. Per-call
binding/resource stability still applies. U4 never mutates that metadata; it
introduces no hot replacement, copy layer or runtime revalidation machinery.

## Fixture observations to establish

West/east top-left world origins are `{48,240}` / `{328,208}`, cells 64×48,
full local views `{0,0,192,96}`, elapsed zero. The parsed source pivot `{0,1}`
places each source bottom-left at its cell bottom-left without reversing rows.
With a 640×360 framebuffer, west occupies X `[48,240]`, screen Y `[120,216]`;
east X `[328,520]`, screen Y `[152,248]`. These are display predictions before
image observation, not proof from update counts.

| Instance | Ground rows | Detail rows (alpha 0.5) |
| --- | --- | --- |
| west | `7,8,9 / 10,0,7` | `0,10,0 / 9,0,0` |
| east | `9,7,0 / 8,10,9` | `10,0,10 / 0,7,0` |

IDs 7–10 bind to original targets 0–3 (brown/black/grey/red); zero is empty.
The four groups emit 5+2+5+3 packets in explicit caller order. Ground/detail
blends and a detail over empty ground distinguish composition. A black tile is
still drawable and is not the empty cell. No implicit sorting or Z policy is added.

## Prerequisite inventory versus preparation

The coordinator's package/environment/socket inventory was read-only and did not
establish context authorization, renderer readiness or visual acceptance.
Assets stay at `98453b2a6e0edfcfba22eea3fde801526d72b861`, clean. The helper
uses Reader on committed `graphics/definitions/tilesets/tileset.json`, checks
qualified `tileset:tileset`, the resolved texture path, parsed 1×4/100×100 grid,
zero origin/spacing, pivot `{0,1}` and no animations. It decodes the PNG and
requires 400×100 RGBA8, checks chosen targets 0–3 against image bounds, uploads
through Images::Manager, then loads/retrieves through Tilesets::Manager. Retained
metadata/image dimensions are checked again before binding. Missing Puny World,
dungeon and magecity PNGs are neither prerequisites nor accepted optional skips.
No artwork, tools or packages were downloaded/installed/copied from dirty trees.

Each previously empty dependency directory and absent local metadata was checked
against the exact HEAD gitlink and the corresponding protected read-only Git
store's commit object before population:

| Dependency | Unchanged pin |
| --- | --- |
| extern/glfw | dbadda26835ec5089ef922e6c290bcf58cf12056 |
| extern/gainput | 0ed32c7619ea1cf8047441e330f7729f1ff72a31 |
| extern/glad | 01277173613cdb0339f58de9f6d090474ddbeab2 |
| extern/cli11 | eced4d837d0ab57242b1c25d03780d35bf2b0d52 |

For each NAME/PIN, the actual preparation was:

```sh
git init --quiet extern/NAME
git -C extern/NAME -c protocol.file.allow=always fetch --no-tags --depth=1 \
  /mnt/projects/cheryl-engine/.git/modules/extern/NAME PIN
git -C extern/NAME checkout --quiet --detach PIN
```

This is local committed-object transfer only, not a network fetch or protected
worktree/config/index/cache copy. Each checkout is clean, detached at its exact
pin, with independent .git/object storage, no hardlinks/symlinks/alternates and no
remote. The feature origin/global configuration is unchanged. Complete commands
and diagnostics are in `build/tilemap-u4-display-debug/u4-dependencies.log`.

## Actual commands and results

All configure/build/native probe/run/capture/test work uses
`/mnt/projects/cheryl-features/scripts/build --`, one job, existing shared lock,
fixed 4 GiB aggregate / 3 GiB throttling / 2 GiB worker / no-swap limits and 4 GiB
MemAvailable admission floor. Queued sessions are retained through normal waiting.
An exit-75 resource refusal requires its exact diagnostic and checkpoint idle,
without an unchanged retry. No subagents or external agents are used.

Preserve `build/tilemap-u1-debug`; it disables native/startup/demo modules. The
separate ignored `build/tilemap-u4-display-debug` is justified by those missing
module contracts, not a new test runner. Configuration actually executed:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake -S . -B build/tilemap-u4-display-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCHERYL_BUILD_SHARED_LIBS=ON -DWARN=ON \
  -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_NATIVE_INPUT=ON -DCHERYL_BUILD_OPENGL=ON \
  -DCHERYL_BUILD_STARTUP=ON -DCHERYL_STARTUP_OPENGL=ON -DCHERYL_BUILD_DEMO=ON \
  -DGLFW_BUILD_X11=ON -DGLFW_BUILD_WAYLAND=OFF -DCHERYL_NATIVE_NULL_PLATFORM=OFF \
  -DGAINPUT_ENABLE_HID=OFF -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF \
  -DCHERYL_BUILD_TESTS=OFF -DCHERYL_BUILD_NATIVE_SDL3=OFF -DCHERYL_BUILD_WORKERS=OFF \
  -DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF -DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=OFF \
  -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
  -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF -DCHERYL_BUILD_DOCS=OFF -DCHERYL_BUILD_SDK=OFF \
  -DCHERYL_BUILD_FIRST_GAME=OFF -DCHERYL_BUILD_TUTORIAL_GAME=OFF \
  -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF -DCHERYL_BUILD_CONSUMER_TESTS=OFF
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug --target demo --parallel 1
/mnt/projects/cheryl-features/scripts/build -- timeout --signal=TERM --kill-after=2s 15s glxinfo -B
```

Configuration exited 0 with GNU 16.2.1, Debug sanitizer instrumentation and WARN ON.
Optional `Could NOT find libdwarf` is nonblocking. GLAD uses the existing
REPRODUCIBLE path and pinned gl.xml/khrplatform files; its static opener's
intercepted-URL log describes local packaged bytes, not a download. No build
behavior is changed to bypass a prerequisite.

The demo build exited 0, with no source warnings/errors. Its new header is
included first and compiles in the real app target. Compile commands retain
`-g -Wall -fsanitize=address,undefined`. The build compiled Engine, NativeGLFW,
OpenGL, Startup and the app using one job; only target demo was requested. No
resource refusal occurred. The lock queue was allowed to finish without relaunch.
Complete configure/build logs are `u4-configure.log` and `u4-build.log` under the
graphics output directory; GLAD's `gladsources/gl46/args.txt` records --reproducible.

The bounded capability probe actually exited **255** with the complete diagnostic:

```text
Error: unable to open display :1
```

This confirms unavailable X11 access for this invocation, without diagnosing an
authorization/server cause. Environment/socket/package inventory is insufficient
to override that result. No unchanged retry, alternate display route, session/
driver/global setting change, headless server, demo execution or capture was
attempted. Its full diagnostic is `u4-display-probe.log`. No resource/lock error
or build error is being conflated with this display-access failure.

The ignored one-shot `u4-display-check.py` is prepared but **unexecuted**. It would
start a 40-second internal watchdog only after wrapper admission, launch its own
finite demo, then use X11 metadata to require the live child PID and unique title
before a single ImageMagick import of that window. It has no desktop capture or
unrelated-process control. It requires a later changed display condition. The
intended child command, **not run**, is:

```sh
build/tilemap-u4-display-debug/demo --tilemap-display --builtin-font --max-updates 600 \
  --window-width 640 --window-height 360 --window-title 'Cheryl tilemap U4 UNIQUE' --swap-interval 1
```

The committed local PNG was inspected as a source image (400×100; brown, black,
grey and burnt-red cells), not a rendered screenshot. Runtime Reader/decode and
manager validation are implemented/compiled but were not executed by the app.
No update completion, pixels, layer blending, map placement, native cleanup or
rendered resource lifetime is claimed. There is no u4-fixture.png artifact.

The existing CPU runner/configuration and incremental outputs were reused;
Engine/module source and all 36 cases are byte-identical to accepted U3. No CPU
reconfigure/rebuild was needed for private app changes. Actual commands:

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug -N \
  -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer)\.'
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer)\.'
```

Discovery exited 0 and selected exactly 36 cases: six tilemap_consumer (#200–205),
15 tilemap_submission (#206–220), 15 tilemap (#221–235). Execution exited 0,
**36/36 passed, no skips, 26.48 seconds**. Full outputs are preserved as
`build/tilemap-u1-debug/u4-registration.log`, `u4-ctest.log`, `u4-LastTest.log`
and the normal Testing/Temporary/LastTest.log. ASan/UBSan stay enabled with
halt_on_error and no sanitizer diagnostic. Only command-local leak detection is
disabled for the known tracing limitation; leak checking remains unverified.

The cases retain the established invalid-bound/overflow/computed-edge oracle,
independent values/edits/clocks, resolver/group failure preservation and weak
resource lifetime observations. Synthetic native bind/draw/upload counters stay
zero in those CPU fixtures. No new unit merely restates app constants; actual
app-mode/lifecycle/image evidence remains pending display access. No broad suite,
extra native runner, documentation build or performance campaign is run.
Static Git whitespace and focused formatting checks pass. Production Engine/
module APIs, source/build files, unit cases and gitlinks match accepted U3.

## Confidence, limitations and next action

Engineering confidence in using existing ownership/submission contracts is high.
The mode flag, fixed data/placements and translucent style are narrow reversible
app choices with low personal-preference confidence. The attribution correction
changes no source contract. Reversal cost stays within the private helper/wiring
and these records; no reusable ABI change is introduced by U4.

U1/U2/U3 synthetic coverage remains source for value/range/failure/retention
contracts, not visual edit acceptance. The fixed fixture does not establish
interactive paint/erase/picking, camera-driven visibility, concurrent metadata
mutation, performance, graphics-domain lifetime proofs or full milestone acceptance.
Installed SDK/Windows exports, other compiler/STL/GLM ABIs and Release remain
unverified. Existing exposed STL/GLM layouts require matching consumer toolchains;
U4 neither changes nor certifies a stable cross-toolchain public ABI.

Leak checking remains unverified because the previous LeakSanitizer run reported:

```text
LeakSanitizer has encountered a fatal error.
HINT: LeakSanitizer does not work under ptrace (strace, gdb, etc)
```

Only invocation-local detect_leaks=0 is permitted; ASan/UBSan and halt_on_error
remain enabled. No global sanitizer setting is changed.

The next bounded action is **complete U4's visual observation after X11 access
changes**. Re-run the recorded capability probe once under the wrapper only when
that prerequisite changes. If accessible, run the preserved one-shot check with
command-local sanitizer options through scripts/build, inspect only its owned
fixture image and compare the two map footprints, empty cells and later detail
blends above. The bounded orchestrator command is proposed, not executed:

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- python3 build/tilemap-u4-display-debug/u4-display-check.py
```

No user response is expected. Do not retry unchanged display access or treat a
future update count as an image check. Editing/picking needs a later coordinator
assignment, and must agree with computed grid boundaries rather than bare floor.
No next feature unit or recurring schedule is activated. Checkpoint only this
helper/app wiring and internal records, preserve outputs/pins, leave assigned-
branch transport to the coordinator publisher, and stop.
