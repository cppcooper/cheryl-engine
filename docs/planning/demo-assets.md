# Demo tile and sprite showcase

The demo needs visible static and animated tile/sprite samples while remaining
usable without externally supplied artwork. This is application presentation using
the existing asset values, playback cursors and retained draw packets; it introduces
no world/entity API. The [demo guide](../../projects/apps/demo/README.md) owns the
resulting controls and asset requirements.

Source implementation is complete; Linux native acceptance remains pending.
Image filtering and atlas isolation discovered during resource review remain
separate [follow-on work](long-term-plan.md#other-engine-extensions); the samples
inherit the current provider policy and supply observations for that contract.

Complete one coherent application unit in this order:

- [x] Identify authored samples and ownership boundaries: Puny World static cells
  and declared tile clips, MiniWorld Swordsman facing/action clips, and a static
  weapon cell. Load on the platform owner and advance only on simulation time.
  Retain separate triangle text and triangle-strip image materials while sharing
  the checked-in shader sources; adopt F5 replacements as one complete pair.
- [x] Load the selected entries independently; isolate missing/malformed manifests,
  absent/unreadable images and invalid image bounds. Make `--full-assets` failure
  nonfatal without changing the Engine loader's contract. Resolve this boundary
  before adding dependent playback or frame submission.
- [x] Add labeled static and animated samples, pause/resume and one-shot attack
  replay. Retain resources in resolved packets in both runtime modes and release
  application-held resources during owner-thread teardown.
- [x] Reconcile the demo guide and shared Linux build/QA queue; review source and
  static checks without configuring, compiling, testing or launching the project.
- [ ] Accept native appearance, timing, controls, partial/missing artwork startup
  and shutdown through user-run Linux/X11
  [TR12](../testing-requests.md#tr12-qa-demo-tiles-and-sprites), reusing the accepted
  `build/testing-native-linux` build with both UI adapters and HID disabled.

**Acceptance:** available samples render upright with intact colors/transparency;
static samples stay fixed while authored clips advance, pause and replay correctly.
Missing or broken optional artwork never prevents the remaining demo from starting
or operating, including `--full-assets`. Sequential/concurrent modes retain the
same simulation-owned playback and immutable-frame behavior. Empty-artwork startup
does not accept present-artwork rendering. Checked-in bootstrap shaders and selected
UI resources retain their existing requirements. Other platforms remain deferred.
