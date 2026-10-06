# RmlUi adapter requirements

RmlUi is the selected second player-facing adapter for U9. The initial dependency
is [RmlUi 6.3](https://github.com/mikke89/RmlUi/releases/tag/6.3), commit
`ba95ffe8bfb6370efb2cdcca927eaad4710c5413`. The adapter links `RmlUi::Core` and
`Cheryl::Engine`, independently of TGUI and native graphics/input modules.

Progress belongs to the [U9 checklist](cheryl-ui-integration-plan.md#remaining-development-sequence).
The [adapter-author guide](../development/ui-adapters.md) owns the engine boundaries.

## Initial scope and decisions

| Requirement | Decision |
| --- | --- |
| Authoring | Expose the native context, RML documents and RCSS styling. The adapter does not parse a second layout language or define common widgets. |
| Dependency | Pin 6.3, use the stock FreeType font engine, and select only Core. Reuse an existing target, explicit source or exact package. Suppress samples, native backends, Lua and optional plugins for an owned source configuration. |
| Rendering | Copy compiled indexed geometry into owned colored triangles. Preserve authored draw order, translation, and rectangular scissor regions in logical coordinates. |
| Alpha | Preserve RmlUi's premultiplied vertex colors and generated RGBA. Application materials use Cheryl's existing `PremultipliedAlpha` mode, disabled depth/write/culling and the projection semantic. |
| Images | Copy generated textures into immutable decoded generations. Premultiply decoded file images before recording. Provider sampling is smoothed and clamp-to-edge; repeating sampler behavior is outside the initial proof. |
| Fonts | Use toolkit FreeType rasterization, then copy each generated atlas before publishing it through the platform provider. Native file loading owns font bytes; memory loading requires a session-owned copy through shutdown. A session texture bound is at least 1024, matching the stock atlas's maximum dimension. |
| Input | Translate portable physical identities, repeats and committed Unicode. Button/scroll delivery first applies its observation-time position. Keyboard/text use target and epoch routing; the caller selects pointer delivery. |
| Time and view | Supply simulation time and copied logical/framebuffer sizes. Context dimensions use logical units; framebuffer ratios resolve native clips through Cheryl. No toolkit window loop or native window query. |
| Lifecycle | Own one RmlUi global session on the UI/simulation owner, with native contexts and interfaces destroyed before their storage. Final transfer to platform follows simulation join and release of external element references. |
| Optional OS services | Report clipboard, cursor and IME as unavailable. Suppress unsupported clipboard shortcuts before they alter a selection. |
| Advanced rendering | Report masks, layers, filters, custom shaders and offscreen effects as unsupported. Reject their use before publishing an incomplete replacement. Transform support must be scoped against the same clipping limits. |

The selected upstream render interface requires compiled geometry, texture handles
and premultiplied output; it does not require immediate graphics calls. Existing
Engine pipelines, resources, clipping, dispatcher and focus contracts cover the
initial proof. Introduce a new generic seam only if implementation demonstrates a
missing contract before dependent code grows.

## Discovery boundaries

- Check source/package feature metadata before constructing toolkit globals. An
  incompatible version or font configuration must fail explicitly; do not rewrite
  a supplied target or silently use a native RmlUi backend.
- Verify image orientation and alpha with controlled colored pixels and real font
  output before native integration depends on them.
- Exercise toolkit teardown with a retained recording and uploaded frame before
  adding the demo; borrowed geometry, font bytes and pixels must not escape.
- Keep coexistence in a cross-project test owner. Neither adapter's library or
  standalone checks depend on the other. Feed both the same immutable records,
  verify focus preemption, and retain both scenes through independent teardown.

**Acceptance:** independently configure/consume the module and its first-include
headers; accept module-owned recording, input, image/font, publication and real
sequential/concurrent runtime checks; inspect native output and input through the
selected backend; then accept the TGUI/RmlUi coexistence proof. Keep unavailable
effects, services and platform coverage explicit.
