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

```{graphviz}
:caption: State integration
:alt: Game components use State for sampling, publication and transitions. Assets, Events and Memory use singleton helpers. State operates on caller-defined values and invokes callbacks on the setter or trigger caller.

digraph state_usage {
    rankdir=LR;
    state [label="State", penwidth=2];
    game [label="Game components"];
    services [label="Assets / Events / Memory"];
    values [label="Application values /\nState and Trigger types"];
    callbacks [label="Application callbacks"];
    game -> state [label="samples / publishes values;\ntriggers transitions"];
    services -> state [label="uses singleton helpers"];
    state -> values [label="copies / compares;\nevaluates transition rules"];
    state -> callbacks [label="invokes observers / hooks\non setter / trigger caller"];
}
```

### Uses

The supplied value types define comparison, copying, assignment and pointee
ownership. Callback captures retain their own dependencies; local state
utilities require no runtime, graphics, input or worker service.

| Types | Systems |
| --- | --- |
| [Caller-defined `T`](../runtime/utility-contracts.md#sampled-and-synchronized-state)<br>[Caller-defined `State` and `Trigger`](../runtime/utility-contracts.md#transition-tables)<br>[`std::mutex` / `std::condition_variable`](../runtime/utility-contracts.md#sampled-and-synchronized-state)<br>[`std::function`](../runtime/utility-contracts.md#transition-tables) | — |

### Used by

Existing services use deliberate singleton construction. Games can separately
choose sampled values, synchronized publication, observers or transition
machines; the engine prescribes no application state-machine architecture.

| Types | Systems |
| --- | --- |
| {cpp:struct}`CE::Assets::Loading::Loader`<br>{cpp:struct}`CE::Events::EventSystem`<br>{cpp:struct}`CE::Memory::Objects::Pool`<br>{cpp:class}`CE::Logging::Log`<br>[Application-owned state models](../runtime/utility-contracts.md#transition-tables) | [Assets](assets.md)<br>[Events](events.md)<br>[Memory](memory.md)<br>[Logging](logging.md) |

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
