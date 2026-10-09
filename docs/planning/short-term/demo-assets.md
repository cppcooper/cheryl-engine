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
  [TR12](../../testing-requests.md#tr12-qa-demo-tiles-and-sprites), using the updated
  native Linux/X11 demo with both UI adapters and HID disabled. The queue's shared
  preparation refreshes it after startup changes.

TR12 owns appearance, playback, partial-artwork and shader-replacement observations.
Its sampling observations inform the later
[filtering/atlas-isolation contract](../long-term/README.md#other-engine-extensions).
