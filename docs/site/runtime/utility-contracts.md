# State, timing and math utilities

These utilities do not choose runtime threads, input devices or graphics policy.
Their public headers state their ownership and preconditions; value types still
retain or borrow external objects according to the caller's chosen type.
`CE::State` owns sampled state and publication, `CE::Time` owns clock/duration
helpers, and `CE::Math` owns rectangle overlap tests and mathematical
representations such as packed bits.
Allocation policy and address operations belong to `CE::Memory`; numeric text
parsing belongs to `CE::Text::Numbers`, and drawable geometry to
`CE::Rendering::Geometry`. Header paths remain stable across these logical owners.

Use the questions below to find an operation without reading every helper family.
The [State](../engine-systems/state.md) and [Utilities](../engine-systems/utilities.md)
overviews explain their place in a game; the declarations linked here own the
individual overload contracts.

| Need | Start here | Declaring header |
| --- | --- | --- |
| Compare, publish or observe a value | [Choose a state representation](#choose-a-state-representation) | [StateTracker](../../../projects/engine/include/cheryl/templates/state-tracker.h), [VersionedVariable](../../../projects/engine/include/cheryl/templates/versioned-variable.h), [ObservedVariable](../../../projects/engine/include/cheryl/templates/observed-variables.h). |
| Model guarded behavior changes | [Transition tables](#transition-tables) | [StateMachine](../../../projects/engine/include/cheryl/templates/state-machine.h). |
| Measure work or convert time units | [Measure elapsed time](#measure-elapsed-time) | [DeltaTime](../../../projects/engine/include/cheryl/templates/delta.h), [duration helpers](../../../projects/engine/include/cheryl/math/time.h). |
| Parse a number or size an allocation | [Parse numbers](#parse-numbers), [size CPU allocations](#size-cpu-allocations) | [Numeric parsing](../../../projects/engine/include/cheryl/math/string-numbers.h), [fit arithmetic](../../../projects/engine/include/cheryl/math/fit.h), [byte formatting](../../../projects/engine/include/cheryl/math/bytes.h). |
| Work with packed bits or addresses | [Packed bits, addresses and geometry](#packed-bits-addresses-and-geometry) | [BitArray](../../../projects/engine/include/cheryl/math/binary.h), [pointer helpers](../../../projects/engine/include/cheryl/math/pointers.h). |
| Build an anchored quad | [Build geometry](#build-geometry) | [Anchor](../../../projects/engine/include/cheryl/math/anchor.h). |
| Test rectangle collisions | [Test rectangle overlap](#test-rectangle-overlap) | [Overlap helper](../../../projects/engine/include/cheryl/math/collision.h). |
| Discover source files | [File discovery](#file-discovery) | [FileIndex](../../../projects/engine/include/cheryl/core/resources/fileio/file-mgr.h). |

## Choose a state representation

| Application need | Choose | Consequence |
| --- | --- | --- |
| Compare this update's value with the previous sample | `StateTracker<T>` | One owner supplies the sampling cadence; no synchronization is added. |
| Let readers obtain the latest value and its revision together | `VersionedVariable<T>` | Readers copy a synchronized snapshot; intermediate updates can coalesce. |
| Run immediate callbacks for successful setters | `ObservedVariable<T>` | Callbacks run on the setter's thread, including equal sets. |
| Model guarded behavior changes | `StateMachine<State, Trigger>` | Application-defined rules choose transitions and enter/exit actions. |
| Deliver each occurrence to registered consumers | A typed `EventChannel<Payload>` | An event bus preserves its delivery contract rather than only the latest value. |

For a score that another owner only needs to sample, include
`<cheryl/templates/versioned-variable.h>` and `<iostream>`:

```cpp
CE::State::VersionedVariable<int> score{0};
const auto previous = score.snapshot();
score.set(10);
const auto current = score.snapshot();
if (current.revision != previous.revision)
    std::cout << current.value << '\n';
```

This prints `10`. Setting `10` again leaves the revision unchanged. Keep the
variable alive through its readers; `snapshot()` owns an integer copy but would
not extend the pointees' lifetime if `T` contained borrowed pointers. Use
[typed events](event-delivery.md#typed-channels) when every score award matters,
or dispatch an owned result when changing a simulation-owned model from elsewhere.
[Threads and Data Flow](../architecture/threads-data-flow.md#choose-how-work-is-delivered)
explains that choice across systems.

## Sampled and synchronized state

`CE::State::StateTracker<T>` owns previous/current samples. It has no synchronization;
accessor references borrow the tracker. `update()` copies current to previous,
then assigns the new current sample. A throwing assignment follows T's guarantee
and can leave partial state. `changed()` compares the samples; `delta()` subtracts
them in T's units, rather than introducing a clock interval.

`CE::State::VersionedVariable<T>` protects one value/revision pair with a mutex. Use `snapshot()`
to copy them together; separate `get()` and `revision()` calls can see different
updates. Equal sets leave the revision unchanged. Comparison, assignment and
snapshot copying execute under the mutex and must not reenter the variable. Their
exceptions propagate. A throwing assignment can change T without advancing the
revision or notifying waiters; synchronized access alone promises no transactional
rollback for arbitrary T.

`wait_for_change(saved_revision)` blocks until the revision differs and returns
the current snapshot. Multiple updates can coalesce; it does not deliver every
intermediate value. Keep the object alive and ensure a producer can make progress
for the whole wait. There is no timeout, stop token or destruction cancellation.

`CE::State::ObservedVariable<T>` owns versioned storage and a fixed array of callbacks.
Every successful set invokes nonempty observers in array order on the setter's
thread, including equal sets. Callbacks run outside the storage mutex and receive
a reference to that set's argument, not a later concurrent snapshot. Do not retain
the reference beyond the call. Concurrent or reentrant sets can overlap callbacks;
callbacks protect their own borrowed targets. A callback exception propagates after
publication and skips remaining observers without rollback. Waiters still follow
value changes alone. `wait_until_change()` saves a revision at the time of that
call; retain an earlier revision explicitly when an earlier unread change matters.

## Transition tables

`CE::State::StateMachine<State, Trigger>` owns state, rules and callback captures. Use one
owner and do not recursively trigger the same machine. `state()` borrows its
storage. Rules are examined in insertion order until the first matching successful
guard; no match returns false. Guards can inspect other machines or add rules;
new rules can be considered later in the same trigger.

For a selected rule the machine copies the rule and relevant hooks, then runs exit,
assigns state, runs the transition action, and runs enter. Replacing hooks during
exit does not replace the copied hook for that transition. Exceptions stop this
sequence and propagate: exit failure leaves the old state; action/enter failure
leaves the new state. Throwing state assignment follows State's own guarantee.
Callback side effects are not rolled back. Keep borrowed callback targets alive
through invocation and capture destruction.

## Timing and numeric helpers

### Inspect types

Engine type introspection uses CTTI 1.1's `name_of` and `type_id_of` APIs.
`TYPENAME`/`TYPENAMEOF` return fully qualified names as `std::string_view`;
the expression-name macro preserves `decltype` qualifiers without evaluating its
argument. Value type IDs apply decay; explicit type IDs preserve the supplied type.

### Measure elapsed time

`CE::Time::DeltaTime` samples its clock once per call, returns elapsed seconds and starts
the next interval at that sample. `elapsed()` only observes; `checkin()` resets.
It is caller-owned and unsynchronized. The default steady clock is monotonic;
a custom clock controls backward jumps and duration representation. Duration
aliases and `CE::Time::duration_cast`/`CE::Time::to_hours`/`CE::Time::to_minutes`/`CE::Time::to_seconds`/`CE::Time::to_milliseconds` follow standard `duration_cast`:
integer conversions truncate toward zero and callers ensure representability.
They add no finite-value, overflow or scheduling policy.

### Parse numbers

`CE::Text::Numbers::parse_floats()` requires a complete `stod` token, accepts its leading-space,
sign, locale and nonfinite-value behavior, and rejects trailing data. Integer
parsing uses decimal `from_chars`, without leading whitespace/plus; signed parsing
allows minus. Syntax errors throw `invalid_args`, range errors `bad_request`.
Integers choose the smallest fitting type of the selected signedness. The
`CE::Text::Numbers::string_to_number` compatibility selector uses the floating parser only when it
finds `.`, `e` or `E`; floating results are double. It does not auto-detect bases or
special values without those markers. Input strings are borrowed only for the call.

### Size CPU allocations

`CE::Memory::format_bytes()` formats binary byte units at powers of 1024 to one decimal;
rounding near a boundary can display 1024.0 in the preceding unit. `CE::Memory::adjust_length()`
uses caller-defined length units: larger adds the growth amount, greedy truncates
the scaled length then adds it, and exact/unknown policies retain the input.
The named policies are `CE::Memory::FitPolicy::exact`, `larger`, and `greedy`.
Greedy requires a finite positive factor; unrepresentable growth throws
`bad_request`. `CE::Memory::reduce_fit_policy()` steps greedy to larger, and larger/exact/unknown to exact.
These functions return values without allocating managed storage.

## Packed bits, addresses and geometry

### Store packed bits

`CE::Math::BitArray<Bits>` owns packed native words with bit zero at the least-significant
position. Indices must be below Bits; debug assertions are not release-mode bounds
checks. Mutable bit proxies and word spans borrow the array. Access requires caller
synchronization even for different bits in the same word. Whole-word access may
change unused high bits; word size/byte order is not a portable serialized format.

### Calculate addresses

Pointer helpers calculate byte offsets/alignment without owning storage or checking
allocation bounds, object lifetime or overflow. Supply representable addresses and
nonzero valid alignments; derived pointers stay within the backing allocation.
Pointer comparisons require comparable ranges. Null address alignment is zero,
which cannot be reused in the alignment-offset helpers. `CE::Memory::Pointers::pointer_to_hash()` accepts
1/2/4/8-byte widths and derives a hash from the address; smaller widths reduce modulo
the unsigned maximum. This is not a stable identity across releases or processes.

### Build geometry

`CE::Rendering::Geometry::Anchor` helpers build CPU quads in local Y-up pixels from top-left source rectangles.
They require a finite normalized pivot and nonzero texture dimensions; invalid
values throw before writing. They do not check frame size/source bounds. Pointer
overloads require six writable `CE::Rendering::Geometry::Vertex2D` entries or thirty interleaved floats.
Case-sensitive two-letter anchor names include CL/ML and CR/MR aliases; unknown
names/enum values select center. The [asset coordinate contract](../assets/asset-values-and-playback.md#coordinates-and-grid-geometry)
defines grid geometry and normalized UV ownership.

### Test rectangle overlap

Include `<cheryl/math/collision.h>` or `<cheryl/cemath.h>` and call
`CE::Math::overlaps(p1, p2, d1, d2)` with `glm::vec2` positions and full width/height
vectors in the same Y-up coordinates and units. This inline `constexpr` helper
returns a `bool` without creating engine resources:

```cpp
const bool colliding = CE::Math::overlaps({0, 0}, {3, 0}, {4, 4}, {4, 4}); // true
```

Each position identifies its rectangle's anchor. Optional fifth and sixth
`CE::Rendering::Geometry::Pivot` arguments select independent anchors, both
defaulting to `{0.5f, 0.5f}`. Anchors use the same normalized top-left origin as
the geometry builders: `{0, 0}` places the rectangle to the right of and below its
position in Y-up coordinates. The {cpp:func}`CE::Math::overlaps` reference
owns the exact input preconditions and boundary rules. Touching edges/corners and
nonpositive dimensions return `false`; containment returns `true`. Supply finite
values and representable edges, and apply any scaling or coordinate conversion
before calling. This tests axis-aligned bounds; rotated shapes need another test.

### Retain blocks and format values

Block range operations are caller-synchronized; manager transactions and retained
release contexts have the [resource lifetime contract](../resources/resource-lifetime.md).
`CE::Memory::Block<T>::vector(deleter)` creates independent element control blocks rather than
aliasing the backing owner. The supplied deleter retains/releases backing storage
and manages constructed element lifetime; this helper does not construct elements
or retain the Block's owner automatically.

Block stream insertion finds `CE::Memory::operator<<` through argument-dependent
lookup. Its free overload participates only when the target has no applicable
member insertion for that block type. A wrapper with its own member insertion
keeps that operation and can forward the block to a standard stream, where the
Memory overload supplies formatting. Custom streams without such a member can
use the same free overload; it is not restricted to standard stream classes.
`std::align_val_t` belongs to `std`, so inserting an alignment into a
standard stream does not find the Memory overload through that lookup. Call it
explicitly (or bring the overload into local scope with a using declaration):

```cpp
CE::Memory::operator<<(stream, std::align_val_t{64});
```

### Share a constructed service

`CE::State::Singleton<Type>` requires public construction;
`CE::State::FriendSingleton<Type>` supports a constructor accessible through
friendship. Both synchronize construction/publication. Returned pointers borrow
static storage and each Type governs its subsequent operations; the full contract is in
[singleton.h](../../../projects/engine/include/cheryl/templates/singleton.h).

## File discovery

Use `CE::Files::FileIndex` for an application-owned extension index. Construct a new
index to rescan an already visited directory; returned vectors borrow that index
and all access needs caller serialization. The
[file discovery guide](../assets/file-and-font-discovery.md#file-index) owns path,
ordering, freshness, iterator and partial-failure rules. Automatic asset preparation
creates its own fresh index instead of refreshing an application's retained one.
