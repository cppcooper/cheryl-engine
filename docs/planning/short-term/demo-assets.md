# Demo tile and sprite showcase

The demo uses existing asset values, simulation-owned playback and retained draw
packets to show static/animated samples without requiring external artwork for
startup. It introduces no world/entity API. The [demo guide](../../../projects/apps/demo/README.md#tile-and-sprite-samples)
owns controls, selected images and the implemented application flow.

## Progress

- [x] Select Puny World cells/clips, MiniWorld Swordsman action/facing clips and a
  weapon cell; keep image loading on platform and playback on simulation.
- [x] Isolate each optional manifest/image/bounds failure and make `--full-assets`
  failure nonfatal before adding dependent playback/submission.
- [x] Add labeled static/animated samples, pause/resume and attack replay, retaining
  resolved resources in both modes and releasing them during owner teardown.
- [x] Document controls/assets and add shared Linux build/QA procedures.
- [ ] Accept appearance, timing, controls, missing/partial artwork and shutdown in
  [TR12](../../testing-requests.md#tr12-qa-demo-tiles-and-sprites), reusing the matching
  native Linux/X11 build with both UI adapters and HID disabled.

## Remaining acceptance

Available samples must render upright with intact colors/transparency; static
samples remain fixed while declared clips advance, pause and replay correctly.
Absent/broken optional artwork must leave the remaining demo operational, including
`--full-assets`. Observe shader-pair replacement and failure retention while samples
are active. Sequential variable and concurrent fixed modes retain simulation-owned
playback and immutable frames. Empty-artwork startup does not accept rendering.

The current provider supplies linear magnification and mipmaps. Selectable filtering
and atlas isolation remain a [consumer-driven follow-on](../long-term/README.md#other-engine-extensions);
these observations establish the baseline for that contract. Bootstrap shaders and
selected UI resources retain their requirements. Other platforms remain deferred.
