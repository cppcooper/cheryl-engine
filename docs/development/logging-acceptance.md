# Logging acceptance

U5's execution gate covers owned destinations, queue loss, callback reentry,
retained resources, and supported teardown. The authorized normal/sandbox matrix
and supplementary ASan/UBSan checks passed; the
[development plan](../planning/develop-review-and-development-plan.md#u5-executable-gate-and-u6-implementation-boundaries)
records the execution evidence and limits. The commands below reproduce that
gate and require explicit build/test authorization under AGENTS.md.

Configure separate normal/sandbox directories for each row. Use Release as the
build configuration for this matrix so profile changes remain independent of the
Debug sanitizer configuration. Normal Linux uses GLFW/X11 with Wayland disabled.

| Row | CHERYL_LOG_PROFILE | CHERYL_LOG_COMPILED_MASK |
| --- | --- | --- |
| Developer | developer | empty |
| Support | support | empty |
| Release | release | empty |
| Minimal | release | 0x07 |
| Disabled | release | 0 |

For example, after authorization:

```sh
cmake -S . -B build-log-sandbox-developer -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
  -DCHERYL_SANDBOX_BUILD=ON -DCHERYL_LOG_PROFILE=developer
cmake --build build-log-sandbox-developer --parallel 3 \
  --target cheryl-logging-tests cheryl-logging-acceptance
python3 tests/acceptance/logging.py build-log-sandbox-developer
```

The runner accepts multiple build directories and runs them serially. It neither
configures nor builds. Its focused filter selects logging.*; the first-include
logging-policy translation unit is compiled as part of cheryl-logging-tests. Severity cases
check selected-profile output, disabled guarded side effects, logger/delegate gates,
and include-order stability. In the disabled profile, facade queue cases explicitly
skip; isolated native queue cases still exercise the backend independently of
compile stripping. Record these distinctions instead of claiming zero skips.

Independent processes exercise standard/non-standard sink log/flush failures,
failed rotation leaving a closed file, saturated recursive native submission,
Block and DiscardNew loss behavior, discarded flush requests, retained clones,
static singleton/pool teardown, and fatal final owner destruction from a callback.
The runner enforces a 30-second timeout, checks surviving record order/content,
checks shared loss output, and verifies the fatal case's deliberate exit code.
Timeouts and nonzero results are failures, not accepted limitations. Native GPU or
real-font fixtures are not prerequisites for this logging matrix.

After the matrix, run relevant concurrency cases under the separately authorized
sanitizer configurations. Debug currently enables ASan/UBSan. A TSan configuration
must disable that combination before enabling TSan; adding it is a separate build
configuration task, not a reason to combine incompatible sanitizers. Source-only
review and this finite matrix cannot prove every interleaving or arbitrary callback
termination. Record the toolchain, commit, profiles, failures/skips, and execution
limits before releasing the U6 integration gate.
