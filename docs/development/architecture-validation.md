# Architecture validation

## Extracted module structure — 4 October 2026

The implemented assembly has neutral Engine, Native GLFW and whole OpenGL owners;
[the module guide](modules.md) records target selection, compatibility, standalone
composition, tests and the restored automatic Debug crash bootstrap. Implementation
commit `cfa286d` contains the coordinated extraction and composition cutover.

Static source checks account for the original 69 production translation units:
52 Engine, six Native GLFW and eleven OpenGL. The existing 408 named test cases are
preserved, with two new small real-runtime contract cases in the default engine
suite. The mixed input file is split; backend/native cases and the PNG fixture now
live with their implementing owners. The owner-committed memory test file is
unchanged. Engine headers/sources contain no GLFW/GLAD/Gainput includes or module
imports, and common dependency discovery contains no native/graphics SDKs.

These are static checks. No configure/compiler probes, builds or tests were run
for this extracted graph. G5 executable acceptance and measured unit build/run
cost remain pending explicit authorization. Required selections are Engine alone,
Engine + Native GLFW, and Engine + Native GLFW + OpenGL, plus standalone module
reuse/bootstrap, owner consumer/header checks and original logging/diagnostics
acceptance. The automatic Debug bootstrap also requires isolated native crash
acceptance; exception traces retain their separate existing checks.

The native backpressure case now waits for a replacement native poll and a subsequent
platform drain before asserting the next input batch. This replaces the previous
host-scheduling assumption while retaining the full-backlog/presentation guarantee.
It has not yet been executed in the extracted assembly.

## Projects layout migration — 4 October 2026

The directory migration retains the combined `cherylGL` / `Cheryl::Engine` library,
existing target names, public include spellings and root build outputs. Production
source contents are unchanged; the native PNG fixture now resolves through its test
owner. Source inventories contain 69 engine translation units normally and 66 in
sandbox, matching the previous selection.

| Selection | Executed result |
| --- | --- |
| Normal Release, developer logging profile, GLFW/X11 | Engine, aggregate tests, logging runners, demo and Backward tool built. Aggregate: 391 passed, 17 opt-in skips, zero failures. |
| Sandbox Release, developer logging profile, GLFW null platform | Engine, aggregate tests, logging runners and Backward tool built. Aggregate: 389 passed, 3 font-fixture skips, zero failures. |
| Normal and sandbox consumers | `cheryl-consumer` and the 12/11 first-include header probes built against their existing engine targets; both consumers ran successfully. |
| Independent consumer bootstrap | Separate normal configuration succeeded, including with the root consumer option enabled in the cache; no duplicate targets. This acceptance reused the existing engines for consumer compilation. |
| Relocated logging and diagnostics scripts | Both scripts passed in normal and sandbox builds; logging included all eight isolated scenarios. |
| Opt-in native OpenGL/X11 | 13 of 14 cases passed together. `polling_backpressure` failed a timing assertion during concurrent compilation and passed its single targeted retry with compilation stopped. The moved PNG fixture passed. |

These runs used CMake 4.4.3 and GCC 16.2.1 on Linux, with
`CMAKE_POLICY_VERSION_MINIMUM=3.5` for pinned dependency compatibility. Normal used
`GLFW_BUILD_X11=ON`, `GLFW_BUILD_WAYLAND=OFF` and the existing desktop display.
Aggregate runs did not enable real-font fixtures; normal native coverage was run
separately with `CHERYL_NATIVE_GL_TESTS=1`. CTest discovery retains the build root as
the working directory. This is migration evidence, not proof of future SDK isolation
or of a clean single-run native suite. Revisit the timing-sensitive case when its
module-owned tests are separated.

After the initial builds, consumer verification used existing build directories,
`CHERYL_BUILD_CONSUMER_TESTS=ON`, and `nice -n 19 cmake --build <build> --parallel 1
--target cheryl-consumer`, with a compilation break between selections. No fresh
engine build or logging-profile matrix was added for this final verification.

## Existing CLion profiles — 4 October 2026

A subsequent `cherylGL` build reported missing `math.h` through GLM and GCC's
`cmath`; the same profile reproduced missing `stdlib.h` through `cstdlib`.
All five existing CLion profiles had cached failed C++ ABI discovery and empty
implicit include directories. Their resulting `-isystem /usr/include` argument
broke the standard headers' `#include_next` lookup. The original failed probes
recorded no diagnostic explaining their failure.

A fresh minimal probe with the same CLion CMake 3.28.6 detected the compiler
correctly. Refreshing only the generated C++ compiler metadata and ABI result
restored discovery in Default, Debug, Release, RelWithDebInfo and MinSizeRel,
preserving profile options and existing objects. The redundant system include
argument disappeared from all five profiles. Both previously failing source
dependency scans passed, and Debug `cherylGL` compiled and linked successfully
with GCC 16.2.1, X11 and Wayland enabled, using one low-priority build job.
The other profiles received configuration checks; test suites were not repeated.
Recovery guidance is in [consuming the engine](consuming-engine.md#standard-headers-in-an-existing-build).

The follow-up also repairs tracking of the relocated 78-byte PNG fixture. The
initial migration had left it locally present but ignored by the general PNG
rule; its exact path is now exempted and tracked, with original bytes preserved.

## Recorded results

The earlier recorded validation on 1–2 October 2026 covers the completed runtime,
rendering, input, worker, and resource architecture. The validated C++ snapshot was
`d07d1a0f98599f46b41aee20cb507a0ef7379f2c`; its changes are integrated into
[PR #9](https://github.com/cppcooper/cheryl-engine/pull/9), merged into `develop`.
These are recorded execution results, not a fresh run of the current checkout.

| Configuration | Build | Aggregate results |
| --- | --- | --- |
| Normal Release | All default targets, including `demo`, compiled and linked. | 349 passed; zero failures, errors, disabled cases, or skips. |
| Sandbox Release | All default targets compiled and linked; the windowed demo is excluded. | 335 passed; zero failures, errors, disabled cases, or skips. |

The recorded toolchain was GCC 13.3.0, C++23, CMake 3.31.10, Python 3.12.14, and
Jinja2 3.1.6 on Linux with the pinned submodules. Normal execution used GLFW/X11
and Mesa 25.2.8 llvmpipe (LLVM 20.1.2), OpenGL core 4.5, under Xvfb. Native and
real-font opt-in cases were enabled for the reported complete runs.

## Executed coverage

- Aggregate regressions exercise dispatcher lifetime/cancellation, event invalidation
  and serial delivery, worker caps/shares/policies, timing/input recovery, cache
  publication, retained packets/material generations, and startup/shutdown failures.
  Recording adapters establish their controlled scenarios independently of a driver.
- Twelve real-context cases exercise resource retirement, selected-context restoration,
  retained-frame shader reload, failed construction/link/reflection cleanup, rotated
  FFont packets, RGBA/alpha row conventions, forced timing/presentation workloads,
  State backpressure, ordered X11 Events/Text, and composed runtime failure cleanup.
  Sources: [native-opengl.cpp](../../projects/modules/opengl/tests/acceptance/src/native-opengl.cpp).
- Native runtime cleanup covers initialization, partial-frame, and presentation failure
  in both modes. Accepted CPU/platform uploads settle before game cleanup; a later
  deinit error preserves the original failure. Driver queries observe native deletion
  while the borrowed window is still alive.
- Three real-font cases sweep all 793 TrueType and 727 CFF scratch allocation requests
  in two concrete ASCII bake traces, plus nested failures. Rejected allocations raise
  `bad_alloc`, release outstanding scratch, and prevent provider upload. Sources:
  [fonts.cpp](../../projects/engine/tests/all-tests/src/resources/fonts.cpp).
- Linux worker cases exercise actual inherited-mask discovery/restoration, required/
  preferred eligibility, native kernel rejection, and recovery. Controlled adapters
  separately cover failure before callback entry and partial thread-start rollback.
  Sources: [worker-pool.cpp](../../projects/engine/tests/all-tests/src/core/worker-pool.cpp).
- Finite real-demo runs complete in both modes with normal font bake/upload/render
  and cleanup. Desktop camera/mouse input, text focus, resize, successful/failed/
  recovered F5 reload, and normal close were reported as passing on 1 October.
  The repeatable sequence is in [native-desktop-checks.md](native-desktop-checks.md).

## Repeating validation

Initialize the pinned dependencies with `git submodule update --init --recursive`.
Install Jinja2 in the Python interpreter selected for GLAD; set `Python_EXECUTABLE`
if a different interpreter supplies it. Normal Linux builds need the X11/OpenGL
and GLFW platform development dependencies. Configure separate build directories:

```sh
cmake -S . -B build-normal -DCMAKE_BUILD_TYPE=Release \
  -DCHERYL_BUILD_ALL_TESTS=ON \
  -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
  -DGLFW_BUILD_X11=ON -DGLFW_BUILD_WAYLAND=OFF
cmake -S . -B build-sandbox -DCMAKE_BUILD_TYPE=Release \
  -DCHERYL_BUILD_ALL_TESTS=ON \
  -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
  -DCHERYL_SANDBOX_BUILD=ON
nice -n 19 cmake --build build-normal --parallel 1
nice -n 19 cmake --build build-sandbox --parallel 1
```

These commands describe the current selected-owner assembly and require explicit
build/test authorization. `PRE_TEST` defers GoogleTest discovery until execution.
`CHERYL_BUILD_ALL_TESTS=ON` selects the aggregate in addition to the focused owner
runners. Build and run one selection at a time because some cases share files;
avoid consecutive full builds when an existing build can supply the needed targets.
With a usable GLFW display, enable native acceptance in the normal aggregate:

```sh
CHERYL_NATIVE_GL_TESTS=1 ./build-normal/all-tests --gtest_output=xml:normal-tests.xml
./build-sandbox/all-tests --gtest_output=xml:sandbox-tests.xml
```

Real-font allocation cases additionally require `CHERYL_STB_ALLOCATION_TTF` and
`CHERYL_STB_ALLOCATION_CFF` to name suitable TrueType and CFF OpenType faces, each
with a glyph wider than 64 pixels at size 128. The recorded fixtures were
DejaVuSans.ttf and NimbusSans-Regular.otf. Export both paths before either full
run; `--gtest_filter=font_bake.real_*` selects only these cases. Missing fixture
paths skip; unusable supplied paths fail.

Native cases skip unless requested. A requested unusable display does not count
as a pass. The ordered X11 case additionally needs a Linux/X11 build and session;
CMake enables its private source guard only when that backend is built. Linux
affinity cases can skip when the necessary CPUs/capabilities are unavailable.
Record the configuration, driver, opt-ins, and skipped cases alongside results;
ordinary or unsupported-host runs need not match the zero-skip counts above.

## Evidence limits

Software-driver acceptance does not establish physical GPU/compositor behavior,
hardware reset recovery, physical-device fidelity, arbitrary OS layouts/IME,
every font/transform, or exhaustive interleavings. Controlled native-policy failures
and actual kernel rejection have distinct coverage. Sanitizer execution, privileged
live policy changes, and a complete Wayland-only build were not recorded; the native
translation unit was separately compiled with its X11 test guard disabled.

These limits are coverage boundaries, not reopened completed tasks. Existing feature
and resource TODOs are listed in [todo.md](../planning/todo.md).
