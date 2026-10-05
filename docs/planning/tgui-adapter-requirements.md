# First TGUI adapter

Use [TGUI 1.13.0](https://github.com/texus/TGUI/releases/tag/v1.13.0) with a
custom Cheryl backend. Its native widget API stays in an optional
`Cheryl::UI::TGUI` owner under `projects/modules/ui/tgui/`. The adapter links
Engine and TGUI; the selected graphics/platform modules belong to the application.
Engine-only selection does not discover TGUI or FreeType.

The initial scope is one window/provider domain and one simulation owner, with
panels, labels, buttons, tooltips, scrolling, images and a committed-text field.
Widget mutation and CPU recording stay on that owner. Native rendering, uploads
and retirement stay on the platform owner through Cheryl's existing dispatcher.
No widget state is shared with render playback.

## Requirements and decisions

| TGUI requirement | Cheryl boundary and implementation decision |
| --- | --- |
| Indexed colored triangles and transforms | Expand indices into owned `Vertex2DColor` triangles and copy transformed positions. Keep toolkit draw order; no Engine indexing or batching API is needed. |
| Rectangular nested clipping and viewport mapping | Record copied logical `ClipRegion2D` values. Use the toolkit's intersected clip/view mapping and Cheryl's framebuffer conversion. Arbitrary rotated masks remain outside the initial scope. |
| Solid and textured materials | The application supplies compatible neutral materials. Require colored triangles, straight alpha and disabled depth/culling; name the texture parameter explicitly. Backend shader construction stays with the graphics module. |
| Image decoding and font textures | TGUI owns CPU decoding; the adapter copies transient RGBA pixels into owned snapshots. Cheryl's provider publishes immutable images. Texture smoothing changes require an explicit supported policy rather than silently changing retained images. |
| Font metrics, glyph rasterization and atlas growth | Use TGUI's FreeType font backend, owned by the UI module. It invalidates its texture when glyph pixels change and creates a replacement on demand. Copy each texture generation before recording draws; earlier frames keep their original image. Cheryl's ASCII STBFont probe does not define this font implementation. |
| Nonblocking upload/publication | Record a complete CPU scene and transfer owned data through `PlatformDispatcher::Submission`. Adopt a completed retained scene on simulation; do not wait for platform work during update or publish a partially uploaded scene. |
| Keys and pointer buttons | Add backend-neutral identifiers to physical records; retain opaque State binding IDs and native/scancode diagnostics. Translate those identifiers to TGUI events inside the adapter, never through GLFW/Gainput headers. |
| Committed text and keyboard focus | Consume routed records for the adapter's focus ID/epoch in observation order. TGUI owns widget focus/editing. Hold Cheryl capture/focus leases while needed; controller gameplay remains available. Text is never inferred from key events. |
| Pointer, wheel and controller navigation | Pointer coordinates and fractional wheel records already exist. Button/wheel records also need observation-time coordinates: a first click may precede any captured move, and a later move cannot relocate it. Initial pointer delivery remains explicitly application-selected; modal suppression, pointer capture and controller-navigation ownership require separate routing acceptance before exposure. TGUI's event interface provides vertical wheel delivery; horizontal offsets remain available in Cheryl's original records. |
| Clipboard, cursor, IME and scale | Expose unavailable services explicitly in adapter capabilities. No private clipboard may masquerade as the OS clipboard. Supply copied logical/framebuffer sizes for view updates; use an explicit font scale until per-window scale reporting exists. IME/preedit and platform cursor changes remain unavailable initially. |
| Toolkit-global state | TGUI has a process-global backend/font/theme/timers. An explicit simulation-owned session installs the backend before widget/font use, rejects an already installed foreign backend and releases toolkit objects before clearing it. Multiple independent TGUI backend sessions are not a supported promise. |
| Texture size and sampling limits | The application supplies the adapter's supported texture bound and sampling policy. The bound is a configured limit, not a fabricated hardware query; provider failures propagate without publishing partial resources. |

The inspected interfaces are TGUI's
[render target](https://github.com/texus/TGUI/blob/v1.13.0/include/TGUI/Backend/Renderer/BackendRenderTarget.hpp),
[texture](https://github.com/texus/TGUI/blob/v1.13.0/include/TGUI/Backend/Renderer/BackendTexture.hpp),
[FreeType implementation](https://github.com/texus/TGUI/blob/v1.13.0/src/Backend/Font/FreeType/BackendFontFreeType.cpp)
and [backend lifetime](https://github.com/texus/TGUI/blob/v1.13.0/src/Backend/Window/Backend.cpp).
The [custom-backend selection guide](https://tgui.eu/tutorials/latest-stable/backends/)
describes upstream's dependency selection. These source links are pinned so a toolkit
upgrade reopens the affected assumptions.

## Ordered implementation and acceptance

Progress belongs to the [U9 checklist](cheryl-ui-integration-plan.md#remaining-development-sequence).
First establish portable button identifiers and their native mapping. Then add the
optional adapter owner and event translation, followed by CPU texture/render
recording, platform upload and toolkit-session lifetime. Add the actual menu and
retained/concurrent acceptance after those foundations are stable.

Reuse supplied `TGUI::TGUI` targets or an explicitly selected dependency source or
package at the pinned version; do not download during configuration. A dependency
source build selects only its custom backend plus FreeType and suppresses upstream
examples, tools and tests. Validate supplied dependency features without rewriting
the host's target or cache. Standalone module composition and an independent
consumer must work with Engine alone and controlled contract adapters.

Module tests own event translation, index expansion, transforms/clipping, texture
copy/replacement, upload failure/cancellation and TGUI's real widget behavior. Engine
tests only establish preservation of portable identifiers through capture/routing
and runtime handoff. Native pixel acceptance reuses the graphics owner's existing
clipping/color proof and adds a real toolkit scene; a recording or skipped native
case does not establish that appearance.

Unicode shaping, grapheme editing, IME, mutable images, multiple domains, offscreen
effects, modal/pointer routing and mixed-toolkit coexistence remain separate work.
The first adapter's scalar-text support must not claim any of those capabilities.
