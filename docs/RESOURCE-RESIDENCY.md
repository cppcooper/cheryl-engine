# Resource residency and maintenance

`AssetCacheContext` is the shared provider/loading-owner guard used by all asset
caches. It allows one active provider domain and one loading owner at a time.
It protects publication and teardown, rather than implementing a cache eviction
policy. Each cache map retains a strong `shared_ptr` until explicit clear,
successful replacement, or provider teardown. A lookup copies another strong
handle under a shared lock. An unused cached asset remains resident; this work
adds no weak cache, LRU, residency budget, or automatic unused-asset eviction.

Provider teardown first marks the domain as releasing, then drops composite
assets, fonts, programs, and images outside cache locks. A reentrant deleter can
inspect a cleared cache but cannot refill it during teardown. When release ends,
a new provider can bind the global domain. Externally retained logical assets
continue to exist independently; their native handles still belong to the old
renderer/context lifetime and cannot be used after that lifetime closes.

The ownership audit follows these paths:

| Resource | Strong logical ownership | Native retirement |
| --- | --- | --- |
| Image/texture | Texture cache, Asset2D, STBFont, application handles | Texture owns one move-only OpenGLHandle. |
| Geometry | Asset2D, STBFont, application handles | VAO owns vertex-array registration plus one 2D VBO; the legacy mesh also owns its index buffer. |
| Linked program | Shader cache, DrawStyle, application handles | GLSLProgram owns one tracked program handle; successful replacement preserves old owners. |
| Compiled stage | Local shader-link guard | Transient checked deletion; stages are not cached or published. |
| Font atlas/glyphs | Font cache and STBFont's image/geometry composition | Texture/VAO final-owner retirement; layout metadata has independent CPU ownership. |
| Sprite/tileset/graphic | Strong cache or application handle; const Asset2D image/geometry handles | Composite release drops constituent owners; no second native deleter. |
| Published frame | Each command retains its asset/font and shader handles | Platform recycling releases the frame's owners; other owners can keep resources resident. |
| Pipeline/material | Current DrawStyle retains Shader; the separate task-7 contracts remain pending | Future material resources must retain these same logical generations and old context domains. |

`OpenGLHandle` registration is move-only. Destruction from any thread marks its
registration pending; it never calls GL. Duplicate pending retirement is ignored,
and collected registration slots can be reused only after deletion. Renderer
shutdown deletes all remaining tracked IDs, including resources with external
logical owners, and closes the lifetime. A later release does not delete again.
Using a closed handle fails before querying its borrowed context.

`iRenderer::maintain_resources()` is the backend-neutral platform maintenance
operation. OpenGL implements it with the context-owned retirement collector.
Backends without deferred resource release implement a no-op. Both runtime modes
call it after platform/render/recycle work and before waiting, including when no
simulation update or first frame is available. It also runs while accepted CPU
and simulation work settle, and after final frame/game releases. Partial renderer
initialization does not count as ready for maintenance. Clear/render validate
their context but no longer duplicate collection.

Idle platform waits are capped at 10 ms. A resource retired just after collection
will therefore receive another maintenance opportunity without a new frame,
poll, or explicit retirement notifier. A full lockstep/finite polling backlog
does not disable this bound. This bounds the **wait**, rather than promising a
10 ms deletion deadline: a blocked presentation, long sequential update, platform
callback, or OS scheduling delay can still postpone owner-thread maintenance.
Concurrent simulation stays on its own timing deadline; these platform wakeups
do not synthesize early simulation updates or consume input themselves.

Tracking, use, collection, shutdown, and untracked failure cleanup require the
native owner and its actual current context. A resource from another backend is
rejected by the existing geometry binding check; a resource from another native
context fails its own lifetime guard. One context being current does not make
another context's IDs valid. These guards are preserved for task 7's richer
material/geometry domain validation.

Native creation guards retain an untracked ID until registration succeeds.
Program linking then transfers ownership to an OpenGLHandle before constructing
the logical GLSLProgram, so later allocation failure has only one retirement
owner. Logical program adoption validates that this registration is a program,
preserving retirement when another native resource kind is rejected.
Texture/VAO/buffer failure guards and program/stage guards use
`discard_untracked()`: it checks owner/current context and cannot replace the
original failure. If the context is unavailable, it leaves native cleanup to
platform context destruction. Renderer destruction similarly uses `abandon()`
when context recovery fails, invalidating logical handles without GL calls.

Prepared recording-native regressions cover worker-thread final release,
exactly-once collection/shutdown, retained handles after closure, moved/reused
registrations, wrong-context collection, and guarded untracked cleanup. Recording
runtime adapters cover maintenance before a first frame with a full backlog and
first-error preservation. Strong-residency/cache-domain scenarios cover explicit
clear and retained owners across provider rebind. These sources are uncompiled
and unexecuted; real context-loss and GPU acceptance remain open.
