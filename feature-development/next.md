# Map-storage U2 checkpoint and next unit

U2 implements private explicit-path Linux load/save over the accepted U1 codec
at `847d2a9c8fa8c190d2b55e0e59b596a9cbc4ff54` (design checkpoint
`20cd3f74dd94932d18745b1e0d90eb68538378f7`). The final focused Debug selection
executed **118 cases: 118 passed, 0 failed, 0 skipped**: 32 filesystem cases and
all 86 codec regressions. This is local private-adapter verification, not full
CLI milestone acceptance. No CLI, SDK export, Engine link, renderer/asset lookup,
tilemap import or lifecycle work was added. Stop after this local U2 checkpoint.

## Small caller and implemented boundary

```cpp
MapFixture::save(path, snapshot);
auto restored = MapFixture::load(path);
const bool equal = restored == snapshot;
```

The caller creates/controls the directory, supplies an explicit file path and
serializes writers. Empty/NUL paths reject before syscalls. The adapter resolves
once, rejects final symlinks/nonregular entries, and uses checked native reads
through a signaling stream buffer. Reported read errors cannot masquerade as EOF;
input acquisition is bounded to 8 MiB plus one byte. JSON limits and private
format/version rules are unchanged.

Save encodes/validates completely before staging. Exclusive mkostemp creates a
sibling whose prefix cannot equal the destination's name, even for absent targets
with temporary-looking names. Unbuffered writes handle short progress/EINTR;
zero progress fails. Checked close relinquishes descriptor ownership before the
syscall and is never retried. One native rename commits, with no later fallible
work. Cleanup unlinks only the owned staging entry, retains the primary Error,
and attaches cleanup errno and an owned leftover path if needed. Publication is
never automatically retried. Proposed/implemented contracts, alternatives and
confidence are in [MS-005/MS-006](agent-notes.md#ms-005-ownership-and-failure-reporting)
and the matching [decision summary](decisions.md).

## Actual commands and results

All commands below ran from this clone through the shared wrapper, one build/test
job, reusing `build/map-storage-u1-debug` (Ninja, GNU C++ 16.2.1, Debug). Logs are
complete clone-local ignored outputs under `build/map-storage-u1-debug/u2-logs/`;
no truncated tool response is needed for diagnosis. No dependency download,
installation, pin change, assets, device run or documentation build was needed.
The JSON pin remains `b36f4c477c40356a0ae1204b567cca3c2a57d201` (3.11.3), gtest
`6dae7eb4a5c3a169f3e298392bff4680224aa94a`.

Final configuration/build/combined-check commands:

```bash
/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/tests/map-storage -B build/map-storage-u1-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > build/map-storage-u1-debug/u2-logs/configure.log 2>&1
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u1-debug --target map_storage_tests --parallel 1 > build/map-storage-u1-debug/u2-logs/build-reviewed.log 2>&1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug -R '^map_storage\.' --output-on-failure --no-tests=error --parallel 1 > build/map-storage-u1-debug/u2-logs/all-ctest-final.log 2>&1
```

All exited 0. Final build emitted no warnings. The combined check took 8.15
seconds (8.07 summed test seconds), with a nonempty 118-case selection. The CTest
`Testing/Temporary/LastTest.log` also retains final per-case output.

Earlier supporting commands used these exact focused selections:

```bash
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug -R '^map_storage\.(files\.|files/save_failure\.)' --output-on-failure --no-tests=error --parallel 1 > build/map-storage-u1-debug/u2-logs/files-ctest.log 2>&1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug -R '^map_storage\.(files\.|files/save_failure\.)' --output-on-failure --no-tests=error --parallel 1 > build/map-storage-u1-debug/u2-logs/files-ctest-final.log 2>&1
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug -R '^map_storage\.(codec\.|invalid/parse_rejection\.)' --output-on-failure --no-tests=error --parallel 1 > build/map-storage-u1-debug/u2-logs/codec-ctest.log 2>&1
```

The first file selection exited 8: 30/31 passed; embedded-NUL rejection was correct
but its printable Error message was truncated. The diagnostic now prints `\0`
and preserves the exact owned origin. The second selection exited 0: 31/31 in
1.69 seconds; codec selection exited 0: 86/86 in 6.43 seconds. `build.log` and
`build-final.log` record the earlier two successful builds with the same build
command above, redirecting to those respective log names. Review then made
staging prefixes disjoint from target names and added the 32nd file case, which
justified the final rebuild/combined run. Earlier passing results are historical;
118/118 is the coherent final-source result.

U1 history: its accepted CPU-only Debug runner previously passed the same 86
codec cases in 6.36 seconds, before U2. Its earlier four-job build preceded the
memory-policy change; **all U2 commands used one job**. The shared 4 GiB aggregate,
3 GiB throttling, 2 GiB worker, no-swap and 4 GiB MemAvailable admission policies
were retained. No resource/admission refusal occurred in U2. Future exit 75 must
be recorded exactly and leave the worker idle without an unchanged retry. A live
shared-lock wait remains a normal queue, not a reason to relaunch.

## Observed outcomes and limits

Executed on Linux x86_64, kernel `7.2.6-arch2-1`, effective UID 1000, clone-local
ext4. Every real-file fixture used its own `map-files-XXXXXX` child under the
clone's build directory. The fixtures restored/removed only their own files;
no disposable fixture directory remained after the checks.

- Real creation and replacement/reload reconstruct both ordered 3x2 layers,
  all twelve values/empties and the named ground entry spawn (1,1), with exact
  deterministic bytes and independent owned snapshots. A distinct valid B has
  changed identity/cells/spawn coordinates; semantic keys need no installed assets.
- For staging-create, partial-write/ENOSPC, zero-progress, close/EINTR, rename
  EACCES/EINTR and cleanup/EBUSY failures, tests compare exact A bytes and reload
  literal A; the parallel absent-target cases keep the target absent. Explicit
  retries succeed after resolving each fault. There is no flush case because
  production output is unbuffered. Short read/write and EINTR success paths pass.
- A narrow boundary removes only its owned, fully written/closed staging entry
  and invokes **actual Linux rename against existing valid A**. The real syscall
  returns ENOENT; A bytes and reload remain exact, then explicit retry publishes B.
  This establishes native failure behavior, not a parent-permission or full-disk
  claim. A separate real save replaces a mode-0400 target successfully and produces
  mode 0600, proving target read-only mode alone is not a rename-failure fixture.
- Exclusive collision reaches native O_EXCL, preserves the unowned file, and
  performs no cleanup on it. Other staging-like entries survive all tests. Cleanup
  failure preserves primary category/errno/location and exposes a readable B
  leftover; later save does not sweep it, and caller cleanup removes only that path.
- Close-failure injection releases/reuses the descriptor before reporting EINTR;
  the reused fd remains open, proving no retry. Load rejects EIO even after all
  valid JSON bytes were read, preserves the previous snapshot, and checks close.
  Open/fstat failures, relative-path diagnostic ownership, empty/NUL paths,
  final/dangling symlinks, directory/FIFO rejection, invalid/oversized encode before
  staging, exact 8 MiB input and one detection byte, truncation/trailing/NUL paths
  and all 86 bounded codec regressions pass.

Generated compile commands contain only codec, file adapter, their tests, gtest-all
and gtest_main. The final executable link edge in `build.ninja` contains only
`libmap_storage_codec.a`, `libgtest_main.a` and `libgtest.a`. No Engine, graphics
module, renderer, active game, shared test helper or sibling is involved. The
codec's JSON include is still private and pinned 3.11.3; no system JSON is used.

There is no unresolved U2 prerequisite or implementation blocker. Verification
covers ordinary process-visible failures on this local Linux/ext4 profile only.
No power-loss/fsync, network-filesystem, media-corruption, hostile path-race or
multiwriter guarantee is made. Ancestor symlinks are within the caller-controlled
directory assumption, not canonicalized security boundaries. Old permissions,
ACLs and ownership are not preserved; published staging requests mode 0600.
Unexpected allocation/programming exceptions propagate with best-effort cleanup;
leftover fields are attached to anticipated owning Error failures. Non-Linux
rejection is source-reviewed, not executed. No other toolchain, sanitizer/fuzz,
Windows, Engine, installed SDK or device campaign was attempted. Confidence is
high in the tested sequence, medium beyond this filesystem and in the private
API/error shape. Reversal changes the adapter/private tests, leaving codec and
logical identities intact; these are engineering judgments, not inferred taste.

## Next action: proposed U3 only

A later coordinator assignment should add one private CLI consumer with the
literal two-layer fixture and `round-trip <file>` / `verify <file>` operations,
using the existing adapter without generalizing the format. Compare every field,
cell/empty, layer order and spawn in a fresh verification process. Check nonempty
CTest registrations and the proposed exit outcomes: success 0, storage rejection
2 (including primary/cleanup diagnostics), mismatch 3, usage 64, unexpected error
1. Include fresh-process valid round trip, malformed/truncated input, unsupported
version, preserved A after a failed B publication and explicit retry. Retain the
standalone dependency boundary and one-job wrapper. U3 was not started here;
no recurring scheduling or sibling integration is authorized by this checkpoint.
