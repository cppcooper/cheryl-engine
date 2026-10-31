# Procedural generation — U18 completed bounded publication proof

U18 starts at exact clean2a1096ada736de1cab3e545a31cf89d23ac63761 on
feat/procedural-generation. The interrupted attempt's source and failures were
retained in the same clone, then continued by the user's repeated U18 request.
Manifest/task/thread and U17 receipt hashes match. U17 remains partial/false;
U16 remains blocked/false. Neither predecessor is retrospectively accepted.

The new private BackgroundWeather caller owns one producer thread, three immutable
endpoint slots, at most four borrowing pins and one pending update/restore.
U17's actual Engine-owned weather buffers, immutable baseline and typed retained
workspace remain. Request admits one tick with existing profile/batch work bounds.
Foreground pin/sample/adopt/retire handles fixed metadata/scalars; explicit
retirement plus final pin release lets the producer destroy/reclaim heavy owners.
A full held slot set refuses work. Late/refused production leaves the previous
complete endpoint readable. No universal scheduler or growing queue is introduced.

Tokens bind stream, epoch, slot generation, request, sequence and exact tick
interval. Simulation commits each contiguous interval once using both its cursor
and the runtime marker; reads never replay rain/soil transfers. Foreign/stale/
duplicate/out-of-order requests/adoptions refuse without partial publication.
Restore/reconfiguration explicitly increments epoch, invalidates adoption tokens,
discards obsolete work and resets allowance/scratch for a new base. All-held slots
defer this base until an old pin releases; held old snapshots stay readable/tagged.

Pins borrow the runtime and must finish before its destruction. Hot metadata
operations try-lock/refuse busy; final release takes a short fixed metadata mutex
to avoid a missed wakeup. No solver or heavy buffer cleanup holds that mutex.
close is a control operation: mark closing, refuse held pins, release them, close
again to join after producer drain. In-flight bounded work is discarded. No
lock-free or foreground wall-clock guarantee is claimed. See precise ownership,
ordering/error/restore contracts in [PROCGEN-059/060](agent-notes.md#procgen-059--u18-bounded-publication-identity-and-ownership).

Executed evidence (all through the shared one-job build wrapper):

- Release53/53 focused CTest entries:147 deterministic protocol assertions plus
  3912 existing Engine/climate/moisture/weather/regional assertions.
- Engine Debug/UBSan54/54, including both real callers and the same protocol cases.
- Preserved default Debug/UBSan161/161:48017 assertions, unchanged option defaults.
- Real32x32/60s and64x48/120s terrain -> baseline -> weather -> regional -> producer
  -> pin/sample/adopt/retire -> joined shutdown. Exact serial-result equivalence,
  independent conservation, exact tick/sample age, full/generation reuse, paused
  reclamation, restore while computing/fully held, allocation exception/budget
  poison, old snapshots and shutdown during compute pass with deterministic gates.
- Release profile exit0: four actual updates per grid, eight foreground/solver
  rows, ten endpoint reclamation rows and two shutdowns;15s native watchdog.
  Foreground Engine allocation/free/checkouts and instrumented grid copies are0.

Finite raw timing medians in ms on Ryzen7 7840HS, GNU16.2.1 Release -O3, Linux
7.2.11 (no warmup; four samples/operation/grid, five reclamations/grid):

| Grid | Pin | Read | Adopt | Retire/release | Solver | Endpoint reclamation |
| --- | --- | --- | --- | --- | --- | --- |
|32x32|.000126|.000131|.000105|.001223|.641569|.002124|
|64x48|.000170|.000126|.000155|.001287|3.903159|.003777|

Raw samples, maxima, null p95/p99, host/source/compiler context, exact commands and
immediate exits are in ignored builds/procedural-generation/logs/u18/. Shutdown
and control waits are separate. These sparse observations establish no tail,
2ms/cadence, parallel throughput or game/platform/graphics acceptance. Endpoint
reclamation timings exclude final retained workspace/service teardown; shutdown
includes joining/drain. Global pooled storage is not a per-runtime RSS bound.

Earlier build/CTest failures remain: an unchecked expected hid an invalid slope
in the one-metre refusal fixture; explicit flat terrain now reaches the intended
substep refusal. A diagnostic compile typo and the2160.8s interrupted tool wait
are also preserved. No warning/sanitizer policy was weakened. Existing Engine
return-time metadata allocation failure remains fatal; legacy ObjectAllocator,
uninstrumented standard allocations and shared-pool retention remain U17 limits.

See [PROCGEN-U18.json](../../continuation/PROCGEN-U18.json), ready/checks_passed=true
for this selected bounded CPU protocol only. Stop for command-center review.
The next useful action is review of the private protocol/source and finite raw
measurements before any newly assigned cadence/interpolation or Engine integration
unit. No automatic follow-on, worker push, external report or service is launched.
