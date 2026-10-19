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

```{graphviz}
:caption: Memory integration
:alt: Applications construct pooled objects and asset or legacy text caches retain managed CPU backing through Memory. Allocators supply storage; retained release contexts keep destruction and slot return valid after a facade disappears.

digraph memory_usage {
    rankdir=LR;
    memory [label="Memory", penwidth=2];
    application [label="Application objects"];
    caches [label="Assets / legacy Text\ncaches and geometry"];
    storage [label="Allocator / backing storage"];
    release [label="Retained release context"];
    application -> memory [label="constructs / reserves\npooled CPU objects"];
    caches -> memory [label="retains managed CPU backing"];
    memory -> storage [label="checks out / returns ranges"];
    memory -> release [label="captures bookkeeping\nin returned handles"];
}
```

### Uses

Allocators/backing owners supply storage and construction contexts track live
slots. Synchronization protects bookkeeping; Diagnostics observes copied
allocation statistics.

| Types | Systems |
| --- | --- |
| {cpp:struct}`CE::Memory::Allocators::ObjectPoolAllocator`<br>{cpp:struct}`CE::Memory::Objects::Constructor <template<typename T> CE::Memory::Objects::Constructor>`<br>{cpp:struct}`CE::Memory::BlockTransactions`<br>{cpp:class}`CE::State::Singleton`<br>{cpp:type}`CE::Diagnostics::DomainId` | [State](state.md)<br>[Utilities](utilities.md)<br>[Diagnostics](diagnostics.md) |

### Used by

Caches and prepared geometry retain managed CPU backing. Application pools need
no runtime lifecycle. Resources has a separate native lifetime contract;
bookkeeping locks do not synchronize arbitrary object contents or authorize
graphics calls.

| Types | Systems |
| --- | --- |
| {cpp:struct}`CE::Assets::Cache::Store`<br>{cpp:struct}`CE::Assets::Grids::GeometryData`<br>{cpp:struct}`CE::Text::Legacy::AsciiFont`<br>[Application-owned pooled objects](../resources/resource-lifetime.md#pool-ownership) | [Assets](assets.md)<br>[Text](text.md) |

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
