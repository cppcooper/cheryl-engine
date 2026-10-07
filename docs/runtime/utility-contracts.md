# State, timing and math utilities

These utilities do not choose runtime threads, input devices or graphics policy.
Their public headers state their ownership and preconditions; value types still
retain or borrow external objects according to the caller's chosen type.

## Sampled and synchronized state

`StateTracker<T>` owns previous/current samples. It has no synchronization;
accessor references borrow the tracker. `update()` copies current to previous,
then assigns the new current sample. A throwing assignment follows T's guarantee
and can leave partial state. `changed()` compares the samples; `delta()` subtracts
them in T's units, rather than introducing a clock interval.

`VersionedVariable<T>` protects one value/revision pair with a mutex. Use `snapshot()`
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

`ObservedVariable<T>` owns versioned storage and a fixed array of callbacks.
Every successful set invokes nonempty observers in array order on the setter's
thread, including equal sets. Callbacks run outside the storage mutex and receive
a reference to that set's argument, not a later concurrent snapshot. Do not retain
the reference beyond the call. Concurrent or reentrant sets can overlap callbacks;
callbacks protect their own borrowed targets. A callback exception propagates after
publication and skips remaining observers without rollback. Waiters still follow
value changes alone. `wait_until_change()` saves a revision at the time of that
call; retain an earlier revision explicitly when an earlier unread change matters.

## Transition tables

`StateMachine<State, Trigger>` owns state, rules and callback captures. Use one
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

`DeltaTime` samples its clock once per call, returns elapsed seconds and starts
the next interval at that sample. `elapsed()` only observes; `checkin()` resets.
It is caller-owned and unsynchronized. The default steady clock is monotonic;
a custom clock controls backward jumps and duration representation. Duration
aliases and `tcast`/`hcast`/`mcast`/`scast`/`mscast` follow standard `duration_cast`:
integer conversions truncate toward zero and callers ensure representability.
They add no finite-value, overflow or scheduling policy.

`parse_floats()` requires a complete `stod` token, accepts its leading-space,
sign, locale and nonfinite-value behavior, and rejects trailing data. Integer
parsing uses decimal `from_chars`, without leading whitespace/plus; signed parsing
allows minus. Syntax errors throw `invalid_args`, range errors `bad_request`.
Integers choose the smallest fitting type of the selected signedness. The
`string_to_number` compatibility selector uses the floating parser only when it
finds `.`, `e` or `E`; floating results are double. It does not auto-detect bases or
special values without those markers. Input strings are borrowed only for the call.

`human_readable()` formats binary byte units at powers of 1024 to one decimal;
rounding near a boundary can display 1024.0 in the preceding unit. `adjust_length()`
uses caller-defined length units: larger adds the growth amount, greedy truncates
the scaled length then adds it, and exact/unknown policies retain the input.
Greedy requires a finite positive factor; unrepresentable growth throws
`bad_request`. `reduce()` steps greedy to larger, and larger/exact/unknown to exact.
These functions return values without allocating managed storage.

## Packed bits, addresses and geometry

`BitArray<Bits>` owns packed native words with bit zero at the least-significant
position. Indices must be below Bits; debug assertions are not release-mode bounds
checks. Mutable bit proxies and word spans borrow the array. Access requires caller
synchronization even for different bits in the same word. Whole-word access may
change unused high bits; word size/byte order is not a portable serialized format.

Pointer helpers calculate byte offsets/alignment without owning storage or checking
allocation bounds, object lifetime or overflow. Supply representable addresses and
nonzero valid alignments; derived pointers stay within the backing allocation.
Pointer comparisons require comparable ranges. Null address alignment is zero,
which cannot be reused in the alignment-offset helpers. `pointer_to_hash()` accepts
1/2/4/8-byte widths and derives a hash from the address; smaller widths reduce modulo
the unsigned maximum. This is not a stable identity across releases or processes.

Anchor helpers build CPU quads in local Y-up pixels from top-left source rectangles.
They require a finite normalized pivot and nonzero texture dimensions; invalid
values throw before writing. They do not check frame size/source bounds. Pointer
overloads require six writable Vertex2D entries or thirty interleaved floats.
Case-sensitive two-letter anchor names include CL/ML and CR/MR aliases; unknown
names/enum values select center. The [asset coordinate contract](../assets/asset-values-and-playback.md#coordinates-and-grid-geometry)
defines grid geometry and normalized UV ownership.

Block range operations are caller-synchronized; manager transactions and retained
release contexts have the [resource lifetime contract](../resources/resource-lifetime.md).
`Block::vector(deleter)` creates independent element control blocks rather than
aliasing the backing owner. The supplied deleter retains/releases backing storage
and manages constructed element lifetime; this helper does not construct elements
or retain the Block's owner automatically. Singleton construction/publication is
synchronized, while returned pointers borrow static storage and each Type governs
its subsequent operations; the full contract is in
[singleton.h](../../projects/engine/include/cheryl/templates/singleton.h).
