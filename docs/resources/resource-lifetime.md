# CPU memory lifetime and reservation

## Choose an allocation path

| Need | Use | Lifetime |
| --- | --- | --- |
| Construct a batch immediately | `Objects::Pool<T>::retrieve_objects(count, args...)` | Each returned shared handle destroys and returns its object independently. |
| Reserve capacity, then construct selected slots with different arguments | `Objects::Reservation<T>` | The reservation returns unclaimed ranges; claimed handles keep their slots. |
| Use an ordinary allocator for one constructed batch | `Objects::Factory<T, Allocator>::create()` | Completed elements can release independently; the last element owner releases the original allocation. |
| Implement storage bookkeeping or custom construction | Blocks, managers and construction contexts | The caller preserves the lower-level range and live-object invariants below. |

### Construct a pool batch

Include `<cheryl/core/resources/objects/pool.h>`:

```cpp
struct Particle {
    float age = 0;
};
auto particles = CE::Memory::Objects::Pool<Particle>::get().retrieve_objects(16);
particles.front()->age = 0.25f;
auto retained = particles.front();
particles.clear();
retained.reset();
```

Clearing the vector releases fifteen objects; `retained` keeps the first alive
until reset. The handles capture the release state rather than looking up the
pool when they die. Share object mutation only with application synchronization.

### Construct selected reserved slots

Include `<cheryl/core/resources/objects/object-reservation.hpp>` and `<memory>`:

```cpp
struct Actor {
    int id;
    explicit Actor(int value) : id(value) {}
};
std::shared_ptr<Actor> actor;
{
    CE::Memory::Objects::Reservation<Actor> slots(4);
    actor = slots.emplace(1, 7);
}
actor->id = 8;
actor.reset();
```

Only slot one is constructed. Reservation destruction returns the other three
slots; the actor remains valid until its handle resets. A throwing constructor
leaves its slot reserved and available for a retry. Claiming the same slot twice
rejects. The default pool allocator supports subrange return; substituting
`std::allocator` does not satisfy this reservation contract.

Use [Ownership and Cleanup](../architecture/ownership-cleanup.md) for the relation
between retained CPU release state and backend lifetimes. The following sections
define the exact bookkeeping, exception and reentrancy contracts.

## Pool ownership

`CE::Memory::Block<T>` is the typed range and backing ownership primitive.
`CE::Memory::ByteBlock` and `CE::Memory::HeapBlock` name its `void` specialization;
`CE::Memory::OptionalByteBlock` is an optional byte block. Byte bookkeeping belongs
to `CE::Memory`, while object construction, reservation and pools belong to
`CE::Memory::Objects`.

`CE::Memory::BlockManagement<T>` stores its pool, sections, registry, stale entries, and release queue in a shared `State`. Its static accessors refer to this state. A `CE::Memory::Objects::Pool<T>` facade holds a `shared_ptr<CE::Memory::Objects::PoolState<T>>`; that state keeps the bookkeeping alive. An object handle captures the `CE::Memory::Objects::PoolState<T>` itself, so it can return storage after the facade dies without calling `CE::Memory::Objects::Pool<T>::get()`.

## Reservations

`CE::Memory::Objects::Reservation<T, Allocator>` accepts an allocator only if its manager identifies a release context derived from `CE::Memory::AbstractBlockManager<T>` and exposes block retrieval, subrange return, and retained release operations. Currently `CE::Memory::Allocators::ObjectPoolAllocator<T>` satisfies that contract. `CE::Memory::Allocators::DefaultAllocator<T>`, `CE::Memory::Allocators::ObjectAllocator<T>`, and `std::allocator<T>` do not. The ordinary allocator `allocate(n)` and `deallocate(original,n)` still address a whole allocation. Reservations return interior ranges through their manager context; `CE::Memory::Objects::Factory<T, Allocator>` deallocates its one allocation only after the last element handle dies.

A reservation stores only unclaimed `CE::Memory::Block<T>` ranges. `emplace(index, args...)` prepares split blocks and the handle before constructing the object, then transfers the one slot to the handle. If construction throws, the reservation still owns that slot. At destruction it returns the remaining contiguous ranges, while claimed object handles can outlive it. Trusted release methods used by destructors are `noexcept`: an internal invariant violation terminates rather than throwing from a deleter. Public return methods continue to report invalid calls with exceptions.

## Release contexts

Underlying `CE::Memory::ObjectMemoryManager<T>` instances can die before the pool's last backing owner.
Object backing owners and grid/STB geometry handles created by `CE::Memory::make_managed_block`
retain a `CE::Memory::ByteReleaseContext`, which holds the shared void bookkeeping independently
of the manager facade. Final release returns the captured `CE::Memory::HeapBlock` through that
context without a raw manager pointer, singleton lookup, or weak-token liveness
check. Returned bytes remain reusable while the shared bookkeeping is retained.
Public returns report invalid blocks with exceptions; trusted final releases are
noexcept and terminate on a broken invariant or a failure to complete the return.
Creating a handle or acquiring its context still requires a live manager facade.

## Block transactions

Byte and typed block checkout/return serialize registry, sections, pool, stale, and
release transitions with `CE::Memory::BlockTransactions<T>`. Each backing owner is either one
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

## Object construction

`CE::Memory::Objects::Constructor<T>` retains a synchronized `Context` shared by that T. This construction context is independent of the runtime's `CE::Runtime::Context`. Each tracked slot
is raw, constructing, live, or destroying. Claiming a raw slot allocates its entry
before user construction; publication afterward cannot allocate. Constructors and
destructors run outside the tracking mutex, and no map iterator crosses those calls.
Distinct slots support concurrent or reentrant operations. Operations on a busy
slot are rejected rather than racing its object lifetime. Erasing tracking requires
an entirely inactive range; a live or busy slot leaves that erase request unchanged.
Batch construction can still fail after earlier slots succeed; the owning batch
must clean up those completed slots. Pool handles, allocator-backed destruction,
and backing owners use their captured construction context during final cleanup.
Unrestricted tracking-map writes cannot maintain these phase and synchronization
guarantees. Use the public static `construct`, `destroy`, and `erase` entry points
for ordinary live callers.

The protected `CE::Assets::Cache::Store::allocate` interface remains available for callers that construct raw slots themselves. Sprite and tileset loaders use reservations and create handles only for entries they actually construct. `CE::Memory::Objects::Pool<T>::retrieve_objects` retains pool state in its element deleters, without looking up a singleton when those handles die.

If construction of a `retrieve_objects` batch fails, completed handles release their slots and the unconstructed tail is returned as one range. `CE::Memory::Objects::Constructor` reserves its tracking entry before invoking a constructor, so tracking allocation cannot fail after the object becomes live.

GPU/cache ownership and retirement follow the
[resource residency contract](resource-residency.md); runtime teardown follows the
[frame boundary](../runtime/runtime-frame-boundary.md). Reusable validation
procedures and coverage limits are in
[architecture-validation.md](../development/architecture-validation.md).
