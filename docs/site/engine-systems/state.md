# State

## Purpose

State supplies reusable sampled values, versioned publication, observation and
transition tables. Applications choose when to sample or transition; these
types do not create threads or automatically synchronize an entire game.

## Interface

Types below are in `CE::State` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`StateTracker <CE::State::StateTracker>`<br>Retain previous/current samples and observe `changed()`/`delta()`. [Sampled state](../runtime/utility-contracts.md#sampled-and-synchronized-state). |
| {cpp:class}`VersionedVariable <CE::State::VersionedVariable>`<br>Publish and copy a synchronized value/revision pair; wait for a changed revision. [Synchronized publication](../runtime/utility-contracts.md#sampled-and-synchronized-state). |
| {cpp:class}`ObservedVariable <CE::State::ObservedVariable>`<br>Add immediate observers outside storage locks, on each setter's thread. [Observation and callbacks](../runtime/utility-contracts.md#sampled-and-synchronized-state). |
| {cpp:class}`StateMachine <CE::State::StateMachine>`<br>Store ordered guarded transitions and enter/exit/action hooks. [Transition tables](../runtime/utility-contracts.md#transition-tables). |
| {cpp:class}`Singleton <CE::State::Singleton>`, {cpp:class}`FriendSingleton <CE::State::FriendSingleton>`<br>Deliberately share construction where an existing service contract requires it. [Singleton declarations and lifetime](../../../projects/engine/include/cheryl/templates/singleton.h). |

### Choosing a state type

| Need | Purpose and details |
| --- | --- |
| Compare successive samples | Use `StateTracker<T>` with caller synchronization; `delta()` uses T's units. [Sampling and assignment](../runtime/utility-contracts.md#sampled-and-synchronized-state). |
| Share the latest value | Use `VersionedVariable<T>`; waits coalesce updates and are not event history. [Value/revision snapshots](../runtime/utility-contracts.md#sampled-and-synchronized-state). |
| React to setters | Use `ObservedVariable<T>`; equal successful sets still notify and callbacks protect their own targets. [Observer execution](../runtime/utility-contracts.md#sampled-and-synchronized-state). |
| Model behavior transitions | Supply application-defined `State` and `Trigger` types, often enums, to `StateMachine`. No engine-wide enum is prescribed. [Rules, ordering and failure](../runtime/utility-contracts.md#transition-tables). |

## Usage

Choose a state type from the information a reader needs: successive local
samples, the latest published value, immediate setter callbacks, or guarded
behavior transitions. These are independent application-owned values rather
than a runtime service that must be started.

```{engine-diagram} state-application
```

The [engine dependency view](diagram-explorer.md#state-internal) also shows
where existing services use shared construction.

### Choose what a change means

| Application question | Choose | Observation and execution |
| --- | --- | --- |
| What changed since my previous update? | {cpp:class}`State::StateTracker <CE::State::StateTracker>` | The caller supplies each sample; `changed()` compares previous/current and is not an unread-change flag. |
| What is the latest value and its revision? | {cpp:class}`State::VersionedVariable <CE::State::VersionedVariable>` | `snapshot()` copies one synchronized pair. Equal sets keep the revision; intermediate changes can coalesce. |
| What should run whenever a setter succeeds? | {cpp:class}`State::ObservedVariable <CE::State::ObservedVariable>` | Observers run on the setter's thread, including equal sets, after the storage lock releases. |
| Which guarded behavior transition is permitted? | {cpp:class}`State::StateMachine <CE::State::StateMachine>` | One caller triggers ordered rules; the first matching successful guard selects the transition. |
| Must every occurrence reach consumers? | [Typed event channel](events.md) | Use event delivery rather than treating the latest value/revision as occurrence history. |

An observer that must mutate simulation-owned state should submit owned work to
[Dispatch](dispatch.md), rather than assume the setter runs on simulation.
`wait_for_change()` has no cancellation/timeout; keep the variable alive and
arrange for its producer and waiters to finish before destroying it. The
[state guide](../runtime/utility-contracts.md#sampled-and-synchronized-state)
owns the detailed exception and callback rules.

### Uses

| Consumer | Dependency supplied by the caller | Requirement |
| --- | --- | --- |
| `State::StateTracker<T>` | Sample type `T` | Defines copying, assignment, comparison and optional subtraction; the caller serializes access. |
| `State::VersionedVariable<T>` | Value type `T` | Copies value/revision under one storage lock. That lock does not extend pointee lifetimes or promise rollback for throwing assignment. |
| `State::ObservedVariable<T>` | `State::VersionedVariable<T>` and observer captures | Publishes before invoking observers; borrowed targets remain alive and protect themselves against overlapping callbacks. |
| `State::StateMachine<State, Trigger>` | Application state/trigger types, guards and actions | Owns the rule table and callable captures; no runtime, input or worker service is required. |

### Used by

| Consumer | State helper | Relationship |
| --- | --- | --- |
| Application models | Sampled/publication/observer/transition types above | The application chooses ownership, sampling and trigger boundaries; no global game state machine is prescribed. |
| {cpp:struct}`Assets::Loading::Loader <CE::Assets::Loading::Loader>` and asset caches | {cpp:class}`State::Singleton <CE::State::Singleton>` | Publish deliberately shared service facades; explicit initialization must precede competing access when configuration matters. [Assets](assets.md). |
| {cpp:struct}`Events::EventSystem <CE::Events::EventSystem>` | `State::Singleton` | Offers the process-default event facade; independently owned buses remain possible. [Events](events.md). |
| {cpp:struct}`Memory::Manager <CE::Memory::Manager>` and {cpp:struct}`Memory::Objects::Pool <CE::Memory::Objects::Pool>` | `State::Singleton` | Share acquisition facades while handles retain independent final-release contexts. [Memory](memory.md). |

### Shared construction is a separate choice

`State::Singleton<T>` requires accessible public construction;
{cpp:class}`State::FriendSingleton <CE::State::FriendSingleton>` supports a
constructor accessible through friendship. Both synchronize construction and
publication, leaving subsequent operations to `T`'s own contract.
`get_existing()` borrows a pointer and does not pin the service against static
teardown. Use an existing service's declared entry point rather than wrapping
an application value in a singleton to obtain synchronization. See
[shared service construction](../runtime/utility-contracts.md#share-a-constructed-service).

## Contracts

`snapshot()` copies a versioned value and revision together; separate getters may
observe different updates. Equal sets do not advance a versioned revision.
Waits coalesce updates and deliver current state rather than every intermediate
value. Assignment/copy exceptions retain the supplied type's guarantee, not a
transactional rollback guarantee.

Observed callbacks run outside storage locks on the setter's thread, including
equal successful sets. Concurrent/reentrant setters can overlap callbacks.
State-machine guards/rules have insertion order; failures stop transition steps
without undoing prior callback effects. Do not recursively trigger the same
machine.

## Lifecycle

Own each value/machine within its application component. Keep borrowed callback
targets alive through calls and capture destruction. Versioned waiters require
a live variable and a producer able to make progress; destruction is not
cancellation. Borrowed accessor references expire with their owner or mutation
according to that type's contract.

## Execution

`StateTracker` and transition machines require caller synchronization.
Versioned storage synchronizes its value/revision, while callbacks protect their
own external targets. Select a [Dispatch](dispatch.md) owner when an observer
must change simulation state from another producer.

## Integration

Sample an application-owned score during update:

```cpp
CE::State::StateTracker<int> score{0};
score.update(10);
if (score.changed()) {
    const int gained = score.delta();
    // Use gained while preparing the next HUD content update.
    static_cast<void>(gained);
}
```

The tracker owns integer samples and introduces no clock. Keep it as a member
to compare successive updates rather than reconstructing it every tick.

## Limits and further reading

State publication is not event history, a completion barrier or automatic
thread assignment. Read [state and utility contracts](../runtime/utility-contracts.md),
[StateTracker](../../../projects/engine/include/cheryl/templates/state-tracker.h),
[VersionedVariable](../../../projects/engine/include/cheryl/templates/versioned-variable.h),
[ObservedVariable](../../../projects/engine/include/cheryl/templates/observed-variables.h)
and [StateMachine](../../../projects/engine/include/cheryl/templates/state-machine.h).
