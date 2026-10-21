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
| {cpp:struct}`PlaybackEnded <CE::Audio::PlaybackEnded>`, {cpp:struct}`PlaybackNotifications <CE::Audio::PlaybackNotifications>`<br>Observe logical generation outcomes through an explicit owner pump and reconcile retained current state after bounded loss. [Playback outcomes](../runtime/audio.md#playback-outcomes); [runtime pump](../modules/miniaudio.md#runtime-notification-pump). |

### Playback values and choices

| Type or choice | Purpose and details |
| --- | --- |
| {cpp:struct}`PlaybackOptions <CE::Audio::PlaybackOptions>` | Initial normalized volume, looping and paused state per voice. [Clip/stream submission](../runtime/audio.md#clips-and-voices). |
| {cpp:enum}`PlaybackState <CE::Audio::PlaybackState>` | `Playing`, `Paused`, `Finished`, `Stopped`, `Closed`; stopped/finished voices require explicit restart. [State transitions and closed handles](../runtime/audio.md#clips-and-voices). |
| Device / offline output | Device output advances independently; offline output advances only through explicit `render()` calls. [Backend selection](../runtime/audio.md#backends-and-limits); [miniaudio preparation](../modules/miniaudio.md#preparation-and-output). |
| Clip / streamed file | Retained decoded samples or file-backed playback whose source must remain available. [Sources and limits](../runtime/audio.md#clips-and-voices). |

Use a decoded clip when several effects should share retained samples without
reopening a file. Stream longer tracks when retaining the whole decoded source
is undesirable; keep that file available during playback. The
[miniaudio preparation example](../modules/miniaudio.md#preparation-and-output)
shows both entry points.

## Usage

Own one selected audio system alongside the game, prepare clips at loading
boundaries and retain voice handles for playback needing later control. Audio
progresses on its output clock; its optional event pump is a separate owner task.

```{engine-diagram} audio-application
```

The runtime maintenance attachment in this map is optional. Audio-only programs
need no Context, display or renderer. The [engine dependency map](diagram-explorer.md#audio-internal)
shows the neutral contracts and miniaudio implementation boundary.

### Uses

| Consumer | Dependency | Integration boundary |
| --- | --- | --- |
| {cpp:class}`Audio::System <CE::Audio::System>` playback | {cpp:class}`Audio::Clip <CE::Audio::Clip>`, `Audio::Format` and `Audio::PlaybackOptions`, or a streamed file | Retain immutable PCM for independent voices; keep stream files available through playback and restart. The selected module owns decoding/mixing/output. |
| Outcome publication | [Events](events.md) {cpp:class}`Events::EventBus::Publisher <CE::Events::EventBus::Publisher>` | The designated owner explicitly pumps publication outside audio locks. Immediate listeners run there; queued listeners use their selected delivery adapter. |
| Miniaudio `pump_with()` | [Runtime](runtime.md) {cpp:class}`Runtime::RuntimeServices <CE::Runtime::RuntimeServices>` and {cpp:class}`Runtime::ServiceAttachment <CE::Runtime::ServiceAttachment>` | Optional platform-maintenance notification pump. It retains weak audio access and does not own output or voice reclamation. |

[Workers](workers.md) can run application-owned CPU file decoding. `Audio::System`
does not depend on an Engine worker pool, and playback does not wait for game
updates or render-frame publication.

### Used by

| Consumer | Audio value/operation | Responsibility |
| --- | --- | --- |
| Application sound/music controller | `System::play()` / `stream()` and retained {cpp:class}`Audio::Voice <CE::Audio::Voice>` handles | Trigger prepared sources, control individual voices and explicitly close output after producers stop. Dropping a handle does not stop active playback. |
| Application state/UI observation | {cpp:struct}`Audio::VoiceSnapshot <CE::Audio::VoiceSnapshot>` and optional {cpp:struct}`Audio::PlaybackEnded <CE::Audio::PlaybackEnded>` | Observe copied state or logical generation outcomes; select simulation delivery when a platform pump must reach simulation-owned state. |
| Offline processors and tests | `Audio::Miniaudio::System::open_offline()` and `render()` | Explicitly advance the mixer clock with complete output frames; no native device opens. [Output selection](../modules/miniaudio.md#preparation-and-output). |

### Playback, maintenance and notifications

| Operation | What it advances or releases | What the application must arrange |
| --- | --- | --- |
| Device output / offline `render()` | Audio playback time. | Choose real device or explicit offline output; opening and initial source preparation can block. |
| `maintain()` | Reclaims unreferenced inactive voices. | Call regularly while open. It neither advances audio time nor publishes callbacks. |
| `publish_notifications()` | Observes terminal facts and attempts pending event publication. | Call on the designated owner, or retain the optional `pump_with()` attachment. The attachment does not call `maintain()`. |
| `close()` | Stops output and retires voices; surviving handles report `Closed`. | Settle producers first. Pump final outcomes explicitly before removing subscriptions and releasing the system. |

### Observe playback outcomes

The optional `Audio::playback_ended_event` reports one logical terminal fact per
admitted generation: `Completed`, `Stopped`, `Replaced`, `OutputClosed` or
`Reclaimed`. Restart admits another generation while retaining voice identity;
pause/resume and loop iterations do not create generations. Miniaudio supports
this extension; `notification_snapshot().system == 0` means unsupported.

Notifications use bounded storage and one publication attempt per retained
record. Use {cpp:struct}`Audio::PlaybackNotifications <CE::Audio::PlaybackNotifications>`
to detect loss and a retained voice's current `snapshot().terminal` to reconcile
that generation. This does not recover superseded playback history. The
[playback outcome contract](../runtime/audio.md#playback-outcomes) explains
identity, reason selection and pump ownership; the
[miniaudio example](../modules/miniaudio.md#playback-notifications) supplies
subscription setup and removal.

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
[the System/Voice interface](../../../projects/engine/include/cheryl/audio/system.h).
