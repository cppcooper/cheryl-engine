# Logging acceptance

Logging acceptance covers owned destinations, queue loss, callback reentry,
retained resources and supported teardown.

Configure a separate directory for each row. Use Release as the
build configuration for this matrix so profile changes remain independent of the
Debug sanitizer configuration. The Engine-only selection below avoids unrelated
native/display prerequisites; use the
[architecture procedures](architecture-validation.md) when module composition also changes.

| Row | CHERYL_LOG_PROFILE | CHERYL_LOG_COMPILED_MASK |
| --- | --- | --- |
| Developer | developer | empty |
| Support | support | empty |
| Release | release | empty |
| Minimal | release | 0x07 |
| Disabled | release | 0 |

For the developer row:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/log-developer -DCMAKE_BUILD_TYPE=Release -G Ninja \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
    -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
    -DCHERYL_BUILD_DEMO=OFF \
    -DCHERYL_SANDBOX_BUILD=OFF -DCHERYL_LOG_PROFILE=developer
  cmake --build build/log-developer --parallel \
    --target tests-logging acceptance-logging
  python3 projects/engine/tests/logging-acceptance/logging.py build/log-developer
)
```

The runner accepts multiple build directories and runs them serially. It neither
configures nor builds. Its focused filter selects logging.*; the first-include
logging-policy translation unit is compiled as part of `tests-logging` (output
`tests-engine-logging`); isolated cases use `acceptance-logging` (output
`tests-acceptance-engine-logging`). Severity cases
check selected-profile output, disabled guarded side effects, logger/delegate gates,
and include-order stability. In the disabled profile, facade queue cases explicitly
skip; isolated native queue cases still exercise the backend independently of
compile stripping. Record these distinctions instead of claiming zero skips.

Independent processes exercise standard/non-standard sink log/flush failures,
failed rotation leaving a closed file, saturated recursive native submission,
Block and DiscardNew loss behavior, discarded flush requests, retained clones,
static singleton/pool teardown, and fatal final owner destruction from a callback.
Named-routing acceptance separately checks the five engine files, formatted/stream/
direct named writes, category filtering and closed/reopened behavior after the
application replaces its default logger. Category creation must not replace that
default, and engine records must never appear in the application destinations.
The runner enforces a 30-second timeout, checks surviving record order/content,
checks shared loss output, and verifies the fatal case's deliberate exit code.
Timeouts and nonzero results are failures, not accepted limitations. Native GPU or
real-font fixtures are not prerequisites for this logging matrix.

For concurrency changes, run relevant cases under separate
sanitizer configurations. Debug currently enables ASan/UBSan. A TSan configuration
must disable that combination before enabling TSan; adding it is a separate build
configuration task, not a reason to combine incompatible sanitizers. Source-only
review and this finite matrix cannot prove every interleaving or arbitrary callback
termination. Assess selected profiles, failures/skips and host limits separately
from source completion; an unselected or skipped scenario supplies no acceptance.
