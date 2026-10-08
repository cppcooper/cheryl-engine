# Unfinished engine work

This is the concise list of unresolved engine facilities. Completed implementation
history belongs in Git and durable contracts belong in their subject documents.
Current roadmap sequencing is in
[develop-review-and-development-plan.md](develop-review-and-development-plan.md).
Unresolved artwork metadata is tracked separately in
[asset-manifest-todo.md](asset-manifest-todo.md).

## Validation and integration

- Provide Debug graphical applications with a native terminal window streaming
  engine/game/module logs and stdout/stderr. Preserve the existing terminal connection
  for CTest and GoogleTest reports. Close the terminal after normal shutdown and
  retain it after a crash; manually closing it leaves the application running.
  Non-Debug builds keep their existing output destinations. The
  [console plan](debug-console.md) owns remaining transport design and development.
- Gainput backend implementation and HID-specific QA remain deferred at the code
  baseline; Linux joystick automation and Bluetooth DualSense QA with HID disabled
  are accepted. Outstanding prerequisites are tracked in
  [HID integration](develop-review-and-development-plan.md#deferred-hid-integration).
- Windows native ownership, resize/typed-event, desktop and controller acceptance,
  Windows HID notifications and macOS/Wayland/other-platform coverage are shelved in
  the [long-term platform plan](platform-acceptance.md). Linux/X11 typed-event and
  resize acceptance is complete; its durable contracts remain in the subject guides.

## Gameplay and presentation facilities

- Accept the demo's optional static/animated tile and sprite showcase, including
  missing/partial-artwork startup, simulation-owned playback and retained rendering.
  The [demo plan](demo-assets.md) owns source progress and Linux acceptance.
- Accept audible native miniaudio output and long-stream playback; neutral owned
  clips and root-assembly decode/mixing/control/short-stream automation are accepted. The
  [audio plan](audio-integration.md) owns remaining Linux observations; FMOD
  remains a later optional backend.
- Accept native accented Latin/Cyrillic, bidi, width-constrained wrapping, shaped
  glyph resources and demo replacement observations. CPU decoder/scalar fallback,
  owned font selection/layout and resource/submission regressions are accepted;
  their contracts are in [text encoding](../assets/text-encoding.md) and
  [Unicode layout](../assets/text-layout.md). Remaining observations are tracked in
  [U11](develop-review-and-development-plan.md#u11--unicode-text-layout-and-glyph-resources).
  IME/editing is a separate consumer requirement unless selected UI work needs it.
- Add the engine's built-in graphical developer console in future work, including
  an in-app output destination for Release. Its UI and command requirements belong
  to the [long-term plan](long-term-plan.md#other-engine-extensions).
- Add broader pointer capture, modal/controller routing or optional platform services
  only for an explicit consumer contract. Current adapters cover rectangular clipping,
  colored geometry and routed committed text; their
  [capability boundaries](../development/ui-adapters.md#routing-and-unavailable-services)
  define what additional requirements must establish before exposure.

## Optional and measured extensions

- Derive complete render compatibility keys and explicit reorder-safe regions before
  batching or sorting. Authored packet order remains the baseline.
- Add a profiling-based timing adviser only from measured workload data; it must not
  silently replace explicit fixed-step, input-retention or recovery policy.
- Treat 3D, topology/NUMA adapters, networking, world/entity/physics,
  serialization, additional graphics backends and broader platform/device support as
  separate consumer-driven work in the [long-term plan](long-term-plan.md), alongside
  deferred Steam API/Input and optional input composition.
