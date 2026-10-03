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
| Linked program | Shader cache, GLSLPipeline, application handles | GLSLProgram owns one tracked program handle; successful replacement preserves old owners. |
| Compiled stage | Local shader-link guard and temporary program attachment | Mark for deletion after compilation; detach every stage after linking so retained programs do not retain stages. Failure destroys the guarded program and its remaining attachments. |
| Font atlas/glyphs | Font cache and STBFont's image/geometry composition | Texture/VAO final-owner retirement; layout metadata has independent CPU ownership. |
| Sprite/tileset/graphic | Strong cache or application handle; const Asset2D image/geometry handles | Composite release drops constituent owners; no second native deleter. |
| Published frame | Each packet retains geometry/material and copied image parameter handles; pass parameters retain their own images | Platform recycling releases the frame's owners; other owners can keep resources resident. |
| Pipeline/material | GLSLPipeline retains its program and definition/default image values; Material retains its pipeline and copied image defaults; resolved parameter sets retain image handles. Frames retain immutable material generations. | Native handles retire through the same program/image owners. Successful replacement preserves old frame owners; failed reload preserves the cache entry. |

`OpenGLHandle` registration is move-only. Destruction from any thread marks its
registration pending; it never calls GL. Duplicate pending retirement is ignored,
and collected registration slots can be reused only after deletion. Renderer
shutdown deletes all remaining tracked IDs, including resources with external
logical owners, and closes the lifetime. A later release does not delete again.
Using a closed handle fails before querying its borrowed context.

`iRenderer::maintain_resources()` is the backend-neutral platform maintenance
operation. OpenGL validates the selected current context, unbinds its last program,
then runs the context-owned retirement collector. A deleted current program stays
alive until it is unbound; this also happens before the shutdown sweep while a
borrowed window/context may remain alive. Subsequent draws select their own program.
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
rejected by pipeline/geometry/image domain checks before binding; another native
context's resources are rejected as well. One context being current does not make
another context's IDs valid. Native pipeline/geometry/image domain validation preserves these guards.

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

Texture and buffer constructors reject pending native errors before generating IDs
and check generation, storage upload, mipmap generation and vertex-layout setup
before publishing a logical resource. Errors include their native code. Failed
untracked creation uses guarded discard; tracked IDs retire during constructor
unwinding. An atlas restores unpack alignment before reporting upload failure and
skips dependent mipmap generation. This detects reported native failures without
promising recovery from memory exhaustion or context loss. OpenGL leaves native
state undefined after OUT_OF_MEMORY; real-driver acceptance remains separate.

Cache insertion retains a local candidate owner across fallible node construction
and rehash. A failed or duplicate insertion can release its final candidate only
after the cache write lock unwinds. Replacement exchanges an already established
slot, and clear detaches the whole map before release. This includes material
candidates and their retained program/image resources; reentrant deleters remain
outside publication locks on all three paths.

`Texture::unbind(unit)` selects the supplied unit after checking the texture's
owner/current/live context and unit limit. It does not depend on an inherited active
unit.

If shutdown cannot obtain the current context, pending/live registrations remain
available for owner recovery and an exactly-once sweep. If recovery fails during
renderer destruction, `abandon()` invalidates every registration without a native
delete or later query of the destroyed context. Late foreign-thread release is safe
for those invalidated handles.

Recorded validation covers strong residency, cache-domain rebinding, idle maintenance,
recording failure/recovery/abandonment, real selected-context restoration, retained
frame pixels and native deletion before window destruction. Hardware context loss
and reset recovery remain outside those executed scopes. See
[architecture-validation.md](../development/architecture-validation.md).
