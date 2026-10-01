# Task 9 validation record

1 October 2026. Remote checkpoint 90,
`a785c0e35e97a904df67b19af0d65a17d5ee1170`, contains the complete audit series.
Its tree exactly matches saved local 90, `2299004dfa8f9d83461a79c99cb712c05b4c2731`;
all 33 applied patches 58–90 preserve ordered author/date/message records.
The validated C++ revision after the fixture correction is checkpoint 91,
`d59d199a040ffb55856b692a17aecf81e9d29781`. Checkpoint 92 changes documentation only.

## Executed results

| Configuration | Compile/link | Full aggregate run | Skipped / disabled |
| --- | --- | --- | --- |
| Sandbox Release | All default targets pass; the existing sandbox option excludes the demo. | 330 passed, 0 failures, 0 errors. | 0 / 0 |
| Normal Release | All default targets pass, including `demo`. | 332 passed, 0 failures, 0 errors. | 0 / 0 |

The first sandbox run at 90 ran 330 tests: 328 passed and two failed. The owned-root
rollback and dedicated simulation-thread startup fixtures expected `failed_operation`
from requests already accepted and then cancelled during cleanup. Both dispatcher
headers specify `future_error/broken_promise` for cancellation; `failed_operation`
rejects new submissions after closure. Checkpoint 91 checks validity/readiness and
the exact `future_errc::broken_promise` code. Callback suppression, capture release,
worker/upload completion and original-startup-error preservation assertions remain.
Production behavior is unchanged. Both configurations were rebuilt and their complete
aggregate suites rerun after the correction, returning zero in about 1.2 seconds each.

Real Linux affinity cases passed: required pinning, mask restoration between groups,
revalidation after a job changes the mask, and overlapping weighted CPU groups.
The host allows nine CPUs; no affinity case skipped. Controlled policy/thread-start,
allocation and context-failure fixtures also pass. Recording OpenGL and in-memory
runtime fixtures establish those scenarios only, not real driver/GLFW/Gainput behavior
or exhaustive interleavings/race freedom.

## Configuration and execution

GCC 13.3.0, C++23, CMake 3.31.10, Python 3.12.14, Jinja2 3.1.6 and Unix Makefiles.
All ten submodules use the repository's pinned revisions. Both builds select Release
and `CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST`, so compilation does not run
tests. GLFW examples/tests/docs are disabled. Normal enables X11, disables Wayland
and disables Gainput samples/tests. No sanitizer execution is claimed.

Build tools and Jinja2 were installed under the validation workspace. System package
installation failed on identity/cache permissions. Twenty-nine Ubuntu development
packages were downloaded and extracted into `task9-sysroot`; development-library
links use already installed runtime counterparts. Normal configuration uses this
workspace prefix/include/linker path. Sandbox uses its GL linker path. No repository
or vendor source was changed for environment setup. The initial sandbox build returned
nonzero; the final configured build and both subsequent rebuilds returned zero.

Configuration/build logs and CMake caches in the delivered validation archive retain
the effective flags and paths. The successful build commands use:

```sh
validation_root=/workspace/scratch/c067ba71f9ed
validation_cmake="$validation_root/task9-tools/cmake/data/bin/cmake"
PYTHONPATH="$validation_root/task9-tools" "$validation_cmake" \
  --build "$validation_root/task9-build-sandbox" --parallel 3
PYTHONPATH="$validation_root/task9-tools" "$validation_cmake" \
  --build "$validation_root/task9-build-normal" --parallel 3
```

Each build's `all-tests` executable ran with `--gtest_output=xml:<report-path>` under
an external 120-second timeout. Configurations ran serially to avoid shared test-file
interference. The archive preserves the initial failing sandbox output/XML and both
corrected full-suite outputs/XML; discovery was not substituted for execution.

## Remaining acceptance and next work period

Task 9.5 stays open. Next establish a real GLFW/OpenGL execution surface and exercise
finite sequential/concurrent demo runs. This workspace has no DISPLAY, Wayland session
or Xvfb installation; the compiled demo was not executed. New native scenario sources
belong in the existing aggregate target rather than extra regression executables.

Remaining checks include real timing/polling/backpressure and slow presentation,
resize/close, successful/failed material reload, cache/frame retention, idle native
deletion, selected-context switching/recovery and shutdown/failure cleanup. Real font
parsing/rasterization, stb internal allocation failure and rotated FFont rendering
remain open without expanding preexisting FFont loading feature TODOs. Actual OS
policy rejection/restriction changes beyond the executed mask scenarios remain
separate from controlled adapter failures.

Work periods target 20 minutes, with up to ten more for a safe checkpoint, patches
and results. Unfinished investigations carry forward. PR metadata publication (9.7)
remains open; the local description reflects current evidence. No push or PR write occurred.
