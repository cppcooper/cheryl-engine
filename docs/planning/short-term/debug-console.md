# External Debug terminal

## Target and status

Debug graphical applications need a native terminal/console window that streams
process stdout and stderr. Include ordinary application output, Cheryl console records,
third-party writes and emergency diagnostics. Provide output during startup,
execution and shutdown rather than depending on the game rendering successfully.

The native terminal exists only in Debug builds. Release retains the existing
stdout/stderr destinations and configured file logging. Logging profiles do not
control window eligibility: selecting developer logging in a Release build must
not create the native terminal. Treat this as an engine facility available to
consumer applications, with the demo as an initial integration consumer.

This task is selected in the [nearest planned work](../develop-review-and-development-plan.md#nearest-planned-work).
Platform, transport and process/output ownership remain open design decisions;
implementation has not started. Native acceptance follows the selected platform scope; existing
[multi-platform testing deferrals](../long-term/platform-acceptance.md) remain in place.

The engine's built-in graphical developer console is future work in the
[long-term plan](../long-term/README.md#other-engine-extensions), which owns its
in-app rendering, input and command scope, including Release use.

Preserve the terminal connection used to launch tests: CTest and GoogleTest progress
and results stay on that existing runner output destination. Create a separate
native terminal for engine/game/module logs and stdout/stderr, including when the
application already has a launching terminal. The requested visible behavior is a
separate engine log terminal, inspired by UE4. This is a planned contract, not an
implemented guarantee.

After normal application shutdown, close the owned terminal once final output has
drained. After a crash, retain the terminal and its available output for inspection
until the user closes it. Choose a viewer lifetime that can survive the engine
process crashing; a shutdown callback in that process cannot provide this guarantee.

Closing the terminal while the application is running closes the viewer and leaves
the application running. Preserve configured file logging and handle viewer
disconnects so subsequent output cannot terminate the application or block its
writers indefinitely. Output handling after manual closure belongs to the transport
design below.

## Current foundation

The [console sink](../../../projects/engine/include/cheryl/core/logging/osink.h) writes
to `std::cout` and `std::cerr`. Required
[failure reporting](../../runtime/failure-reporting.md) also writes directly to stderr.
The [demo entry point](../../../projects/apps/demo/src/main.cpp) has no explicit
console-window setup. A logger-only viewer would therefore leave other process
output outside the required coverage.

The [shared test entry point](../../../projects/engine/tests/support/gtest-main.cpp)
runs GoogleTest and tested engine/module code in the same process. Their ordinary
stdout/stderr writes therefore share destinations and carry no producer identity.

## Test output separation

Keep CTest and GoogleTest connected to the inherited runner output channel. CTest
runs separately from the test executable. Preserve its capture and normal
[output-display options](https://cmake.org/cmake/help/latest/manual/ctest.1.html).

First prove engine console logging through a destination owned by the native
terminal, while the existing test-report streams retain their connection. Then
resolve direct C stdio, C++ stream and native writes from tested code. Raw bytes on
the shared stdout/stderr channels do not identify their producer, so complete
separation needs explicit routing at the producer or test-report boundary.

If capturing raw writes requires redirecting the test process's standard streams,
preserve the original reporting channel separately. A GoogleTest result printer
using that saved channel is one candidate; GoogleTest supports
[replacing its console printer through event listeners](https://google.github.io/googletest/advanced.html#extending-googletest-by-handling-test-events).
That candidate covers normal progress and assertion reports. Help/listing and
framework messages outside listener callbacks need separate handling. Choose the
smallest integration that preserves the requested connections and output coverage.

Resolve discovery, output-capture assertions and death-test child behavior before
generalizing this path. Preserve machine-readable results and test exit status.
Decide whether parallel CTest processes share one viewer and how records identify
their source test. Apply the Debug-only eligibility rule. Discovery and
unattended/headless runs need a defined path that does not
depend on creating a desktop terminal.

## Investigation and design checkpoint

Investigate the first platform's host terminal dependencies and output channels.
At the design checkpoint, select platform, viewer transport and ownership using the
[module boundary criteria](../../development/modules.md#module-boundary-criteria).
Keep process-wide stream ownership separate from individual engine contexts.
Resolve output after manual closure, reopening and unavailable-viewer behavior.
Distinguish normal failed tests/handled startup errors from crashes before selecting
exit-status or disconnect-based detection.

Settle flushing, encoding, pre-existing output destinations and mirroring to IDE
capture, files and pipes. Choose shared/separate test viewers, producer/report routing,
interactive activation and diagnostic collection without a desktop viewer. Resolve
the discovery/capture/death-test boundaries above before dependent implementation.

## Implementation order after design approval

1. Implement the owned engine logging destination and the smallest startup path
   for both stdout and stderr. Keep the original test-report connection available.
2. Implement agreed direct-write routing for C stdio, C++ streams and native writes,
   including test-report separation. Complete this boundary before extending the
   integration; result-printer routing alone does not cover framework messages.
3. Complete ownership and failure behavior: partial initialization, concurrent
   writers, window closure, startup failure and final diagnostic draining. Close
   the viewer after normal shutdown and retain it after a crash, independently of
   cleanup in the crashed process. A retained viewer must not hold test-report
   capture channels open or delay CTest completion. Keep
   emergency reporting independent of the ordinary logging backend and game loop.
4. Derive activation from the actual Debug configuration independently of severity;
   define Debug controls for automated/interactive consumers. Integrate activation
   and the existing non-Debug output path into the engine and its demo/test consumers.
   Establish startup before their first engine diagnostics and retain the output
   destination until producers and logging workers finish. Preserve configured file
   logging and severity filters.

## Validation and documentation phases

Follow the [work-mode progression](../develop-review-and-development-plan.md#work-mode-progression).
Design coverage from the acceptance scope below before implementation; add automated
cases and observation harnesses in the separate test-implementation phase. Build/test
execution requires explicit authorization. Batch related targets and distinguish
source completion from executable/native acceptance, including prototype validation.
Reconcile resulting contracts and [testing requests](../../testing-requests.md) in
separately authorized documentation maintenance once runnable observation paths exist.

## Acceptance

On the selected native platform, verify the agreed launch behavior from a desktop,
terminal and IDE, plus explicit redirection of each stream. Show both streams live
from application, logging, C stdio and worker-thread output. Verify early failures,
normal shutdown, failure termination and console closure according to the selected
lifetime policy: normal shutdown drains final output and closes the terminal; a
crash leaves the terminal open for manual inspection and closure. Verify that CTest
can finish reporting a crashed test while that engine terminal remains open.
Confirm that an unavailable viewer or closed output consumer does
not produce an unnoticed loss of the required diagnostics or a shutdown hang.
Verify Debug defaults, overrides and behavior in other build configurations.
Closing the terminal during execution leaves the application and its workers
running, with file logging continuing and no hang or termination from later writes.
Release and other non-Debug configurations create no native terminal and use their
existing output destinations, including when developer logging is selected.
For the selected test-viewing mode, engine/game/module output appears in the native
console and GoogleTest reports remain on the original runner channel. Discovery,
capture assertions, death tests, parallel runs and unattended execution retain their
defined behavior. Verify framework messages outside the normal result printer
separately; a passing result-printer prototype does not establish complete separation.
