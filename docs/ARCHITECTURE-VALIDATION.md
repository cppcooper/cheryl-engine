# Task 9 validation record

Latest checkpoint 98 records successful complete builds and 337 normal / 330
sandbox tests, including five real-context cases without skips. The validated
C++ commit is `00f8ca67534c2597266e6590a4a226e2892e73a6` (97). Earlier execution
records below retain their checkpoint scope.

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

## Native execution: checkpoints 93–95

Remote 92 is now `f8ab42f5cb4f10ae7c5379795054d879a0cf2a64`. Its exact tree and
ordered patch metadata match saved local 92. Checkpoints 93–94 add the existing
demo's `--max-updates=N` option and two opt-in aggregate native-context tests.
The validated C++ revision is `4161ec04857f08b346114f1640a116feb6cbffbc` (94).

A real TCP Xvfb session supplies an X11 display on Mesa 25.2.8 llvmpipe (LLVM 20.1.2),
OpenGL core 4.5. This is a software driver, not physical-GPU acceptance. Unix sockets
are rejected with EPERM in this workspace. The server and clients must run in the
same execution session; separate commands could not connect. Xvfb/XKB/font packages
were extracted into the existing workspace prefix, with an extracted xkbcomp helper
linked at the server-required path. TCP sessions use a private authorization file
and terminate after validation; authorization bytes are not part of the archive.

Both complete Release builds pass. With `CHERYL_NATIVE_GL_TESTS=1` and the real
DISPLAY, the full normal suite passes **334 tests**, including both native cases;
the sandbox suite passes **330**. Neither run has failures, errors, disabled cases
or skips. The new native file is excluded by its sandbox preprocessor guard and
uses the existing aggregate target. Unrequested native cases explicitly skip in
normal builds; requesting them with an unusable display does not count as a pass.

Native observations: provider texture storage remains live after final foreign
release until owner maintenance runs without drawing. Selecting another unshared
context rejects image use/collection; renderer shutdown selects its own context,
deletes retained texture storage, closes resource use and supports final foreign
release after both windows/the engine are gone. `glIsTexture` and `glGetError`
observe actual driver state.

Eight finite real-demo runs each complete twelve updates and exit zero: both modes
under variable stepping with finite input, variable stepping with unlimited input,
fixed drop with 3 ms steps/capacity one, and fixed hybrid with 3 ms steps/prefix one/
maximum two fixed updates. Real font parsing/baking/atlas upload, shader linking,
packet preparation, drawing/presentation and normal cleanup execute. These runs
exercise configurations without proving forced catch-up, input responsiveness,
pixel appearance, presentation pacing, interactive resize or reload.

At checkpoint 95, manual observations were assigned to the user's desktop in
[NATIVE-DESKTOP-CHECKS.md](NATIVE-DESKTOP-CHECKS.md): both modes, camera/mouse/text
focus, resize, successful/failed/recovered F5 reload and normal window close.
Checkpoint 96 records the subsequently reported result below.
Next automated work can cover real material/program reload and forced native
failure paths while desktop checks supply hardware/input/visual evidence.
The original remaining-acceptance section below describes the prior checkpoint;
9.5c/d/e/f remain open for their outstanding acceptance. Rotated FFont, internal stb allocation
failure and remaining OS restrictions remain separate. No push or PR write occurred.

## User desktop observations: checkpoint 96

The remote integrates 93–95 at `2c23bf8164ac69e74a6e06bd999ef54a61d7c8f4`.
Its exact tree and ordered author/date/messages match the delivered checkpoint.
After the copied-asset F5 sequence was clarified, the user reported running the
checks without failures and with everything behaving as expected on 1 October.
[NATIVE-DESKTOP-CHECKS.md](NATIVE-DESKTOP-CHECKS.md) preserves that report and the
repeatable commands. The supplied manual checklist is accepted as a reported pass:
camera/mouse/text focus, resize, valid/failed/recovered reload and normal close,
with sequential/concurrent modes requested by the checklist. Hardware/driver
identity and exact local build flags were not supplied. This documentation-only
checkpoint does not change the automated counts or validated C++ revision.

Outstanding 9.5c–f acceptance remains forced timing/backlog/presentation overload,
further native resource/reload/failure combinations, internal stb allocation
failure, rotated FFont output and remaining actual OS policy restrictions/rejections.
The user's report closes the manual checklist, rather than those separate scenarios.
9.7 publication remains open. Next independent work can exercise real reload/failure
paths and forced overload. No push or PR write occurred.

## Native retained-frame and failure acceptance: checkpoints 97–98

The user will skip applying documentation-only 96 separately. The next pending
bundle therefore includes **96–98** from confirmed remote 95,
`2c23bf8164ac69e74a6e06bd999ef54a61d7c8f4`, with its recorded desktop pass.

Three additional opt-in cases execute against actual Mesa llvmpipe OpenGL objects:

- A fragment-source change swaps red and green while the authored color stays red.
  Old and new retained frames still produce their distinct pixel readbacks after
  cache clear and external-owner release. Recycling and idle maintenance delete
  programs, vertex arrays and buffers at the observed lifetime boundaries.
- Compile failure, link-interface mismatch, a missing fragment after successful
  vertex compilation and post-link reflection rejection preserve the published
  material and its red frame output. Forwarding only native create entry points
  records actual driver IDs; shader/program queries confirm partial cleanup and
  collection of the adopted rejected candidate. Entry points restore on every exit.
- Renderer shutdown closes a still-bound frame's program, vertex array, buffer and
  texture while the borrowed window/context remains alive. Closed program/image/
  geometry use rejects before and after engine destruction. Final logical owners
  then release on another thread.

The initial five-case native run passed four and failed shutdown: `glIsProgram`
remained true after shutdown while the window was still alive. Deleting the current
program defers its deletion until it is unbound. 97 unbinds after owner/current-
context validation before idle collection and before shutdown. Later draws select
complete program state. The final idle case deliberately leaves its last program
bound, so neither collection nor shutdown relies on a later draw/window destruction.

Both full Release builds pass after the fix. Native-requested normal execution
passes **337** cases, including all five native cases; sandbox passes **330**.
Failures, errors, disabled cases and skips are all zero. The archive retains the
initial failure, final full logs/XML, build commands and the bounded same-session
Xvfb runner. Private authorization files are excluded and temporary displays/helper
links are cleaned up. Formatting and whitespace checks pass. 98 changes only docs;
no repeated build/test run is needed for that checkpoint.

9.5d.1 is complete for these scenarios. 9.5c–f remain open for forced timing/input/
presentation overload, further native fault/context-loss combinations, internal
stb allocation, rotated FFont and remaining actual OS restrictions/rejections.
The user's existing desktop report is preserved; these source changes affect native
maintenance/shutdown, which the native cases execute. No new desktop checklist is
assigned in this period. 9.7 publication remains open; no push or PR write occurred.

## Earlier acceptance plan (checkpoint 92)

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
