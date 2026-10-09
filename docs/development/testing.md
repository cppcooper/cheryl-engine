# Building and selecting tests

Tests belong to the owner whose behavior they exercise. Engine uses controlled
contract implementations; modules test their real integrations; `projects/tests/`
owns cross-module behavior. See [architecture validation](architecture-validation.md)
when changing isolation, composition or lifetime boundaries.

## Runner selection

`CHERYL_BUILD_TESTS=ON` selects focused GoogleTest runners and makes broader runners
explicitly buildable. `CHERYL_BUILD_ALL_TESTS=ON` adds aggregates to the default
build and CTest discovery; `CHERYL_BUILD_ACCEPTANCE_TESTS=ON` does the same for
acceptance runners. Both require tests. Standalone module tests default to `OFF`.

| Owner | Focused build targets | Aggregate build target |
| --- | --- | --- |
| Engine | `tests-engine`, `tests-logging` | `all-engine` |
| Native GLFW | `tests-native-glfw` | `all-native-glfw` |
| OpenGL | `tests-opengl` | `all-opengl` |
| TGUI | `tests-ui-tgui` | `all-ui-tgui` |
| RmlUi | `tests-ui-rmlui` | `all-ui-rmlui` |
| Miniaudio | `tests-audio-miniaudio` | `all-audio-miniaudio` |
| Selected assembly | `tests-ui-coexist` when both UI owners exist | `all-tests` |

Aggregates reuse focused and GoogleTest acceptance case objects. Build targets use
`all-*`, but their executables use `tests-all-*`; `all-tests` produces `tests-all`.
`tests-logging` produces `tests-engine-logging`. Other focused executable names
match their build targets. The [owner guides](../../projects/modules/README.md)
describe their coverage. Linux's separate `tests-native-joystick` uses syscall
wrapping and stays outside aggregates.

CTest prefixes are the concrete build target followed by `.`. Select one prefix
for overlapping cases instead of running both focused and aggregate copies:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCHERYL_BUILD_TESTS=ON \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST
  cmake --build build/release --target tests-engine --parallel
  ctest --test-dir build/release --output-on-failure --no-tests=error \
    -R '^tests-engine\.'
)
```

Reuse a matching configured build when source, compiler and options agree. Narrow
the regex for a focused change. A direct runner can instead select cases with
`--gtest_filter='suite.*'`. CTest discovers aggregates only when their option is
enabled; explicitly built aggregates can always run directly. Batch required build
targets, and keep tests that share mutable files in isolated working directories.

## Acceptance and consumers

| Build target | Executable | Scope |
| --- | --- | --- |
| `acceptance-engine` | `tests-acceptance-engine` | Runtime, concurrency, failure and resource GoogleTests. |
| `acceptance-opengl` | `tests-acceptance-opengl` | Native graphics/runtime GoogleTests; requires native input selection. |
| `acceptance-logging` | `tests-acceptance-engine-logging` | Fault/timeout/static-teardown driver via [logging acceptance](logging-acceptance.md). |
| `acceptance-signal` | `tests-acceptance-engine-signal` | POSIX crash-bootstrap driver in Debug/Release. |
| `backward-cpp` | `tests-dependency-backward-cpp` | Standalone dependency trace check. |

Manual logging and signal drivers stay outside GoogleTest aggregates and CTest;
[architecture validation](architecture-validation.md#manual-acceptance-drivers)
owns their invocations.

`CHERYL_BUILD_CONSUMER_TESTS=ON` selects consumers independently of GoogleTest.
Each consumer also builds its object-only first-include header probes. The Engine
targets are `consumer-cengine` and `consumer-headers-cengine`; modules list their
consumer targets in their owner guides. Configure each `tests/consumer/` entry
point independently to establish standalone composition; a root consumer alone
cannot prove that boundary. See [independent consumers](consuming-engine.md#independent-consumers-and-header-probes).

## Native and fixture coverage

- Native OpenGL cases require `CHERYL_NATIVE_GL_TESTS=1` and a usable display.
  The ordered X11 case also requires a Linux/X11 configuration/session.
- Real-font allocation cases require both `CHERYL_STB_ALLOCATION_TTF` and
  `CHERYL_STB_ALLOCATION_CFF` with suitable faces; [fixture requirements](architecture-validation.md#native-and-fixture-checks) define them.
- RmlUi checks require its SDK sample font or an explicit `CHERYL_RMLUI_TEST_FONT`.
- CPU text, tile and offline audio checks do not establish appearance or device
  output. Physical observations have separate procedures in the
  [desktop guide](native-desktop-checks.md) and [audio guide](../../projects/modules/audio/miniaudio/README.md#acceptance-boundaries).

An empty selection, skipped case or missing device leaves that coverage pending.
Report the revision, configuration, platform and unavailable observations. The
[testing queue](../testing-requests.md) tracks pending checks and QA; owning plans track
implementation progress. Durable limitations belong in the relevant subject guide.
