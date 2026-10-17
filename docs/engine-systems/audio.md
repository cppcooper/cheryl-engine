# Audio

## Purpose

Audio plays independent voices from immutable clips or streamed files. Playback
has its own output clock and application-owned lifetime rather than depending
on render-frame publication or a runtime context adapter.

## Interface

Types below are in `CE::Audio` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`Clip <CE::Audio::Clip>`, {cpp:struct}`Format <CE::Audio::Format>`<br>Own immutable interleaved float PCM and source rate/channel metadata through `from_samples()`. [Clips and voices](../runtime/audio.md#clips-and-voices). |
| {cpp:class}`System <CE::Audio::System>`<br>Play clips/streams, set master volume, maintain retired voices and close output. [Lifetime and scheduling](../runtime/audio.md#lifetime-and-scheduling); [adapter catalogue](../architecture/interfaces-adapters.md#integrations-alongside-the-runtime). |
| {cpp:class}`Voice <CE::Audio::Voice>`, {cpp:struct}`VoiceSnapshot <CE::Audio::VoiceSnapshot>`<br>Control independent playback and obtain a copied state/volume/loop/duration observation. [Playback controls](../runtime/audio.md#clips-and-voices). |
| {cpp:class}`Miniaudio::System <CE::Audio::Miniaudio::System>`<br>Select device or offline output; use module decoding for file clips. [miniaudio setup](../modules/miniaudio.md#preparation-and-output). |

### Playback values and choices

| Type or choice | Purpose and details |
| --- | --- |
| {cpp:struct}`PlaybackOptions <CE::Audio::PlaybackOptions>` | Initial normalized volume, looping and paused state per voice. [Clip/stream submission](../runtime/audio.md#clips-and-voices). |
| {cpp:enum}`PlaybackState <CE::Audio::PlaybackState>` | `Playing`, `Paused`, `Finished`, `Stopped`, `Closed`; stopped/finished voices require explicit restart. [State transitions and closed handles](../runtime/audio.md#clips-and-voices). |
| Device / offline output | Device output advances independently; offline output advances only through explicit `render()` calls. [Backend selection](../runtime/audio.md#backends-and-limits); [miniaudio preparation](../modules/miniaudio.md#preparation-and-output). |
| Clip / streamed file | Retained decoded samples or file-backed playback whose source must remain available. [Sources and limits](../runtime/audio.md#clips-and-voices). |

## Usage

### Uses

The selected module owns mixing, decoding and output. Workers are an optional
place for synchronous decode/file preparation; playback advances on the audio
output clock.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Audio::Clip`<br>{cpp:struct}`CE::Audio::Format`<br>{cpp:struct}`CE::Audio::PlaybackOptions`<br>[`std::filesystem::path`](../runtime/audio.md#clips-and-voices) | [Workers](workers.md) |

### Used by

Applications start sound effects and manage music voices. Voices can share a
clip with independent cursors. Runtime hooks provide useful lifecycle
boundaries, but the application closes audio explicitly; audio-only programs
need no display or graphics.

| Types | Systems |
| --- | --- |
| [Application-owned sound/music controllers](../runtime/audio.md#clips-and-voices) | — |

## Contracts

Clips require complete nonempty finite mono/stereo PCM at supported source
rates. Volume is finite normalized linear gain in `[0, 1]`; mixing loud sources
can still exceed nominal amplitude. A stream file must remain available through
playback. Device opening, decode and initial stream opening can block.

Dropping a voice handle does not stop active playback. Stop and natural finish
require explicit restart; toggling looping alone does not restart them.
Snapshots observe playback rather than freezing its native cursor.

## Lifecycle

Create output before submitting voices. Retain handles for voices needing later
control and call `maintain()` regularly to reclaim eligible unreferenced voices.
Maintenance does not advance audio time. Settle producers before system
destruction; idempotent `close()` stops output and retires voices before native
state. Surviving handles report `Closed` and reject controls. Construct a new
system for another session.

## Execution

Controls, submissions, snapshots and maintenance synchronize application
producers. Playback advances independently on the backend clock, without calling
game callbacks from mixing. Runtime concurrency does not change the audio
ownership contract.

## Integration

With an application-owned `audio` system and previously prepared `clip`:

```cpp
auto voice = audio.play(clip, {.volume = 0.5f});
// Keep voice if the game needs to pause, stop or change this playback.
audio.maintain();
```

Create the system and decode clips at loading boundaries. Trigger prepared clips
from game state, then close output only after all producers stop.

## Limits and further reading

Offline processing is explicit; an unavailable device must not silently succeed
inaudibly. Spatial/world/collision behavior belongs to game code or another
selected capability. Read [audio ownership](../runtime/audio.md),
[miniaudio setup and coverage](../modules/miniaudio.md) and
[the System/Voice interface](../../projects/engine/include/cheryl/audio/system.h).
