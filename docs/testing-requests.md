# Testing requests

This is the pending user-run acceptance queue for implemented work. Requests combine
coverage from multiple development cycles. Remove accepted requests and retain only
their unresolved platforms or cases; the [roadmap](planning/develop-review-and-development-plan.md)
owns development progress, while subject guides retain reusable procedures and
meaningful coverage limits.

| Request | Type | Platform | Status |
| --- | --- | --- | --- |
| [TR6](#tr6-qa-hid-lifecycle-and-notification-observations) | QA | Linux/X11 | Deferred; blocked on backend work and an observation harness |
| [TR7](#tr7-automated-engine-asset-preparation) | Automated | Linux | Ready; reuse the existing Engine-only build |
| [TR8](#tr8-automated-controller-diagnostic-build) | Automated | Linux/X11 | Ready; configure the existing native build with HID disabled |
| [TR9](#tr9-qa-unicode-text-rendering) | QA | Linux/X11 | Ready after TR8 builds the changed demo; requires a display |

Each command block locates the checkout root with Git and runs there, so it can
be launched from `docs/` or any other directory inside this checkout. It restores
the starting directory afterward. These commands require CMake 3.28 or newer,
Ninja, a C++23 toolchain, FreeType/HarfBuzz/ICU uc/i18n development libraries and initialized pinned submodules; see
[setup and dependencies](../README.md#setup). Native builds also need OpenGL and
Python with Jinja2. Linux native builds need X11 and the selected hidapi backend's
libudev/libusb development dependencies. HID configuration can fetch pinned hidapi.
Builtin demo text has an embedded fallback and needs no installed font.
All command-line configurations explicitly select the Ninja generator.
Windows, macOS, Wayland and other platform acceptance are deferred to the
[long-term platform plan](planning/platform-acceptance.md); TR3–TR5 and the Windows
portion of TR6 are retained there, outside this active queue.

Reuse these build directories when the compiler and configuration match. Each block
batches its build targets and uses the available CPU count for builds and CTest
at normal priority, selecting only the requested cases. Stop
on a command failure. A zero-case selection or skipped case leaves that coverage
pending. Report the request ID, tested revision, platform, failures and skips;
successful automation does not establish the separate QA observations.

## TR6: QA HID lifecycle and notification observations

**Deferred and blocked:** Gainput backend work remains deferred at the pinned
baseline; an observation harness is needed before requesting this run. Successful
Gainput initialization does not prove HID readiness: the HID path can be compiled
out, and the dependency discards its initialization return code when enabled.
The demo's opt-in controller trace observes Gainput callbacks and sampled pad state,
not HID enumeration/open results or backend readiness.
The next development action is to expose those observations without changing the
[single-owner lifetime contract](../projects/modules/platform/native-glfw/README.md#input-lifetime-and-mapping).

The eventual QA request needs:

- A supported physical HID controller and explicit evidence of HID report delivery
  on Linux over USB and Bluetooth, independently of keyboard/mouse or joydev input.
- Report continuity after a second owner is rejected, after detach/same-window
  reattachment, and after destruction permits a replacement owner in one process.
- Initialization outcome, enumeration/open, selected report source and
  disconnect/reconnect observations; successful initialization alone is insufficient.

Keep this Linux request blocked until the harness has runnable setup/launch
instructions. Windows notification observations are in the
[deferred platform plan](planning/platform-acceptance.md#deferred-hid-lifecycle-and-windows-notifications).

## TR7: Automated Engine asset preparation

Build the CPU tile selector/Tileset integration, UTF-8 decoder and STBFont integration
and owned font selection with their neutral first-include probes, then run the focused selection, animation,
submission, manifest, encoding and font regressions. Coverage includes edge/corner site
labels, declared bit order, diagonal gating, outside/unknown policies, missing rules,
weighted seeded repeatability, large finite weights and malformed direct rules,
one-time target substitution, caller-owned phases, original/resolved grid bounds and
packet resource retention without native binding or world resampling.
Audio clip cases cover owned immutable PCM, mono/stereo frame counts and duration,
shared copies, preserved move sources, invalid formats/incomplete frames, non-finite
samples and normalized gain validation. The new audio contracts also have neutral
first-include probes; these do not establish a mixer or native output.
Encoding cases cover valid scalar ranges, byte offsets, embedded NUL/BOM,
maximal-subpart replacement, truncated prefixes, forbidden encodings and preservation
of valid successor bytes. Owned records survive source mutation/destruction.
Font cases cover unchanged ASCII spacing/controls, one fallback per unsupported
scalar or malformed subpart, callback exception behavior, and text packets retaining
resources and copied placement after the text/font is released. These cases use
synthetic immutable font metrics/resources and require no installed font file.
Font-selection cases use the repository-owned fallback fixture and isolated discovery
roots: verify the initial English/French/German/Russian alphabets, actual families,
explicit order, deduplication, load failures and owned
font bytes after deleting their original file. Layout cases cover accented Latin and
Cyrillic, composed/decomposed clusters and ligatures, mixed bidi/overrides/numbers,
explicit RTL leading alignment, whole-grapheme replacement, embedded NUL/malformed
source ranges, hard/soft breaks, bidi across wrapped lines, indivisible overflow
and concurrent owned layouts.
The Engine requires FreeType, HarfBuzz and ICU uc/i18n development headers/libraries
even with both UI modules disabled. Reconfigure the
matching existing build to pick up that dependency and the embedded font source.
Text-resource cases rasterize actual builtin outlines, verify repeated-glyph admission,
page cropping/padding, multiple page selection and copied provider buffers, then retain
old packet resources across successful replacement and failed candidate uploads.
Empty text uploads nothing; invalid sizes, page images and sampler collisions fail.
The existing timing cases also cover the pending ignored-result warning correction;
exception assertions explicitly discard irrelevant values while retaining the public
`[[nodiscard]]` contract. This request needs no display, controller or graphics module.
Reuse the Engine-only build from completed TR1 when its compiler/configuration match;
unrelated typed-event acceptance needs no rerun.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-engine -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=ON \
    -DCHERYL_BUILD_DEMO=OFF
  cmake --build build/testing-engine --parallel "$(nproc)" --target \
    tests-engine consumer-headers-cengine
  ctest --test-dir build/testing-engine --parallel "$(nproc)" --output-on-failure \
    --no-tests=error \
    -R '^tests-engine\.((audio_clip|tile_selection|tileset_selection|tile_animation|asset_manifest|utf8|stbfont|font_selection|text_layout|text_resources)\.|asset_submission\.(selected_tile|tile_strip_ranges|text_layout_and_retention|utf8_text)$)'
)
```

Acceptance: the implementation and public header probes compile without new warnings,
and all selected `audio_clip.*`, `tile_selection.*`, `tileset_selection.*`, `tile_animation.*`,
`asset_manifest.*`, `utf8.*`, `stbfont.*`, `font_selection.*`, `text_layout.*`, `text_resources.*`
and the four selected `asset_submission` cases
pass without skips. Legacy STBFont cases establish scalar fallback over its ASCII atlas;
the separate layout cases use actual builtin glyphs and shaping. These CPU checks do
not establish visual rendering/upload acceptance. Committed source and static checks
do not establish this acceptance.

## TR8: Automated controller diagnostic build

Build the Linux joystick correction, native input diagnostics and demo, then run
the mapper regression, synthetic joystick suite and native consumer. Reconfigure
the existing native build with owned static Gainput, HID disabled and both UI
adapters disabled. The synthetic suite supplies kernel mappings/events to the real
pad implementation and mapper, covering A presses/holds/releases, other controls,
reordered slots, d-pad hats, disconnect/reconnect and retained legacy mappings.
It requires no physical controller or display. The reusable
[Linux joystick smoke procedure](development/native-desktop-checks.md#linux-joystick-controller-checks)
covers physical reports separately.
The same batched build compiles the changed Unicode HUD, worker/dispatcher replacement
path and text-preview options. Native visual text observations belong to TR9; no
additional build of this configuration is needed before those launches.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-native-linux -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=ON -DCHERYL_BUILD_OPENGL=ON \
    -DCHERYL_NATIVE_INPUT=ON -DCHERYL_NATIVE_NULL_PLATFORM=OFF \
    -DGAINPUT_ENABLE_HID=OFF -DGLFW_BUILD_X11=ON -DGLFW_BUILD_WAYLAND=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=ON \
    -DCHERYL_BUILD_DEMO=ON
  cmake --build build/testing-native-linux --parallel "$(nproc)" --target \
    consumer-module-native-glfw tests-native-glfw tests-native-joystick demo
  ./build/testing-native-linux/cheryl-native-glfw-consumer
  printf 'Native GLFW consumer passed.\n'
  ctest --test-dir build/testing-native-linux --parallel "$(nproc)" --output-on-failure \
    --no-tests=error -R '^(tests-native-glfw\.input_mapper\.|tests-native-joystick\.linux_joystick\.)'
  python3 - <<'PY'
import json
from pathlib import Path
import shlex

build = Path('build/testing-native-linux')
source = Path('extern/gainput/lib/source/gainput/GainputInputManager.cpp').resolve()
entries = json.loads((build / 'compile_commands.json').read_text())
matches = [entry for entry in entries
           if (Path(entry['directory']) / entry['file']).resolve() == source]
if len(matches) != 1:
    raise SystemExit('Expected one owned Gainput manager compiler command.')
entry = matches[0]
arguments = entry.get('arguments') or shlex.split(entry['command'])
command = entry.get('command') or shlex.join(arguments)
output = build / 'tr8-gainput-command.txt'
output.write_text(command + '\n')
macro_arguments = [argument for argument in arguments if 'GAINPUT_ENABLE_HID' in argument]
print('Gainput manager HID macro arguments:', macro_arguments or '(none)')
print('Saved compiler command:', output)
PY
)
```

Acceptance: build/consumer/header checks and all selected mapper/joystick cases pass
without new warnings, failures or skips. The joystick runner is intentionally
separate from owner aggregates because its syscall wrappers apply process-wide.
The exported command belongs to the owned dependency; HID macro arguments should
be absent with this explicit OFF configuration. This accepts neither HID runtime
startup nor physical controller reports/reconnection.

## TR9: QA Unicode text rendering

**Platform/prerequisites:** Linux/GLFW/X11/OpenGL, a usable display/driver, and the
TR8-built `build/testing-native-linux/demo` with both UI adapters and HID disabled.
The checked-in shaders/font fixture and embedded font are available; no controller,
installed font, full image tree, clipboard or IME service is required. Close each run
before the next. Reuse the TR8 build rather than building these targets again.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-native-linux/demo --unicode-text --builtin-font
  ./build/testing-native-linux/demo --unicode-text --builtin-font --concurrent
  ./build/testing-native-linux/demo --unicode-text --builtin-font --text-direction=rtl
  ./build/testing-native-linux/demo --unicode-text --builtin-font --text-direction=rtl --concurrent
  ./build/testing-native-linux/demo --unicode-text
  ./build/testing-native-linux/demo --unicode-text --concurrent
  ./build/testing-native-linux/demo --unicode-text --builtin-font --font=assets/fonts/DejaVuSans.ttf
  env XDG_DATA_HOME="$PWD/assets" ./build/testing-native-linux/demo \
    --unicode-text --builtin-font '--font-family=DejaVu Sans'
)
```

The last run supplies a controlled family-discovery root containing the repository
fixture. Existing installed DejaVu Sans candidates can precede it; both are valid
explicit family selections. Common-family automatic runs can legitimately select
only the embedded fallback when none are installed. This establishes graceful
absence, while isolated `font_selection.families` cases establish discovery metadata
and ordering. Missing font preferences are not proof of successfully loading them.

- Inspect English, French accents and `e`+combining acute, German umlauts/`ß`, and
  Russian glyphs. Accents stay with their base and glyph masks are upright, with
  no per-byte replacements, solid rectangles or atlas bleed. In builtin-only runs,
  the Chinese missing-glyph example is one visible replacement; color/CJK rendering
  is outside acceptance.
- In automatic direction, the Hebrew-only line begins at the right of the available
  width; its letters read right to left while `123` remains left to right. The mixed
  Hebrew/English line preserves both run directions. Explicit RTL aligns the Latin
  paragraphs to the right without reversing their letters or numbers.
- Resize narrower/wider, maximize and restore while moving the mouse or entering
  text. The wrap sample and HUD follow the framebuffer width with 24-pixel margins;
  accents/whole graphemes remain together, hard breaks remain, and updates settle
  without blank text, corrupted old frames, hangs or native errors. Ordinary glyph
  overhang is separate from measured line advance; a too-wide indivisible grapheme
  can overflow and is not silently split.
- Use F2 and committed text input for supported accents/Cyrillic where keyboard
  layouts allow, then Enter/Esc to release focus. Text focus still prevents WASD
  from panning; the unfocused camera/input controls and F5 shader replacement remain
  usable. The probe edits logical scalars, so grapheme/bidi caret behavior is not
  part of this request. Static samples establish display independently of typing.
- Close during rapid resizing/text/counter changes in sequential and concurrent
  modes. Worker preparation and platform uploads settle/cancel without stale
  callbacks, deadlocks or cleanup errors. Failure-injection/retained-generation
  behavior is covered separately by `text_resources.replacement` in TR7.

Report the tested revision, launch variants, visible failures, console errors and
skips/unavailable input layouts. Finite runs and successful CPU checks do not establish
these visual observations. If using an existing toolkit-enabled build as well,
check the builtin HUD with overlapping views hidden and verify normal view startup;
toolkit text shaping/fallback remains its own service, outside this builtin scope.
