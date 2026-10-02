# Task 9 validation record

Checkpoint 110 closes bounded task-9 technical acceptance. Validated source 109
adds native runtime failure cleanup and actual inherited affinity restriction/
OS rejection checks. Both complete Release builds pass; all **349 normal / 335
sandbox** cases pass without failures or skips, including twelve real-context,
three real-font and two new OS-affinity cases. The six native failure combinations
preserve the original error, settle accepted CPU/platform work and delete driver
handles after restoring the actual current context. The affinity checks restore
the calling thread's nine-CPU mask after narrowing it to CPU 0; an absent CPU 1023
request reaches the kernel and returns Invalid argument, followed by worker recovery.
Together with prior timing/input/font/demo/desktop evidence, 9.5 is complete for
its recorded scopes. **Only separately requested PR metadata publication (9.7)
remains open.** Earlier sections retain their checkpoint scope; current evidence
is in [ARCHITECTURE-VALIDATION.md](ARCHITECTURE-VALIDATION.md).

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

## Real stb allocation acceptance: checkpoints 99–100

Confirmed remote 95 remains `2c23bf8164ac69e74a6e06bd999ef54a61d7c8f4`.
The delivery includes pending **96–100**; 99 is the source change and 100 records
its evidence. The source revision is `251b924135032a9a6b66856d2c1c10e73745eb68`.

An isolated executable compiles the pinned stb implementation with a real
null-return allocation hook. With DejaVuSans.ttf at 18 pixels, rejecting request
one leaves the positive bake status unchanged at 68 but changes 22 atlas pixels.
With NimbusSans-Regular.otf, rejecting request two terminates the isolated child
with SIGSEGV. The CFF shape path writes into the returned allocation without a
null check; the heap scanline path has a similar unchecked allocation. A positive
bake status alone cannot establish successful rasterization after allocation failure.
The reproducer's source, return codes and output are archived; its intentionally
failing child is separate from the aggregate suite. Font files are not redistributed.

99 routes real stb allocations during loading through a private scoped
`memory_resource` boundary. Production uses `new_delete_resource`; no public API
changes. Allocations throw before stb can consume null storage. Intrusive records
track live blocks without another tracking allocation, and scope destruction frees
scratch left by exception unwinding through stb's manual cleanup. Normal frees
remove their records. Thread-local scope selection and previous-scope restoration
support independent and nested bakes; the latter is executed explicitly. The scope
ends before provider upload, so a failed bake publishes no geometry or atlas.

Three aggregate opt-in cases execute real parsing/rasterization at 128 pixels:

- DejaVuSans.ttf: reject each of **793** requests on its complete ASCII bake trace.
- NimbusSans-Regular.otf (CFF): reject each of **727** requests on its trace.
- Start a failing nested CFF bake while outer TrueType scratch is live, then
  separately allow the outer bake to succeed or fail. Each resource retains its
  own ownership, and both release all outstanding scratch.

Every swept failure checks `bad_alloc`, the precise rejected request, zero live
scratch and zero provider upload calls. Subsequent public/private loads succeed.
Both reference faces have a glyph wider than 64 pixels, exercising heap scanline
allocation as well as shape/contour/edge scratch. These are two concrete printable-
ASCII traces, not every font, glyph range, size or possible interleaving.

Both complete Release builds pass. Full runs with the native and font checks enabled
pass **340 normal / 333 sandbox** cases, with zero failures, errors, disabled cases
or skips; all five real OpenGL cases execute on the same Mesa llvmpipe driver.
Two real demos then complete twelve updates each, sequential and concurrent, with
successful real font bake/upload/render and normal cleanup. The archive includes
raw build logs, full/focused XML and logs, fixture paths/hashes, commands, the bounded
Xvfb runner and replay results. An initial helper count mismatch expected two font
cases after the third was added; all three tests passed in that run. Only the
helper's expected count changed, and the corrected focused/full runs passed.
Formatting and whitespace checks pass. 100 changes documentation only.

To run these optional font cases, set `CHERYL_STB_ALLOCATION_TTF` to a TrueType face
and `CHERYL_STB_ALLOCATION_CFF` to a CFF OpenType face with a glyph wider than 64
pixels at size 128, then run `all-tests --gtest_filter=font_bake.real_*` in either
configuration. Missing paths explicitly skip; unusable supplied paths fail.
The recorded fixture paths are `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`
and `/usr/share/fonts/opentype/urw-base35/NimbusSans-Regular.otf`.

9.5e.1 is complete for this bounded real-font allocation acceptance. Parent 9.5e
remains open for rotated FFont output. Forced timing/input/presentation overload,
further native fault/context-loss combinations, remaining actual OS restrictions/
rejections and 9.7 publication remain open. The user's prior desktop report remains
accepted. No new manual checklist, push or PR write occurred.

## Rotated FFont and image-row acceptance: checkpoints 101–102

Confirmed remote 95 remains `2c23bf8164ac69e74a6e06bd999ef54a61d7c8f4`.
Pending delivery is **96–102**, retaining the earlier unapplied checkpoints.
The source revision is `6c7cafc2b8396aa66c54613643a1db30346bdc57` (101); 102 records evidence.

The first native FFont run failed ten pixel assertions. The geometry's top-left
atlas rectangles use upward-positive UVs, but decoded RGBA rows were uploaded in
unchanged top-to-bottom order. Glyphs therefore sampled the wrong atlas rows,
including incorrect banks. 101 reverses RGBA rows in transient OpenGL upload
storage, leaving the caller's pixels unchanged. Both file/raw Texture construction
and prepared/provider image loading share the conversion. Overflow is checked
before allocating the copy. Red-only stb atlases keep their distinct baked UV row
convention. No common geometry, font layout or preexisting FFont loading TODO changes.

Two additional aggregate native cases execute on Mesa llvmpipe:

- FFont reads a complete binary widths fixture and uses its actual generated glyph
  geometry, a controlled top-to-bottom color atlas and the production shader2d
  shaders. Five retained frames verify upright and quarter-turned asymmetric glyph
  pixels, primary/alternate banks, different bank widths, spaces and the legacy
  newline transformed along the caller's local axes. A clipped edge makes the
  scaled 1/128 newline displacement observable as one pixel. The source image,
  widths file, FFont object, material handles and texture cache are released before
  drawing. Packets alone retain image/geometry/material resources. Maintenance
  keeps live owners, then deletes texture, VAO, buffer and current program after
  all frames recycle. Weak-owner checks and actual GL object queries pass.
- A committed 3-by-2 RGBA PNG exercises real CPU decoding, direct file Texture
  construction, provider file loading and prepared-image upload. Driver storage
  reads match bottom-up RGBA rows, and the supplied decoded pixels remain intact.
  An odd-width 3-by-2 one-channel atlas retains the exact original alpha byte order;
  its readback restores pack alignment. This checks the row conversion's other
  consumers and its boundary with STB, beyond the font's sampled pixel output.

This is real backend playback of controlled FFont data, not a visual-quality claim
for an unavailable external legacy font image or every glyph/transform. Existing
CPU regressions retain unsupported-byte/fallback and layout compatibility coverage.
The widths-file validation/format feature TODOs remain outside this task.

Both complete Release builds pass. Full enabled runs pass **342 normal / 333
sandbox** cases with zero failures, errors, disabled cases or skips. All seven
native and three real-stb allocation cases execute. Both finite real-demo modes
complete twelve updates and exit zero after the production row fix. The final
auxiliary check changes test sources only. The archive retains initial pixel
failure output/XML, final focused/full logs/XML, build logs, driver/commands, PNG
fixture identity, runner and replay results. An intermediate auxiliary fixture
compile error mixed derived/base pointers in a deduced initializer list; an explicit
base-pointer array corrects it without changing assertions. Formatting and whitespace
checks pass. No repeated build/test run is needed for documentation-only 102.

9.5e.2 and parent 9.5e close for these scopes together with 99's real TrueType/CFF
allocation acceptance. Task 9.5 stays open for forced timing/input/presentation
overload, further native fault/context-loss combinations and remaining actual OS
policy restrictions/rejections. The user's existing desktop report is retained;
no new manual checklist is assigned. 9.7 publication remains open. No push or PR write.

## Real-runtime overload acceptance: checkpoints 103–104

The user confirms patches through **98** are applied. A fresh fetch still observes
remote 95 (`2c23bf8164ac69e74a6e06bd999ef54a61d7c8f4`); that does not override the
user's local application report. Pending delivery is **99–104**, based on the saved
98 tree at `19912dd54972dd08bcef3ff96057379a38207be2`. The unique application bundle
is `cheryl-engine-99-104-from-19912dd.patch`, excluding 96–98. The source revision
is `7f3690427011f2d5eec6737f1920058973dc0790` (103); 104 records evidence only.

Three opt-in aggregate cases use real GLFW/Gainput/Mesa OpenGL adapters and
resolved triangle/material packets. None replaces the runtime clock or native GL
calls. All three execute rather than skip:

- Slow updates: sequential and concurrent runs each exercise variable timing,
  fixed drop-excess-lag, capped variable catch-up with one fixed-prefix update,
  and capped catch-up with no fixed prefix. Each of the eight runs deliberately
  spends at least 80 ms inside its first update, then stops after 24 updates.
  Assertions observe the real input-consumption lag, variable elapsed delta,
  unchanged 10 ms fixed deltas, discarded lag and catch-up deltas no greater than
  the configured 15 ms cap. Controlled-clock scheduler regressions separately
  establish exact batch bounds and once-per-batch dropped-time accounting.
- Slow presentation: a delegating native context forwards current-context operations,
  framebuffer reads and GLFW swaps. After its first completed swap, it holds the
  owner inside `present()`. The sequential run deliberately delays 80 ms and
  observes that elapsed time in a later update. The concurrent run instead waits
  on a condition predicate until eight updates finish; the two-second deadline
  only bounds a broken runtime. XML records **8 updates / 2 preparations** during
  that hold: the current frame occupies one slot, publication exhausts the other
  two, and authoritative simulation continues. Presentation subsequently resumes.
  Every actual swap is preceded by a real red-pixel readback of the retained packet.
- Full State backlog: a forwarding native input observer preserves actual Gainput
  snapshots. In concurrent lockstep (capacity one) and finite (capacity three),
  the fourth update holds consumption until real unchanged polls fill capacity.
  Two separate platform drains finish while native poll sequence stays fixed;
  presentation advances between them. Once the update releases, the next update
  receives exactly every retained poll in sequence and subsequent polling resumes.
  No sleeping establishes the drain order or full-batch invariant.

This is bounded native workload acceptance on Mesa llvmpipe, not an induced physical
GPU/compositor stall. The input case forwards real State and creates no hardware
Events/Text; ordered native records during forced backlog remain open. Existing
aggregate recording/input tests and the user's reported desktop focus/resize/close
results retain their separate scopes. No new manual checklist is assigned.

Both complete Release builds pass. Full enabled runs pass **345 normal / 333
sandbox** cases with zero failures, errors, disabled cases or skips, including all
ten native and three real-font allocation cases. Focused execution passes the three
new cases. The first fixture build missed the explicit AbstractGame header; adding
it corrects compilation without changing production. Raw build/focused/full logs,
XML, driver identity, command/timeouts, runner and patch replay results are archived.
Formatting and whitespace checks pass. Source work ends at the 20-minute checkpoint;
104's documentation-only changes need no repeated build/test execution.

**9.5c.1** closes for forced native timing/presentation workloads and **9.5c.2** closes
for native State backpressure. Parent 9.5c remains open for ordered native Events/Text
under forced backlog. 9.5d/f remain open for further native fault/context-loss
combinations and actual OS restriction/rejection behavior. Font acceptance 9.5e
stays complete. 9.7 publication remains separately pending. No push or PR write.

## Native ordered input acceptance: checkpoints 105–106

The confirmed applied checkpoint remains **98**. Fresh fetch still observes remote
95 (`2c23bf8164ac69e74a6e06bd999ef54a61d7c8f4`); the user's local application report
sets delivery scope. Pending delivery is **99–106**, retaining 99–104. Use
`cheryl-engine-99-106-from-19912dd.patch` after applied 98. Source 105 is
`82b33791b36be7de57180175b0d391bbb1aa5403`; 106 records evidence only.

One Linux/X11 opt-in aggregate case runs four scenarios: sequential/concurrent,
each with fixed-drop or capped variable catch-up. The sender uses XSendEvent and
XSync against only its own native test window; GLFW's ordinary X11 event pump and
character conversion generate the callbacks. No input callback is directly invoked
or replaced and no synthetic PollSnapshot is created. This is synthetic server
input, not physical keyboard, arbitrary layout/IME, or device coverage.

Each scenario checks exactly eleven delivered records in combined sequence and
nondecreasing observation-time order: A press, text `A`, A repeat, another text
`A`, A release; B press, text `b`, B release; and a later C press/text `c`/release.
Native key codes, Shift modifiers, repeat phases and characters match. The first
eight records retain target 7, its epoch and exclusive routing after focus and
capture leases are replaced. The later C tap uses target 42, its new epoch and
pass-through routing. All three keys map to one semantic action: exclusive A/B
contribute no press/release, and C contributes exactly one of each with no held
State or record replay in subsequent recovery updates.

Concurrent mode holds a simulation-mailbox callback before consumption. Three
actual native polls fill capacity, including the unchanged third observation.
The C tap has already reached the server/Xlib queue while two separate completed
platform drains verify the native poll sequence is still three. Releasing the gate
transfers all three polls and their eight old-focus records at the first update;
normal resumed polling later captures C under the replacement focus. Sequential
mode runs the same sender, adapter, focus/capture changes and recovery on its shared
owner without a cross-thread gate. Its initial consumption need not have identical
batch boundaries. In both modes the first update deliberately spends 120 ms to
force dropped lag; capped catch-up also produces its distinct update kind. Future
coordination establishes full-batch ordering independently of that workload delay.
XML reports eleven records for each of the four mode/recovery combinations.

Both full Release builds pass. Complete enabled runs pass **346 normal / 333
sandbox** cases with zero failures, errors, disabled cases or skips, including all
eleven native and three real-font cases. Focused execution passes the X11 case;
the full run also checks the final three-key semantic binding and modifier assertions.
The aggregate target and production dependencies are unchanged. The new case is
compiled on Linux and skips on another GLFW platform. Raw build/focused/full logs,
XML, driver identity, command/timeouts, runner and exact patch replay results are
archived. Formatting and whitespace pass. No additional build/test run is required
for documentation-only 106. No new manual checklist, push or PR write occurred.

**9.5c.3 and parent 9.5c close** for these recorded scopes together with 103's native
timing/presentation/State backpressure and the existing finite-demo and reported
desktop checks. Font acceptance 9.5e stays complete. Remaining task-9 execution is
now concrete:

- **9.5d.2:** compose real resource creation and partial frame/native failure with
  GameRuntime cleanup in both modes. Check actual handle deletion, owner/context
  recovery and original-failure preservation across later cleanup failure and
  accepted CPU/platform dependencies. Existing context switching, reload failure,
  idle retirement and late-owner cases retain their already executed scope.
- **9.5f.1:** execute inherited affinity restriction/discovery with a temporarily
  narrowed calling-thread mask, reported required/preferred eligibility, restoration,
  and a genuinely rejected native OS affinity request. Controlled adapter failures
  already exercise pre-callback suppression/accounting; do not label them actual
  OS rejection. Reconcile this evidence before closing 9.5f and parent 9.5.
- **9.7:** publish PR metadata only when separately requested; the local draft stays
  current. Hardware GPU resets, physical input devices and privileged policy changes
  are outside the bounded automated claims above.

## Optional-backend build guard: checkpoint 107

A final dependency review found direct X11 helper references in the new Linux
fixture even when GLFW's X11 backend was disabled. Checkpoint 107 sets the native
X11 test macro only for Linux normal builds with GLFW_BUILD_X11 enabled, on this
source file only. Both the native-access include and X11 case use that guard.
Other aggregate native cases remain available. Production dependencies are unchanged.

Normal/sandbox Release builds pass after the guard, and complete enabled suites
again pass 346/333 cases without skips. The affected translation unit also compiles
with the X11 test macro absent; nm confirms no glfwGetX11/XSendEvent/XSync helper
references. This is an explicit disabled-macro compile check, not a complete
Wayland-only build. The archive distinguishes the earlier overlapping suite run
from the final run after the rebuilt executable is complete.

Pending delivery is now **99–107** from applied 98, in
`cheryl-engine-99-107-from-19912dd.patch`; next unused number is **108**. Source
behavior and the bounded 9.5c/9.5e closures are unchanged. 9.5d.2/9.5f.1 and 9.7
remain open. This guard was completed during the checkpoint packaging buffer.

## Native runtime/OS acceptance closure: checkpoints 108–110

Validated source is 109, `d07d1a0f98599f46b41aee20cb507a0ef7379f2c`.
108, `293f275846eae0a4183e63c5d6d1e2a531f4fc3f`, adds the native runtime case;
109 adds the two actual OS affinity cases. Both default Release builds complete,
including the normal demo. The focused three-case run and full **349 normal / 335
sandbox** runs pass with zero failures, errors, disabled tests or skips. Twelve
real-context and three real-font cases execute. The expanded native translation
unit also compiles with CHERYL_NATIVE_X11_TESTS absent; nm finds no X11 helper
references. This retains the explicit compile-only scope, without claiming a full
Wayland build. No production behavior correction was required in this period.

The native case records **six** mode/failure combinations: initialization after
resource creation, throw after a frame has retained its packet, and throw after
a real completed swap. Each accepts a CPU job retaining native dependencies and
owned CPU data. The job waits until quiesce, after submissions close, then submits
a real platform image upload. Shutdown pumps that completion and joins the owned
worker before game cleanup; its future returns 42 and accounting settles exactly
one accepted job. Geometry/material/image owners expire after frame/game release.
Input detaches and closed worker/platform endpoints reject subsequent submissions.

Deinit really unbinds the GLFW context and raises a secondary cleanup failure.
Maintenance rejects the absent current binding, while renderer shutdown restores
its owner context and completes program/VAO/buffer/original-texture/upload-texture
deletion. Driver glIs* queries verify deletion while the borrowed window remains
alive, and run rethrows the original failure. This exercises context-binding
recovery; it does not simulate a hardware GPU reset. **9.5d.2 and parent 9.5d close**
with the already executed context/reload/retirement/late-owner scopes.

The affinity restriction case uses the unmodified production native adapter.
It narrows only the calling test thread from CPUs 0–8 to CPU 0. A new child/pool
inherits and discovers that mask; required CPU 1 rejects, preferred {0,1}
intersects to {0}, and preferred {1} falls back to {0}. Actual job mask/CPU
observations match. The restricted child joins before a guarded restoration of
the caller's complete nine-CPU mask, verified by readback and recorded in XML.

The rejection case selects absent CPU 1023 inside the adapter's mask capacity.
A nonempty, in-range request reaches pthread_setaffinity_np and the kernel returns
**Invalid argument**. The accepted callback future reports failed_operation and
its next required job executes on the unchanged inherited mask. Both jobs settle;
policy_failures stays zero because rejection happened inside the callback.
Controlled adapter fixtures separately establish rejection before callback entry,
preferred fallback, accounting and constructor rollback. No external process mask,
cgroup, privileged system policy or physical CPU configuration changes.

**9.5f.1/9.5f and parent 9.5 close** after reconciliation with prior full regression,
finite-demo and reported desktop results, software-driver context/reload/retirement,
forced timing/input/presentation and TrueType/CFF/rotated-FFont evidence. The source
audit remains complete at 90. Sanitizers, physical GPU/device/IME variants and
privileged live policy changes were not claimed by these bounded scopes. Only
separately requested PR metadata publication **9.7** remains open; its local draft
is current. No push, PR write or new manual checklist occurred.

The current validation archive contains raw build/focused/full/XML/driver records,
runner commands and timeouts, native/worker source, compile-only backend guard
proof and patch replay results, plus the prior 107 archive for earlier acceptance.
Pending delivery is **99–110** from user-confirmed applied 98 in
`cheryl-engine-99-110-from-19912dd.patch`; only 108–110 are new this period. Fixed-base
cumulative delivery and individually numbered commits preserve their ordering.
Documentation-only 110 does not require another execution run.

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
