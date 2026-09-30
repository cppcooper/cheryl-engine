# Simulation timing

`GameRuntime(engine, game, mode, polling, timing)` uses the same
`SimulationTimingOptions` in sequential and concurrent execution. Execution mode
selects the owning thread; it does not select a different timing policy.
`SimulationScheduler` receives monotonic clock values through `advance(now)` and
`next_update_at()`. It never polls input, calls the game, or sleeps. Regression
sources can therefore describe clock advances without depending on OS scheduling.

The default is variable simulation with a nominal 1/60-second pacing interval.
Its delta is the full elapsed time between selected updates, including time spent
updating, preparing a frame, or waiting. A zero variable interval permits unpaced
updates. The independent fixed step defaults to 16,666,667 ns, the nearest integer
nanosecond to 1/60 second. Input polling spacing/capacity remain separate options.
Sequential execution still shares a thread with presentation, so blocking graphics
or simulation can delay eligible polls; concurrent execution can poll during an update.

In fixed mode the scheduler accumulates observed elapsed time. A normal update
always uses `fixed_step`, regardless of the duration reported by its input view.
`max_fixed_updates` bounds the fixed sequence selected in one scheduler cycle;
the default is one. `DropExcessLag` discards excess whole steps and retains only
the sub-step remainder. With a 30 ms step, two allowed updates, and a 500 ms stall,
the batch uses 60 ms, drops 420 ms, and retains 20 ms for the next deadline.

`VariableCatchUp` applies when the due whole-step count exceeds
`max_fixed_updates`. `fixed_updates_before_recovery` selects an explicit fixed
prefix, between zero and that limit. Zero requests one larger update directly.
The prefix is followed by exactly one `UpdateKind::VariableCatchUp` update using
the remaining accumulated time, capped at `recovery_cap` (100 ms by default).
A zero cap explicitly disables that limit. The rest is dropped; no lingering debt
or fractional remainder remains after this larger recovery. The total hybrid
batch can contain at most the configured prefix plus one update.

For a 20 ms fixed step, a 500 ms stall, and a 100 ms cap: direct recovery uses
100 ms and drops 400 ms. A two-step prefix uses 40 ms, then 100 ms, and drops
360 ms. The stall never implicitly requests all 25 fixed updates. Time spent
executing the chosen batch enters the next cycle, rather than extending the
current batch while it is running.

Every actual update first drains a detached simulation-mailbox batch, then
transfers the whole currently available polling backlog. With no due update,
input remains pending. Later recovery updates get newly completed polls or
persistent State; edges, counts, relative motion, Events, and Text from an earlier
update are never replayed. Mailbox work posted during its drain belongs to a later
actual update, which can be another update in the same bounded recovery batch.

`TickContext::delta_seconds` is simulation time. `update_kind` explains whether
that delta is variable, fixed, or larger recovery. `observed_seconds()` and all raw
`TickInput` durations use the real consumption interval. `dropped_seconds` reports
the batch's discarded time once, on its final selected update; sum it across ticks
instead of multiplying it by the number of updates. An interrupted batch has no
guaranteed final timing report because simulation is ending.

`button_simulation_seconds(action)` is a derived movement policy. It multiplies
the observed down-time fraction by the selected simulation delta. A 50 ms tap
inside a 500 ms observation interval contributes 2 ms to a 20 ms fixed update,
or 10 ms to a 100 ms recovery update. The raw down duration remains 50 ms. With a
zero observation interval it uses current held State. This does not reconstruct
when a tap happened within past fixed steps. A released tap is not replayed by a
later recovery call; applications needing a different control policy can inspect
the unchanged raw State/records. The demo uses this named helper explicitly.

Frame preparation occurs at most once after a bounded update sequence, from its
final useful state. Sequential mode retains one complete frame; concurrent mode
retains the platform's current slot while publishing through the remaining slots.
Rendering can reuse that complete frame on a cycle without a new publication.
Superseded frames recycle on the graphics owner. Simulation never waits for a free
render slot; publication may be skipped while updates continue.

Long suspension is elapsed time under this policy, rather than an implicit pause.
Fixed recovery can therefore slow simulated time or drop it; variable mode can
produce a large delta. A recovery cap bounds one update, not the total wall time
or duration of a fixed prefix. Large deltas can skip collisions, triggers, or
intermediate animations: physics/substepping and render interpolation remain
separate concerns. Applications wanting pause semantics must implement that policy
explicitly. Clock inputs moving backwards, negative pacing/caps, nonpositive fixed
steps, zero fixed-update limits, and prefixes larger than the limit are rejected
before runtime initialization. Clock-range overflow is rejected or deadlines saturate.

The demo accepts `--fixed`, `--fixed-step-ms=N`, `--variable-interval-ms=N`,
`--max-fixed-updates=N`, `--variable-catch-up`, `--recovery-prefix=N`, and
`--recovery-cap-ms=N`, independently of its polling switches. For example:

```sh
demo --concurrent --fixed-step-ms=20 --variable-catch-up --max-fixed-updates=2 --recovery-prefix=2 --recovery-cap-ms=100
```

Prepared scheduler, tick-input, and runtime regressions have not been compiled
or executed. Real sequential/concurrent acceptance remains open.

TODO: add an optimization configurer that suggests pacing/recovery limits from
measured workloads while preserving explicit user configuration. It must not
silently replace fixed delta, change input history, or introduce interpolation.
