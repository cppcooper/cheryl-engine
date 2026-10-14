# State, timing and math utilities

These utilities do not choose runtime threads, input devices or graphics policy.
Their public headers state their ownership and preconditions; value types still
retain or borrow external objects according to the caller's chosen type.
`CE::State` owns sampled state and publication, `CE::Time` owns clock/duration
helpers, and `CE::Math` owns mathematical representations such as packed bits.
Allocation policy and address operations belong to `CE::Memory`; numeric text
parsing belongs to `CE::Text::Numbers`, and drawable geometry to
`CE::Rendering::Geometry`. Header paths remain stable across these logical owners.

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

Engine type introspection uses CTTI 1.1's `name_of` and `type_id_of` APIs.
`TYPENAME`/`TYPENAMEOF` return fully qualified names as `std::string_view`;
the expression-name macro preserves `decltype` qualifiers without evaluating its
argument. Value type IDs apply decay; explicit type IDs preserve the supplied type.

`CE::Time::DeltaTime` samples its clock once per call, returns elapsed seconds and starts
the next interval at that sample. `elapsed()` only observes; `checkin()` resets.
It is caller-owned and unsynchronized. The default steady clock is monotonic;
a custom clock controls backward jumps and duration representation. Duration
aliases and `CE::Time::duration_cast`/`CE::Time::to_hours`/`CE::Time::to_minutes`/`CE::Time::to_seconds`/`CE::Time::to_milliseconds` follow standard `duration_cast`:
integer conversions truncate toward zero and callers ensure representability.
They add no finite-value, overflow or scheduling policy.

`CE::Text::Numbers::parse_floats()` requires a complete `stod` token, accepts its leading-space,
sign, locale and nonfinite-value behavior, and rejects trailing data. Integer
parsing uses decimal `from_chars`, without leading whitespace/plus; signed parsing
allows minus. Syntax errors throw `invalid_args`, range errors `bad_request`.
Integers choose the smallest fitting type of the selected signedness. The
`CE::Text::Numbers::string_to_number` compatibility selector uses the floating parser only when it
finds `.`, `e` or `E`; floating results are double. It does not auto-detect bases or
special values without those markers. Input strings are borrowed only for the call.

`CE::Memory::format_bytes()` formats binary byte units at powers of 1024 to one decimal;
rounding near a boundary can display 1024.0 in the preceding unit. `CE::Memory::adjust_length()`
uses caller-defined length units: larger adds the growth amount, greedy truncates
the scaled length then adds it, and exact/unknown policies retain the input.
The named policies are `CE::Memory::FitPolicy::exact`, `larger`, and `greedy`.
Greedy requires a finite positive factor; unrepresentable growth throws
`bad_request`. `CE::Memory::reduce_fit_policy()` steps greedy to larger, and larger/exact/unknown to exact.
These functions return values without allocating managed storage.

## Packed bits, addresses and geometry

`CE::Math::BitArray<Bits>` owns packed native words with bit zero at the least-significant
position. Indices must be below Bits; debug assertions are not release-mode bounds
checks. Mutable bit proxies and word spans borrow the array. Access requires caller
synchronization even for different bits in the same word. Whole-word access may
change unused high bits; word size/byte order is not a portable serialized format.

Pointer helpers calculate byte offsets/alignment without owning storage or checking
allocation bounds, object lifetime or overflow. Supply representable addresses and
nonzero valid alignments; derived pointers stay within the backing allocation.
Pointer comparisons require comparable ranges. Null address alignment is zero,
which cannot be reused in the alignment-offset helpers. `CE::Memory::Pointers::pointer_to_hash()` accepts
1/2/4/8-byte widths and derives a hash from the address; smaller widths reduce modulo
the unsigned maximum. This is not a stable identity across releases or processes.

`CE::Rendering::Geometry::Anchor` helpers build CPU quads in local Y-up pixels from top-left source rectangles.
They require a finite normalized pivot and nonzero texture dimensions; invalid
values throw before writing. They do not check frame size/source bounds. Pointer
overloads require six writable `CE::Rendering::Geometry::Vertex2D` entries or thirty interleaved floats.
Case-sensitive two-letter anchor names include CL/ML and CR/MR aliases; unknown
names/enum values select center. The [asset coordinate contract](../assets/asset-values-and-playback.md#coordinates-and-grid-geometry)
defines grid geometry and normalized UV ownership.

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

`CE::State::Singleton<Type>` requires public construction;
`CE::State::FriendSingleton<Type>` supports a constructor accessible through
friendship. Both synchronize construction/publication. Returned pointers borrow
static storage and each Type governs its subsequent operations; the full contract is in
[singleton.h](../../projects/engine/include/cheryl/templates/singleton.h).

## File discovery

`CE::Files::FileIndex` indexes regular files by extension beneath explicitly
supplied directories. It is caller-owned and unsynchronized, with no refresh or
removal operation. Construct a new index to rescan an already visited root.
Paths use equality without canonicalization, so alternate paths can index the
same physical file twice. Extensions are lowercased with their leading dot;
extensionless files use `file_no_ext`, and enumeration is unsorted.
`get_files_of_type()` borrows an index-owned vector and inserts an empty bucket
for an unknown key. Further indexing can change its contents and invalidate
element references or iterators. Filesystem/allocation errors propagate and can
leave a partial index; see
[file-mgr.h](../../projects/engine/include/cheryl/core/resources/fileio/file-mgr.h).
