# Miniaudio audio backend

`Cheryl::Audio::Miniaudio` implements Engine's
[audio ownership and playback contracts](../../../../docs/runtime/audio.md).
It owns CPU file decoding, native output, mixing, streamed music and voice lifetime.
It links Engine publicly and its audio SDK privately; it needs no window, graphics
module or UI toolkit. Public headers contain no miniaudio types.

## Selection and dependency

Select `CHERYL_BUILD_AUDIO_MINIAUDIO=ON` at the repository root and link:

```cmake
target_link_libraries(game PRIVATE Cheryl::Engine Cheryl::Audio::Miniaudio)
```

This module defaults to `OFF`, preserving existing Engine-only and graphical
assemblies until their applications select audio. It has a standalone entry point:
set `CHERYL_REPOSITORY_ROOT` to an absolute checkout path before adding the owner.
It reuses `Cheryl::Engine` or bootstraps Engine through the common module helper.

Miniaudio is selected for its available source, permissive licensing and support
for decoding, mixing and streaming.

The owner reuses a supplied `miniaudio::miniaudio` or `miniaudio` target. Otherwise
it uses `CHERYL_MINIAUDIO_SOURCE` or the pinned `extern/miniaudio` submodule at
0.11.25 (`9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`). Supplied dependencies must
provide the high-level engine, resource manager, decoding and device APIs with
matching public compile definitions. The private SDK support target compiles only
`miniaudio.c`; it does not configure upstream extra nodes, codecs, examples or
installation, and it does not rewrite host cache options.

Miniaudio offers the Unlicense and MIT No Attribution; its notices remain in
[LICENSE](../../../../extern/miniaudio/LICENSE). Linux output loads supported native
audio backends dynamically and links Threads, the platform loader and the math
library. Engine-only selection neither discovers nor compiles miniaudio.

## Preparation and output

```cpp
#include <cheryl/backends/miniaudio.h>

auto audio = CE::Audio::Miniaudio::System::open_device();
const auto effect = CE::Audio::Miniaudio::decode_file("effect.wav");
auto voice = audio->play(effect, {.volume = 0.5f});
auto music = audio->stream("music.flac", {.volume = 0.25f, .looping = true});
// During normal application maintenance:
audio->maintain();
// After application producers settle:
audio->close();
```

`decode_file` supports the SDK's built-in WAV, FLAC and MP3 decoders and returns
immutable source-rate mono/stereo PCM. It opens no device. Whole-file decode has
a caller-selected byte budget (64 MiB by default); over-budget, empty, malformed,
unsupported format and non-finite sample data fail without publishing a clip.
Ogg/Vorbis and additional custom codecs are outside this initial selection.

`open_device` requests stereo 48 kHz by default; supported mono/stereo output is
8–192 kHz. `output_format` reports the mixer's selected format, which the device
backend may convert to its hardware format. The system excludes Null from native
backend selection and throws when device initialization fails. `backend_name`
identifies the selected native backend. Offline output uses `open_offline`, opens
no device and reports `offline`; `render` alone advances its clock. Native systems
reject manual mixing. Offline output spans contain complete interleaved frames.
Successful renders fill the whole span, including silence before playback or after
voice retirement, and advance the clock by the requested frame count. Voices bypass
pitch processing at matching sample rates; differing source rates still use the
SDK's rate conversion. Cheryl exposes no pitch or spatialization control.

File stream opening waits for initial readiness; subsequent decode-ahead uses the
SDK's resource-manager worker. Keep the file and its contents stable until playback
ends. Restart prepares a fresh node and cursor to discard processing caches;
for streams it reopens the file. Preparation failure preserves the previous voice.
A native start failure leaves it stopped. Control changes take effect at backend
processing boundaries. Buffered decoded PCM can delay a looping-policy change;
already queued device output cannot be withdrawn.

## Repeating root-assembly validation

Reuse `build/testing-audio` when its toolchain matches; it
keeps the optional owner separate from Engine-only and native/graphics selections.
Initialize the pinned SDK and Engine prerequisites from
[setup](../../../../README.md#setup). These commands need CMake 3.28 or newer, Ninja
and a C++23 toolchain, and preserve the caller's working directory.

Rerun cases only when related source/configuration changes invalidate their evidence.
The complete owner selection below includes decode, mixing and short-stream cases;
narrow it for a focused change and avoid repeating cases through aggregates.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-audio -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=ON -DCHERYL_MINIAUDIO_SOURCE= \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF \
    -DCHERYL_BUILD_DEMO=OFF
  cmake --build build/testing-audio --parallel "$(nproc)" --target \
    tests-audio-miniaudio consumer-module-audio-miniaudio
  ctest --test-dir build/testing-audio --parallel "$(nproc)" --output-on-failure \
    --no-tests=error -R '^tests-audio-miniaudio\.(audio_mix|audio_decode|audio_stream)\.'
  ./build/testing-audio/cheryl-audio-miniaudio-consumer --offline
)
```

## Acceptance boundaries

`tests-audio-miniaudio` owns offline PCM, mixing/control, owned-source, file decode,
short-stream and lifetime regressions. `all-audio-miniaudio` and the root `all-tests`
reuse those case objects. `consumer-module-audio-miniaudio` supplies an independent
audio consumer and SDK-free first-include probe. None requires a display or device.

The consumer defaults to `--offline`. Its `--device` mode opens native output and
generates its own 20-second stereo WAV: left/right alternate each second, and pitch
steps through 220/440/660/880 Hz. It requires no external media. `--producers` adds
an independent 200 ms, 440 Hz two-channel effect every two seconds. It continues
after music pauses or stops; music commands and `v` control only the stream, while
`m` controls all output. Maintenance continues while stdin waits. `--stream=FILE`
selects another stable WAV/FLAC/MP3 source. The consumer settles its producer before closing output and
then observes a handle surviving destruction.

| Command | Action |
| --- | --- |
| `p`, `r` | Pause and resume music. |
| `s`, `x` | Stop and restart music from the beginning. |
| `l` | Toggle looping. |
| `e` | Submit three overlapping short effects without retaining handles. |
| `v VALUE`, `m VALUE` | Music or master volume, in `[0, 1]`. |
| `t`, `q` | Observe state or close output. |

Linux Release root-assembly WAV/FLAC/MP3 decoding, offline mixing/control/short-stream
regressions, consumer/header checks and native stereo output/sustained WAV streaming
are accepted. Native acceptance covers music/effect controls, independent producers,
loop/restart and shutdown/relaunch. WAV coverage uses generated PCM16 fixtures,
byte-budget boundaries, missing/corrupt paths and owned PCM after source-file removal.
Short-stream offline cases use an initially buffered WAV and do not establish
sustained decode-ahead, native latency,
audible channel routing or device shutdown. Owned
[codec fixtures](tests/fixtures/README.md) cover whole-clip FLAC/MP3 decoding;
the native QA baseline uses a long WAV and does not establish compressed-stream
seek/loop behavior. Supplied-SDK and standalone configurations remain unaccepted
composition variants until explicitly selected and built. Other platforms remain deferred under the
[platform plan](../../../../docs/planning/long-term/platform-acceptance.md).

## Repeating native WAV validation

Reuse the matching `build/testing-audio` consumer from the root-assembly procedure
above. Native checks require a working audio backend/server/device and audible
stereo output; no display or external media is needed. The generated 20-second WAV
exceeds the SDK's two one-second stream pages. Offline success does not establish
audible channel routing, sustained decode-ahead or device shutdown. If stereo
output or a usable device is unavailable, provide it before requesting native
acceptance. A real backend must be reported; offline/Null is outside native scope.

Run each launch, complete the observations, then enter `q` before the next launch:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build/testing-audio/cheryl-audio-miniaudio-consumer --device
  ./build/testing-audio/cheryl-audio-miniaudio-consumer --device --producers
)
```

- Listen past the initial two seconds. The generated stream alternates left/right
  once per second and cycles through 220/440/660/880 Hz; it continues for 20 seconds
  without dropouts or becoming silent when terminal input is idle. Check left/right
  with stereo output, rather than a mono speaker configuration.
- Enter `p`, wait, then `r`: the stream pauses and continues its previous sequence.
  In the producer launch, an independent 200 ms, 440 Hz two-channel effect continues
  every two seconds while music is paused or stopped. Music commands and `v` affect
  only the stream; `m` affects all output. Enter `e` repeatedly: three distinct
  overlapping effects finish after their handles are discarded, without cutting off music.
- Enter `v 0`, then `v 0.5`: music alone mutes/restores. Enter `m 0`, then `m 0.5`:
  all output mutes/restores while playback time continues. An invalid gain such as
  `v -1` reports a command error and preserves the previous gain/playback.
- Enter `s`, then `r`: stopped music remains stopped. Enter `x`: it restarts at
  the initial low left-channel tone rather than replaying an old cached segment.
  Leave looping off for a full 20-second playback; `t` reports `Finished` and `r`
  leaves it finished. Enter `l`, then `x`, and listen through the 20-second boundary:
  playback loops and remains `Playing` without a multi-second gap or stale segment.
- In both launches, close with `q` while music/effects are active, then relaunch.
  Shutdown joins the producer, releases the device without hanging or trailing
  playback, and reports the surviving music handle as `Closed` after destruction.
  Relaunch obtains usable output again. Audible artifact/latency observations are
  separate from the sample-level offline regressions.

These observations cover the generated long WAV. Native compressed-stream seek/loop
requires a stable long FLAC or MP3 fixture when selected. Standalone and supplied-SDK
composition, device hotplug and other platforms retain separate acceptance requirements.
