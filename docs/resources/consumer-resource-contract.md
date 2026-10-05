# Consumer resource contract

U8's selected consumer is U0's library-neutral UI submission/input probe. It uses
one active cache/provider and rendering window, immutable CPU-created images,
expanded triangles, and the existing printable-ASCII font atlas. These choices
require explicit resource contracts, rather than new domains or mutable textures.

| Requirement | Selected treatment |
| --- | --- |
| Dynamic image creation | ResourceProvider::create_image already copies owned RGBA pixels into a new backend handle. |
| Changed image/geometry | Create another immutable handle and publish a replacement material/frame. Retained old frames keep their selected resources. |
| Vertex colors | Upload `Vertex2DColor` with float RGBA and select `Position3UV2Color4` in the pipeline. An unimplemented provider overload rejects the request explicitly; the existing `Vertex2D` path keeps its layout. |
| Atomic batch reload | Not required. Loader upload can partially publish entries; its metadata pointer commits only after successful upload/allocation. |
| Multiple domains/windows | Not required. Global managers support one active provider and serialized loading owner. Separate roots/loaders share that domain. |
| In-place texture update or atlas growth | Not exposed. Reopen U8/U11 before a consumer requires mutation or Unicode glyph residency. |
| Eviction/budgets | No automatic policy. Strong cache residency and explicit clear/replacement/teardown remain. |

PreparedAssets owns its decoded pixels and definitions and retains no provider or
native handle. Preparation can run on a worker and fails before cache publication.
Upload consumes a value, copies transient data before returning, and obeys provider
thread/context affinity. Moving the value into an upload request consumes the
caller's ownership; retry requires a fresh prepare or a preserved copy. Pending
platform work can cancel before execution and release its captures/future as
documented by PlatformDispatcher. An executing upload has no rollback or mid-batch
cancellation contract.

Each newly created cache entry can be visible before the next entry is created.
If upload fails, completed entries remain and the last successful metadata snapshot
remains. Even allocation of the final snapshot can fail after all entries exist.
Retry skips existing keys instead of replacing them. Submitted manifests therefore
describe submitted definitions, not an atomic view of all current cache contents;
changed files and the same key do not turn Loader into a hot-reload transaction.
Existing per-key material/program replacement builds a complete candidate before
publication and preserves old retained generations; it does not imply batch reload.

Published frames/materials must retain selected image/geometry/program handles.
Replacement releases the cache's old owner while those handles remain safe within
their original live backend lifetime. OpenGL final logical release queues native
retirement; the owner/context collects it during maintenance. Renderer shutdown
sweeps remaining native registrations, including externally retained logical assets.
Logical handles can survive shutdown for destruction/CPU metadata inspection, but
cannot bind/draw through a closed domain or migrate to a new provider.

## Accounting boundary

Cache entry counts are observable, but are not physical residency bytes. Images can
be shared by many sprites/materials/frames, and staged pixels, native storage,
mipmaps, compiled programs, and pending retirement have different lifetimes.
Before introducing automatic residency, assign each allocation a domain-qualified
identity and account once for staging, live native storage, pending retirement,
and externally pinned generations. Specify upload-byte estimates versus actual
backend allocation, enforceable versus advisory budgets, and owner-thread admission
before eviction. Eviction cannot invalidate an in-flight frame or count shared
assets repeatedly. No current count promises a GPU budget or global byte total.

Current regression sources cover prepared-pixel ownership, independent metadata
scans, partial upload lifetime/retry, provider guards, strong residency, cancellation,
and retained material generations. See
[asset-loading.md](../assets/asset-loading.md),
[resource-residency.md](resource-residency.md), and
[the development plan](../planning/develop-review-and-development-plan.md#u8-resolve-resource-extension-requirements-before-consumers).
