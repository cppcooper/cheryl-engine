# Map-storage U1 checkpoint and next unit

U1 is implemented and verified: owning snapshots/layers/spawns, value equality,
bounded memory-stream parse/encode, structured owning errors and a standalone
CPU-only test runner. It builds on design checkpoint
`20cd3f74dd94932d18745b1e0d90eb68538378f7`. No path-based load/save, replacement,
CLI, public SDK export, renderer/asset lookup, tilemap import or level lifecycle
was implemented. U1 does not complete the full map-storage acceptance milestone.

## Actual verification

The following commands ran from this clone through the shared build wrapper,
using clone-local `build/map-storage-u1-debug`, GNU C++ 16.2.1, Ninja and Debug.
The dependency pins are unchanged: JSON
`b36f4c477c40356a0ae1204b567cca3c2a57d201` (3.11.3) and googletest
`6dae7eb4a5c3a169f3e298392bff4680224aa94a`. No download or asset input was needed.

```bash
/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/tests/map-storage -B build/map-storage-u1-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/map-storage-u1-debug --target map_storage_tests --parallel 4
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/map-storage-u1-debug -R '^map_storage\.' --output-on-failure --no-tests=error
```

Configuration exited 0. The final incremental build exited 0 without warnings.
The focused CTest selection executed **86 nonempty cases: 86 passed, 0 failed**,
in 6.36 seconds. It comprises 19 codec cases and 67 parameterized rejection
cases. The saved evidence is
`build/map-storage-u1-debug/Testing/Temporary/LastTest.log`, completed
2026-10-22 at 15:54 PDT; it contains 86 passed invocations. Source timestamps
precede the tested binary and completed log. Native success was not inferred
from a runner exit or a zero-test selection.

Those commands are historical results from before the memory continuation.
The earlier build used four jobs under its then-current authorization. **Any new
configuration/build/test uses the current wrapper and one job only.** After
resume, unchanged passing native checks were not repeated. The shared 4 GiB
agent/tool cap, 3 GiB throttling, 2 GiB per-worker cap, no-swap policy and 4 GiB
MemAvailable admission threshold remain authoritative. A wrapper exit 75 means
record its exact error and checkpoint idle without retrying unchanged conditions.
No native admission attempt or resource refusal occurred during this completion.

Observed coverage includes:

- All fields, twelve cell values/empties, layer order and entry spawn (ground,
  1,1) of the asymmetric 3x2 fixture; exact deterministic output, arbitrary
  property order, decoded names, independent owned copies and unbound tile keys.
- Malformed, truncated, trailing, second-document and invalid UTF-8 input;
  valid JSON followed by raw NUL alone, NUL+junk or NUL+another document.
- Owning category/origin/operation/location diagnostics for duplicate decoded
  names (including escaped equivalents), unknown/missing members, format/version,
  numeric bool/float/exponent/overflow, identity/reference/coordinate/count errors.
  Early duplicate/depth failures can precede later syntax errors.
- Byte, decoded-string/key, depth, array, object-member, node, dimension,
  layer/spawn, total-cell and encoded-byte boundaries, including exact 8 MiB input
  and output, one detection byte, and growth rejection during SAX construction.
- Failed assignment preserving an existing snapshot; encoder revalidation;
  retained diagnostic lifetimes; injected signaled I/O failures; unchanged stream
  flags/masks; allocation and programming exceptions propagating unchanged.

Generated `compile_commands.json` lists only codec, tests, gtest-all and
gtest_main sources. The codec command uses this clone's private JSON include
directory and a 3.11.3 static assertion. `build.ninja` links the runner with only
`libmap_storage_codec.a`, `libgtest_main.a` and `libgtest.a`; no Engine/native/
graphics target is involved. The standalone build does not use
`cheryl_add_test_suite`. No shared build infrastructure was changed.

## Remaining limits and next action

There is no unresolved U1 implementation or prerequisite blocker. Verification
is limited to this GNU/Linux Debug CPU runner; no Windows, installed SDK,
sanitizer/fuzz campaign, Engine build or device/GUI acceptance was attempted.
Resource budgets bound input and logical DOM growth, not exact allocator bytes
or latency. A buffer's reported EOF is treated as EOF; silent file/device errors
are outside this memory-stream unit. Full CLI persistence and failed-write
preservation remain unexecuted.

The next useful unit is **U2: private file publication adapter**, after a later
coordinator assignment. Use [MS-006](agent-notes.md#ms-006-failed-write-preservation)
with the implemented codec and [MS-007](agent-notes.md#ms-007-integration-and-implementation-placement).
Add explicit-path load/save and exclusive same-directory staging, checking all
writes/close before one Linux replacement. Keep the codec independent of file
ownership and preserve an old valid file on every failed precommit operation.

U2 checks must cover real successful replacement and an actual OS replacement
error against a valid old file, plus deterministic staging-create, partial-write,
close/flush and cleanup failures. Check exact prior bytes and successful reload,
absent-target preservation, ownership of temporary files, and successful retry.
Mocks alone do not establish filesystem acceptance. Keep CLI publication as U3;
do not start U2, broaden the schema or schedule recurring work from this checkpoint.
