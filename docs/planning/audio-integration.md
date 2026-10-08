# Audio integration

## Scope and boundaries

Audio is selected for short-term engine development. World storage, entities,
collision and game mechanics remain application work: a game can consume Cheryl
as a Git submodule and supply its own optional modules from the start. This task
does not introduce those facilities or require a later extraction from game code.

The initial audio scope is owned decoded clips, overlapping sound effects,
streamed file playback for music, looping, pause/resume/stop, restart, and per-voice
and master volume. Playback progresses independently of rendered frames and
simulation ticks. Device playback and deterministic offline mixing have explicit
selection; an unavailable device must not silently select inaudible output.

The public Engine contract contains no audio SDK types. The selected optional
module owns decoding, native output, mixing and its private dependency. This gives
applications a concrete omit/replace benefit and permits device-free acceptance
without a display or graphics owner. Engine does not depend on the module.

The selected first backend is miniaudio 0.11.25, pinned at
`9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`, for its source availability,
permissive license and loading/mixing/streaming support. FMOD remains a later
optional backend. Its Studio event/bank authoring is a separate capability from
ordinary clip playback and needs its own consumer contract rather than a promise
of interchangeable authoring formats.

## Ownership and concurrency

Decoded clips own immutable interleaved finite floating-point mono/stereo samples
at 8–192 kHz. A voice has an independent cursor even when it shares a clip.
Controls are synchronized for platform/simulation producers. Native callback code
does not call game code, log through game callbacks, or borrow game state.

The application owns the audio system separately from the display/render context.
It creates output before playback, stops producers before shutdown, and closes
audio after those producers settle. Closing stops output and releases native voice
resources before the mixer/device. Surviving handles report closure and cannot
access released native state. Dropping a handle permits a playing one-shot to
finish; regular maintenance reclaims completed, unreferenced voices.

File decode and initial stream opening are synchronous preparation work; the
application can dispatch owned decode requests through existing worker groups.
Playback/control calls do not require platform dispatch. Their ordering is the
order admitted by the system's control lock, not a deterministic simulation clock.

## Ordered implementation

- [x] Settle the backend dependency, initial PCM limits, voice state transitions,
  lifetime/concurrency contract and module-selection boundary.
- [x] Add immutable clips and backend-neutral playback/system contracts, focused
  validation regressions and Engine first-include probes.
- [x] Add the selected optional backend with pinned dependency provenance, private
  SDK implementation, standalone/root composition and owner-local tests.
  - [x] Implement CPU file decode, clip voices and explicit offline mixing.
  - [x] Implement native playback, streamed voices and synchronized controls.
  - [x] Cover failure cleanup, independent cursors, retained one-shots and shutdown
    with handles surviving their system through source regressions.
- [x] Add an independent consumer/observation harness for native audio and stream
  QA; document application lifecycle and dependency selection.
- [x] Reconcile the testing queue with batched Linux automation and audible QA.
- [ ] Accept Linux automation and native sound observations through user-run work.
  - [x] Accept neutral clip/header coverage in the Engine-only assembly and the
    root audio owner's WAV/FLAC/MP3 decode cases.
  - [x] Accept corrected offline mixing/control/stream cases, empty/retired graph
    silence and consumer/header checks in the Linux Release root audio assembly.
  - [ ] Accept audible native output and long-stream observations through
    [TR11](../testing-requests.md#tr11-qa-native-audio-and-streaming).

At the contract boundary, revise dependent work if the chosen backend cannot
preserve ownership or control semantics. At the offline/native boundary, do not
count a successful device-free mixer as hardware output acceptance. Streaming
acceptance requires a file longer than its decode-ahead window and actual end,
restart and loop observations; a short cached clip does not establish it.
The native baseline uses the consumer's owned 20-second WAV. Whole-clip FLAC/MP3
regressions use owned short fixtures; compressed-file streaming/seek/loop coverage
requires a stable long fixture when selected. Standalone and supplied-SDK
configuration acceptance also remains separate from the initial root assembly.

Spatial audio, effects graphs, capture, device enumeration/hotplug, custom codecs,
Studio-style authoring and all additional platform acceptance remain later work.
Builds and executable tests require explicit authorization under
[AGENTS.md](../../AGENTS.md); source completion does not establish acceptance.
