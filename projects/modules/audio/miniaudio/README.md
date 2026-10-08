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

File stream opening waits for initial readiness; subsequent decode-ahead uses the
SDK's resource-manager worker. Keep the file and its contents stable until playback
ends. Restart prepares a fresh node and cursor to discard processing caches;
for streams it reopens the file. Preparation failure preserves the previous voice.
A native start failure leaves it stopped. Control changes take effect at backend
processing boundaries and already queued device output cannot be withdrawn.

## Acceptance boundaries

`tests-audio-miniaudio` owns offline PCM, mixing/control, owned-source, file decode,
short-stream and lifetime regressions. `all-audio-miniaudio` and the root `all-tests`
reuse those case objects. `consumer-module-audio-miniaudio` supplies an independent
audio consumer and SDK-free first-include probe. None requires a display or device.

The consumer defaults to `--offline`. Its `--device` mode opens native output and
generates its own 20-second stereo WAV: left/right alternate each second, and pitch
steps through 220/440/660/880 Hz. It requires no external media. `--producers` adds
a background sound-effect producer while terminal commands control the stream;
maintenance continues while stdin waits. `--stream=FILE` selects another stable
WAV/FLAC/MP3 source. The consumer settles its producer before closing output and
then observes a handle surviving destruction.

| Command | Action |
| --- | --- |
| `p`, `r` | Pause and resume music. |
| `s`, `x` | Stop and restart from the beginning. |
| `l` | Toggle looping. |
| `e` | Submit three overlapping short effects without retaining handles. |
| `v VALUE`, `m VALUE` | Music or master volume, in `[0, 1]`. |
| `t`, `q` | Observe state or close output. |

Linux automation and audible native/long-stream acceptance remain pending in the
[testing queue](../../../../docs/testing-requests.md). Short-stream offline cases
use an initially buffered WAV and do not establish sustained decode-ahead, native
latency, audible channel routing or device shutdown. Owned
[codec fixtures](tests/fixtures/README.md) cover whole-clip FLAC/MP3 decoding;
the native QA baseline uses a long WAV and does not establish compressed-stream
seek/loop behavior. Supplied-SDK and standalone configurations remain unaccepted
composition variants until explicitly selected and built. Other platforms remain deferred under the
[platform plan](../../../../docs/planning/platform-acceptance.md).
