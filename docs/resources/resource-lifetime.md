# Resource lifetime and reservation

`Block<T>` remains the typed range and backing ownership primitive. `BlockManagement<T>` stores its pool, sections, registry, stale entries, and release queue in a shared `State`. Existing static accessors refer to this state. An `Obj::Pool<T>` facade holds a `shared_ptr<PoolState<T>>`; that state keeps the bookkeeping alive. An object handle captures the `PoolState` itself, so it can return storage after the facade dies without calling `Pool<T>::get()`.

`ObjectReservation<T, Allocator>` accepts an allocator only if its manager identifies a release context derived from `AbstractManager<T>` and exposes block retrieval, subrange return, and retained release operations. Currently `ObjectPoolAllocator<T>` satisfies that contract. `DefaultAllocator<T>`, `ObjectAllocator<T>`, and `std::allocator<T>` do not. The ordinary allocator `allocate(n)` and `deallocate(original,n)` still address a whole allocation. Reservations return interior ranges through their manager context; the generic factory now deallocates its one allocation only after the last element handle dies.

A reservation stores only unclaimed `Block<T>` ranges. `emplace(index, args...)` prepares split blocks and the handle before constructing the object, then transfers the one slot to the handle. If construction throws, the reservation still owns that slot. At destruction it returns the remaining contiguous ranges, while claimed object handles can outlive it. Trusted release methods used by destructors are `noexcept`: an internal invariant violation terminates rather than throwing from a deleter. Public return methods continue to report invalid calls with exceptions.

Underlying `Mem::ObjMMgr<T>` instances can die before the pool's last backing owner.
Object backing owners and grid/STB geometry handles created by `make_managed_block`
retain a `ByteReleaseContext`, which holds the shared void bookkeeping independently
of the manager facade. Final release returns the captured HeapBlock through that
context without a raw manager pointer, singleton lookup, or weak-token liveness
check. Returned bytes remain reusable while the shared bookkeeping is retained.
Public returns report invalid blocks with exceptions; trusted final releases are
noexcept and terminate on a broken invariant or a failure to complete the return.
Creating a handle or acquiring its context still requires a live manager facade.

Byte and typed block checkout/return serialize registry, sections, pool, stale, and
release transitions with `BlockTransactions<T>`. Each backing owner is either one
unsplit registry range or a complete, nonoverlapping section partition. A free
range appears in the pool and exactly one of those locations. Only complete free
owners carry stale timestamps or pending release records. Checkout cancels those
markers in the same transaction that claims and partitions the range. Returns
retain active front/back ranges and coalesce only free neighbors of the same owner.

Replacement set nodes and any required hash capacity are prepared before live
records change. Failed preparation leaves the logical partition unchanged;
successful commit transfers nodes without allocating. Preallocation commits each
owner independently. A later failure preserves earlier completed preallocations.
Backing allocation happens outside collection locks and is rechecked against the
pool before publication. Culling detaches retired owner references before releasing
them, so backing frees and user destructors can reenter other memory operations.
Statistics copy one synchronized snapshot before formatting it.

The legacy protected lookup/fill methods remain single operations; custom derived
managers must use the transaction path for compound partition changes. Raw static
collection access requires external quiescence and preservation of these invariants.
Live bytes/objects remain the caller's responsibility. Destroying a facade requires
its own callers to finish; previously retained contexts can continue releasing
ranges independently. `lifetime_token()` remains a compatibility observation and
must not authorize raw-manager access during teardown.

`ObjCtor<T>` retains a synchronized `Context` shared by that T. Each tracked slot
is raw, constructing, live, or destroying. Claiming a raw slot allocates its entry
before user construction; publication afterward cannot allocate. Constructors and
destructors run outside the tracking mutex, and no map iterator crosses those calls.
Distinct slots support concurrent or reentrant operations. Operations on a busy
slot are rejected rather than racing its object lifetime. Erasing tracking requires
an entirely inactive range; a live or busy slot leaves that erase request unchanged.
Batch construction can still fail after earlier slots succeed; the owning batch
must clean up those completed slots. Pool handles, allocator-backed destruction,
and backing owners use their captured construction context during final cleanup.
The old protected boolean map was replaced because unrestricted map writes cannot
maintain those phase and synchronization guarantees. Public static construct,
destroy, and erase entry points remain available for ordinary live callers.

The protected legacy `AssetMgr::allocate` interface remains available for callers that construct raw slots themselves. Sprite and tileset loaders now use reservations and create handles only for entries they actually construct. `Pool<T>::retrieve_objects` retains pool state in its element deleters; it no longer looks up a singleton when those handles die.

If construction of a `retrieve_objects` batch fails, completed handles release their slots and the unconstructed tail is returned as one range. `ObjCtor` reserves its tracking entry before invoking a constructor, so tracking allocation cannot fail after the object becomes live.

GPU handles are registered with the OpenGL renderer's resource lifetime. Texture,
VAO/VBO, and program destructors only retire registrations. Resource use and native
deletion require both the owner thread and its actual current context. Renderer
shutdown restores that context, deletes every tracked handle, closes the lifetime,
and releases the context. External asset handles may outlive shutdown; use then
fails before querying the borrowed context. If destructor cleanup cannot recover
the context, it invalidates registrations without OpenGL calls, leaving remaining
native cleanup to platform context destruction.

`AssetCacheContext` guards strong cache residency and the active loading domain.
Asset caches clear and release their provider binding when the provider is
destroyed. Shared-lock lookups retain complete handles; unique-lock publication
and clearing release retired handles outside cache locks. Provider teardown marks
the binding as releasing first so reentrant deleters cannot refill the caches.
`GameRuntime` stops simulation/worker acceptance while pumping accepted platform
dependencies, then cancels remaining platform work, recycles frames, and cleans up
the game before stopping input and graphics. Renderer maintenance runs independently
of new frames, before bounded idle waits and while accepted work settles. Owned input is
destroyed before the provider, renderer, surface, and display. The platform
context must outlive its renderer. Recorded execution scopes are in
[architecture-validation.md](../development/architecture-validation.md).

The resource-by-resource ownership trace, explicit residency policy, 10 ms idle
wait bound, native failure guards, and maintenance contract are in
[resource-residency.md](resource-residency.md).
