# Map-storage U3 local checkpoint and next action

U3 implements the private CPU CLI over accepted U2F
`ef1d0ac741cdbe888d06d3f6216f8d1d2104f077`. The codec/adapter, versioned format,
resource limits and native path semantics are unchanged. All **136 focused cases
passed**, with no failures/skips: the exact 122 accepted file/codec names, two
formatter cases and twelve process cases. Coordinator acceptance of this local
U3 checkpoint is next. No SDK extraction, integration or later phase was started.
The coordinator publisher owns checkpoint transport; this worker does not push
or alter remotes.

## Small consumer and implemented behavior

```text
map_storage_cli round-trip <file>
map_storage_cli verify <file>
```

Both require exactly one explicit nonempty path. Round-trip deliberately writes
the literal asymmetric 3x2 fixture through save (and may replace an existing
regular file), loads it and compares the complete owned Snapshot. Verify only
loads/compares; it never creates, rewrites, repairs or supplies defaults. No
implicit filenames, asset search, cwd changes, interaction or automatic retry
exist. Standalone --help exits 0 and usage states the writing/read-only behavior.

Success 0 prints map ID, dimensions, ordered layers and named entry to stdout.
Storage/format rejection 2, valid mismatch 3, usage 64 and unexpected failure 1
use stderr. Owning storage diagnostics retain category, operation, origin/location,
known byte offset/native error, primary detail and separate cleanup/leftover
fields. Completed saves are explicitly reported if a later load/comparison/output
failure occurs; those failures do not claim old-byte preservation. Stdout is
flushed/checked, stderr is untied from stdout, and SIGPIPE is ignored only in the
CLI process where available. Full contracts, confidence and alternatives are
[MS-008](agent-notes.md#ms-008-private-cli-contract), with matching
[decisions](decisions.md).

## Actual configure/build/native commands

All commands ran from this clone through the existing shared wrapper, one job,
reusing `build/map-storage-u1-debug` (Ninja, GNU C++ 16.2.1, Debug). Complete
stdout/stderr and final statuses are retained under `u3-logs`. Each command also
captured `$?` to its matching `.exit` file and returned that status. The original
configure session remained active until completion; it was not cancelled or
relaunched while waiting under the shared wrapper. No resource/admission refusal occurred.

```bash
/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/tests/map-storage -B build/map-storage-u1-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > build/map-storage-u1-debug/u3-logs/configure.log 2>&1
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u1-debug --target map_storage_cli map_storage_tests --parallel 1 > build/map-storage-u1-debug/u3-logs/build.log 2>&1
/mnt/projects/cheryl-features/scripts/build -- build/map-storage-u1-debug/map_storage_cli round-trip '/mnt/projects/cheryl-features/projects/map-storage/build/map-storage-u1-debug/u3-logs/smoke-xhnmggww/fixture.map.json' > build/map-storage-u1-debug/u3-logs/round-trip.stdout 2> build/map-storage-u1-debug/u3-logs/round-trip.stderr
/mnt/projects/cheryl-features/scripts/build -- build/map-storage-u1-debug/map_storage_cli verify '/mnt/projects/cheryl-features/projects/map-storage/build/map-storage-u1-debug/u3-logs/smoke-xhnmggww/fixture.map.json' > build/map-storage-u1-debug/u3-logs/verify.stdout 2> build/map-storage-u1-debug/u3-logs/verify.stderr
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug -R '^map_storage\.' --show-only=json-v1 --no-tests=error --parallel 1 > build/map-storage-u1-debug/u3-logs/discovery.json 2> build/map-storage-u1-debug/u3-logs/discovery.stderr
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug -R '^map_storage\.' --output-on-failure --no-tests=error --parallel 1 > build/map-storage-u1-debug/u3-logs/all-tests.log 2>&1
```

Every command exited 0. Configure used existing `/usr/bin/python3.14` (3.14.7),
standard library only; no package/tool/dependency download, install or pin change
was needed. The build emitted no warnings. JSON remains privately pinned to
3.11.3 (`b36f4c477c40356a0ae1204b567cca3c2a57d201`), gtest remains
`6dae7eb4a5c3a169f3e298392bff4680224aa94a`. No asset input was required.

Discovery was nonempty and compared by exact names against accepted
`u2f-logs/discovery-after.json`: all 122 retained, plus 12 `map_storage.cli.*`
and two `map_storage.cli_output.*` cases. The comparison is saved in
`u3-logs/discovery-comparison.json`. CTest executed 136/136 passes in 9.19 seconds
(9.05 summed test seconds), no skips. `u3-logs/all-tests.log` and the updated
`Testing/Temporary/LastTest.log` retain full results/per-case output. No passing
broad selection was repeated and no test failure or acceptance gap was hidden by
zero discovery or runner status alone.

## Actual independent smoke and process outcomes

The standalone commands used a unique `u3-logs/smoke-xhnmggww` child. Both exited
0, wrote no stderr and printed:

```text
round-trip: map=fixture:two-layers dimensions=3x2 layers=[ground,overlay] spawn=entry@ground(1,1)
verify: map=fixture:two-layers dimensions=3x2 layers=[ground,overlay] spawn=entry@ground(1,1)
```

The first bytes were saved as `round-trip.bytes`. Separate verification left
exact bytes unchanged; decoded content also equals the independent Python literal,
including all twelve values/empties and entry on ground at (1,1). SHA-256 before
and after was `9704e93957e9fe26ae30476e12879ebc4d7c47d85d070cef4c2b90bf7c087a1e`.
`smoke.json` retains the explicit file path. These were actual separate executable
invocations before the automated CLI selection, not an in-process main shim.

The twelve process cases made **71 actual CLI invocations**, each with saved
stdout/stderr, PID, argv, cwd and expected/actual exit under unique build-local
`cli-checks/<case>-<suffix>/calls/` children. `process-summary.json` inventories
these retained artifacts. Intentional outcomes were 18 exits 0, two exits 1,
16 exits 2, 25 exits 3 and ten exits 64; all matched expectations. These nonzero
child results are deliberate fixtures, not failed CTest checks.

- Round-trip/fresh verify and repeated replacement compare against an independent
  literal and exact deterministic bytes. Twenty-three valid differing documents
  exercise map ID, dimensions, layer order/count/identity, each cell/empty in both
  layers, each spawn field and extra/absent spawns; every verify returns 3 and
  leaves bytes unchanged. Equality compares all owned fields, not counts/sizes.
- Malformed/truncated JSON returns 2 with category/operation/origin/location and
  known byte offsets. Unsupported integer version 2 and another format return 2
  with their specific categories/fields. Existing files stay byte-for-byte intact.
- Missing file/parent, final/dangling symlinks, directory/FIFO targets and all
  missing/extra/unknown/empty-path argument cases return expected rejection/usage
  statuses. Verify creates/repairs nothing, including on absence/failure. The
  profile tests also preserve symlink targets and rejected-entry identities.
- Absolute/relative ancestor-symlink/.. calls select the native parent, retain
  untouched lexical-neighbor bytes, and work for both existing/absent native
  targets. Child working directories are set at process creation; the CLI never
  changes cwd. The four accepted U2F regressions also pass unchanged.
- The normal CLI's genuine failed-write case ran as UID 1000 on this clone's ext4:
  owned parent mode 0500 prevented mkostemp, yielding exit 2, category Open,
  operation save, location $create and native `generic:13` (Permission denied).
  Exact predecessor bytes remained and verify returned 3 before/after, proving a
  valid full reload of intentionally different A. Status sequence was 3,2,3,0,0:
  finally restored original parent mode 0755, explicit round-trip retry and
  separate verify then succeeded. No target-read-only trick or CLI fault control
  was needed; the disposable directory alone was changed/restored.
- Read-only stdout descriptor and pipe-without-reader fixtures each returned 1
  after save, reported `unexpected failure during output` and `save completed;
  subsequent failure does not undo that publication`. The published literal
  remained readable; independent verify returned 0 after each. The stdout
  sentinel was unchanged and no old-byte/rollback claim was made.
- Two formatter cases check known offset/native errors and combined primary plus
  cleanup error/owned leftover. They retain primary details/category/location;
  combined cleanup failure is injected directly in the private formatter test,
  not claimed as a native CLI cleanup fault. Accepted U2 tests still exercise
  actual staging ownership and adapter cleanup error behavior.

Earlier U1/U2/U2F command/result records remain in their committed next.md views
at `847d2a9c8fa8c190d2b55e0e59b596a9cbc4ff54`,
`4751b271b1c7c75ae8951d71cddd1e8e303930f6`, and
`ef1d0ac741cdbe888d06d3f6216f8d1d2104f077`. The accepted U2F handoff was also
retained locally as `u3-logs/accepted-u2f-next.md`; earlier unit log directories
remain intact. CTest's standard LastTest.log now contains this U3 run.

## CPU boundary, limits and next action

Generated `compile_commands.json` lists only codec.cpp, file-storage.cpp, cli.cpp,
the three focused test sources and pinned gtest-all/gtest_main. `build.ninja`
links `map_storage_cli` with only its cli.cpp object and `libmap_storage_codec.a`;
the gtest archives appear only on `map_storage_tests`. Codec JSON includes remain
private. Source/link inspection and actual headless CLI execution establish no
Engine/platform/graphics target, renderer, active game, asset lookup or sibling
requirement. No shared build/test infrastructure or SDK export changed.

There is no unresolved U3 implementation/prerequisite blocker. The 4 GiB aggregate,
3 GiB throttling, 2 GiB worker, no-swap and 4 GiB host-admission policies remain;
all native work used one job. A future wrapper exit 75 requires exact error
recording and idle without unchanged retry. Normal shared-lock queueing requires
retaining its live session. No subagent or external agent was started.

Acceptance remains the controlled local Linux/ext4, caller-owned stable tree,
regular-or-absent file and serialized-writer profile. No fsync/power-loss, hostile
race, network/media corruption, multiwriter, save-slot or general save-game
contract is added. Permission-fault reliability is demonstrated on this host;
privileged/DAC-override environments would need their own reliable test-only
fault boundary. Non-Linux and other toolchains, sanitizers/fuzz, Engine integration,
installed SDK and device/GUI acceptance were not attempted. Unknown C++ exception,
SIGPIPE-setup failure and stderr-reporting failure fallbacks are source-reviewed;
the executed unexpected-output cases cover read-only stdout and a broken pipe.
No OOM was induced.
Confidence is high in the checked private behavior/isolation, medium outside this
profile and in private output wording. These are engineering choices, not user
taste inferences; reversal remains limited to the consumer/formatter/harness.

Stop for coordinator acceptance of this U3 checkpoint. The brief's private CPU
consumer has been demonstrated here; public extraction, durability/platform work,
new consumers, level lifecycle and sibling integration need a later assignment.
No automatic phase progression or recurring scheduling follows this commit.
