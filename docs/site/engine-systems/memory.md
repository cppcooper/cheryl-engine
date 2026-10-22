# Memory

## Purpose

Memory provides CPU block bookkeeping, pooled object construction, reservations
and retained release contexts. Storage ownership and an object's live state are
separate; returning a block is not permission to destroy still-live objects.

## Interface

Types below are in `CE::Memory` unless another namespace is shown.

For ordinary application objects, start with `Objects::Pool<T>` and its retained
handles. Use reservations for explicitly selected construction, and read the
[allocator requirements](../resources/resource-lifetime.md#reservations) when
implementing storage integration rather than using the default pool.

### Types and operations

| API and purpose |
| --- |
| {cpp:struct}`Block <CE::Memory::Block>`, {cpp:struct}`Manager <CE::Memory::Manager>`<br>Describe ranges and check out/return/coalesce CPU storage. [Pool ownership](../resources/resource-lifetime.md#pool-ownership); [block transactions](../resources/resource-lifetime.md#block-transactions). |
| {cpp:struct}`Objects::Pool <CE::Memory::Objects::Pool>`<br>Construct retained object handles whose release state can outlive the facade. [Object-pool ownership](../resources/resource-lifetime.md#pool-ownership). |
| {cpp:class}`Objects::Reservation <CE::Memory::Objects::Reservation>`<br>Reserve slots and transfer individually constructed objects into handles. [Reservation and allocator requirements](../resources/resource-lifetime.md#reservations). |
| {cpp:struct}`Objects::Constructor <template<typename T> CE::Memory::Objects::Constructor>`<br>Track construction phases while user constructors/destructors run outside bookkeeping locks. [Object construction](../resources/resource-lifetime.md#object-construction). |
| {cpp:struct}`ByteReleaseContext <CE::Memory::ByteReleaseContext>`<br>Retain byte bookkeeping for final release without a destroyed-manager lookup. [Release contexts](../resources/resource-lifetime.md#release-contexts). |

### Storage and lifetime choices

| Choice | Purpose and details |
| --- | --- |
| Pooled handles / reserved slots | Handles own claimed objects; a reservation returns only its unclaimed ranges. [Reservation lifetime](../resources/resource-lifetime.md#reservations). |
| {cpp:struct}`Allocators::ObjectPoolAllocator <CE::Memory::Allocators::ObjectPoolAllocator>` | Supplies the retained manager/release contract needed by reservations; ordinary allocators do not automatically qualify. [Supported allocators](../resources/resource-lifetime.md#reservations). |
| {cpp:enum}`FitPolicy <CE::Memory::FitPolicy>` | `exact`, `larger`, `greedy` select length-growth policy; these helpers do not allocate storage. [Fit arithmetic and limits](../runtime/utility-contracts.md#timing-and-numeric-helpers). |
| CPU / native resource ownership | CPU bookkeeping does not authorize native GPU use or replace graphics retirement. [CPU lifetime](../resources/resource-lifetime.md); [native retirement](../resources/resource-residency.md#native-retirement-and-maintenance). |

## Usage

Start with {cpp:struct}`Memory::Objects::Pool <CE::Memory::Objects::Pool>`
when application objects should be constructed now and released independently.
The returned handles retain their release state; owning a handle does not
synchronize the object's members.

```{engine-diagram} memory-application
```

The [engine dependency view](diagram-explorer.md#memory-internal) follows
caches and geometry into their actual storage owners.

### Choose storage and construction together

| Need | Entry point | What the caller retains |
| --- | --- | --- |
| Construct every object in a batch now | `Memory::Objects::Pool<T>::retrieve_objects()` | One shared handle per live object; its final release destroys and returns that slot. |
| Construct selected slots with different arguments | {cpp:class}`Memory::Objects::Reservation <CE::Memory::Objects::Reservation>` | The reservation owns unclaimed slots; claimed handles can outlive it. A failed constructor leaves its slot available to retry. |
| Integrate an ordinary allocator for a complete batch | {cpp:struct}`Memory::Objects::Factory <CE::Memory::Objects::Factory>` | Element handles release independently; the last handle releases the original whole allocation. |
| Manage bytes or build a storage adapter | {cpp:struct}`Memory::Manager <CE::Memory::Manager>` and {cpp:struct}`Memory::ByteReleaseContext <CE::Memory::ByteReleaseContext>` | A live facade for acquisition and a captured release context for final returns. Object construction remains a separate responsibility. |

An allocator that supports whole-allocation `allocate`/`deallocate` is not
necessarily valid for reservations. The default
{cpp:struct}`Memory::Allocators::ObjectPoolAllocator <CE::Memory::Allocators::ObjectPoolAllocator>`
supplies retained release state and interior-range return. See the
[allocation recipes and allocator requirements](../resources/resource-lifetime.md).

### Uses

| Consumer | Dependency | Relationship |
| --- | --- | --- |
| `Memory::Objects::Pool<T>` | {cpp:struct}`Memory::Objects::PoolState <CE::Memory::Objects::PoolState>` | Retains the release operations that object handles capture instead of looking up the pool during deletion. |
| `Memory::Objects::PoolState<T>` | {cpp:struct}`Memory::Objects::Constructor <template<typename T> CE::Memory::Objects::Constructor>` | Retains construction tracking for pooled object lifetime; user constructors/destructors run outside tracking locks. |
| `Memory::Objects::Reservation<T>` | `Memory::Allocators::ObjectPoolAllocator<T>` and {cpp:struct}`Memory::Block <CE::Memory::Block>` | Tracks only unclaimed ranges and transfers individual slots into handles. |
| `Memory::Manager` and `Memory::Objects::Pool<T>` | {cpp:class}`State::Singleton <CE::State::Singleton>` | Offer shared facade access. Retained handles do not depend on a later singleton lookup. [State](state.md). |
| `Memory::Manager` | {cpp:enum}`Memory::FitPolicy <CE::Memory::FitPolicy>` and address/fit helpers | Computes request growth/alignment before bookkeeping checkout. [Utilities](utilities.md). |
| Byte bookkeeping | {cpp:struct}`Memory::Stats <CE::Memory::Stats>` and {cpp:type}`Diagnostics::DomainId <CE::Diagnostics::DomainId>` | Copies synchronized allocation observations before formatting or reporting. [Diagnostics](diagnostics.md). |

### Used by

| Consumer | Memory entry point | Why it is used |
| --- | --- | --- |
| Application object owners | `Memory::Objects::Pool<T>` or `Memory::Objects::Reservation<T>` | Retain independently released CPU objects without starting a runtime service. |
| {cpp:struct}`Assets::Cache::Store <CE::Assets::Cache::Store>` | `Memory::Objects::Reservation<T>` / `Memory::Allocators::ObjectPoolAllocator<T>` | Reserves cache object storage and creates handles only for constructed entries. [Assets](assets.md). |
| {cpp:struct}`Assets::Grids::GeometryData <CE::Assets::Grids::GeometryData>` preparation | {cpp:type}`Memory::ExactManager <CE::Memory::ExactManager>` and retained byte release | Holds CPU vertex backing produced by `Assets::Grids::make_geometry()`. [Grid geometry](../assets/asset-values-and-playback.md#coordinates-and-grid-geometry). |
| {cpp:struct}`Text::Legacy::AsciiFontData <CE::Text::Legacy::AsciiFontData>` preparation | `Memory::ExactManager` and retained byte release | Supplies CPU glyph vertices while building/uploading the legacy font. [Text](text.md). |

### CPU release and native retirement

Returning CPU storage and retiring a graphics resource have different owners.
Release live objects before returning their raw ranges. For GPU-backed assets,
dropping a CPU handle does not authorize a native deletion call: the
[resource owner collects native retirement](../resources/resource-residency.md#native-retirement-and-maintenance)
on the appropriate backend thread/context.

## Contracts

Checkout/return preserves nonoverlapping ownership and contiguous partition
invariants. Construct/destroy operations on a busy slot reject. User constructors
and destructors run outside tracking locks. Allocation or batch construction may
fail after earlier independent work succeeds; owners must clean up completed
objects. Trusted destructor release is nonthrowing and terminates on broken
bookkeeping invariants.

Block/pointer helpers do not validate every dereference or establish C++ object
lifetime. Callers remain responsible for live object access, alignment and
external quiescence where raw collection access is used.

## Lifecycle

Acquire storage with a live manager, construct objects, retain handles and let
their deleters destroy/return slots through captured release state. That state
can outlive the facade. A reservation returns unclaimed ranges; claimed handles
may survive it. Finish facade callers before destroying the facade even when
retained contexts permit later final release.

## Execution

Bookkeeping and distinct-slot construction support synchronized/reentrant
operations as documented. This does not synchronize members of a pooled game
object. Choose a simulation owner or add application synchronization. Native GPU
release continues to follow backend affinity/retirement rather than these CPU
manager locks.

## Integration

A class can allocate a batch of default-constructible application particles:

```cpp
struct Particle { float age = 0; };
auto particles = CE::Memory::Objects::Pool<Particle>::get().retrieve_objects(16);
particles.front()->age = 0.25f;
particles.clear();
```

Each shared handle retains the release context. Keep mutation on the game's
owner; sharing a handle alone does not make `age` synchronized.

## Limits and further reading

There is no general memory budget, automatic world ownership or NUMA placement
policy here. Read [CPU memory lifetime and reservation](../resources/resource-lifetime.md),
[value/pointer utility contracts](../runtime/utility-contracts.md),
[object pool declarations](../../../projects/engine/include/cheryl/core/resources/objects/pool.h)
and [native resource lifetime](../resources/resource-residency.md).
