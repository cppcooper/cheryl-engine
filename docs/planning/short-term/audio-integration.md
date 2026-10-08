# Audio integration

The selected first backend is miniaudio 0.11.25 for source availability, permissive
licensing and decode/mixing/streaming support. The neutral [audio contract](../../runtime/audio.md)
owns clip/voice/system semantics; the [module guide](../../../projects/modules/audio/miniaudio/README.md)
owns dependency selection, device/offline modes and application integration.

This task covers ordinary clips, overlapping effects, streamed music, looping,
pause/resume/stop/restart and volume. Playback progresses independently of simulation
and rendering. Device selection is explicit; an unavailable device must fail rather
than silently choosing offline output. World/entity/collision organization belongs
to the application.

## Progress

- [x] Settle backend, PCM limits, voice transitions, concurrency and owner lifetimes.
- [x] Add immutable neutral clips/playback contracts, regressions and header probes.
- [x] Add the optional backend, pinned private SDK and standalone/root composition.
  - [x] Implement decode, clip voices and explicit offline mixing.
  - [x] Implement native playback, streams and synchronized controls.
  - [x] Cover independent cursors, retained one-shots, failure cleanup and surviving handles.
- [x] Add the independent native/stream observation consumer and lifecycle guidance.
- [x] Add batched automation and audible QA to the testing queue.
- [ ] Complete Linux native acceptance.
  - [x] Accept neutral clip/header and root WAV/FLAC/MP3 decode coverage.
  - [x] Accept corrected offline mixing/control/short-stream, empty/retired silence
    and consumer/header coverage in the Linux Release root assembly.
  - [ ] Accept audible output and long-stream observations through
    [TR11](../../testing-requests.md#tr11-qa-native-audio-and-streaming).

## Remaining acceptance and boundaries

Use the consumer's owned 20-second stereo WAV, longer than its decode-ahead window.
Observe actual channel routing, continuing playback while stdin is idle, controls,
end/restart/loop, overlapping discarded-handle effects and shutdown with producers.
Device-free mixing and short cached streams do not establish those observations.
Compressed-stream seek/loop needs a stable long fixture when selected; standalone,
supplied-SDK and additional platform variants retain separate coverage limits.

Spatial audio, effects graphs, capture, device enumeration/hotplug and custom codecs
remain later consumer-selected work. A future FMOD backend can implement ordinary
playback; Studio event/bank authoring requires a separate capability and deployment
scope in the [long-term plan](../long-term/README.md#other-engine-extensions).
