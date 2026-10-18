# Native callbacks and failure reporting

A native callback records failure for an ordinary C++ boundary to rethrow.
`GameRuntime::run()` performs its cleanup sequence before propagating the first
failure to the application's caller. Catch there to choose the process result
or application recovery policy; ordinary log output is not the error channel.

```text
Native callback catches and retains its first exception
    -> C++ pump/check boundary consumes and rethrows it
    -> Runtime stops and cleans up, preserving that first failure
    -> run() rethrows to the application
Later cleanup failures -> bounded emergency stderr reports
```

For setup and the application entry point, see
[First Game](../getting-started/first-game.md). The
[runtime shutdown sequence](runtime-frame-boundary.md#shutdown) owns dependency
ordering; [logging diagnosis](logging.md#investigate-missing-or-delayed-records)
explains ordinary records that may accompany the exception. The sections below
define callback, emergency-output and destruction limits.

## Native callbacks

Native GLFW callbacks never unwind through the C call stack.
`CE::Platform::GLFW::InputSystem` and `CE::Platform::GLFW::Window` retain their first
callback exception on the platform owner thread.
Window callbacks still update dimensions while a failure is pending, but suppress
further resize listener work until that failure is consumed. Input update checks
window failures before publishing a successful sample, then checks its own input
failure. A pending input failure left behind by a window failure is reported during
detachment. Host pumps follow begin_poll, native event pumping, and update.

`CE::Platform::Window::check_native_failure()` consumes/rethrows at an ordinary C++ boundary.
Adapters without throwing native callbacks inherit its no-op default. `CE::Runtime::GameRuntime`
checks after each input pump, including a pump that requests stop, and during
cleanup. Explicit GLFW resize/mode calls check before and after native operations;
should_close also checks. An unconsumed failure reaching window destruction is
reported without throwing. Size getters remain usable for current state.

The GLAD procedure-address callback also captures its first exception and returns
null to the C loader. Renderer startup rethrows it after the loader returns,
preserving that cause ahead of the unsupported-version diagnostic. A subsequent
context-release failure is reported while the original startup failure propagates.

## Shutdown failures

Runtime shutdown preserves the first exception for its caller. Each distinct later
exception is reported with the phase that raised it: producer/dispatcher closure,
worker completion, pending native callback, frame recycling, game cleanup, input
shutdown, or renderer cleanup. Reports run outside scheduler and collection locks.
Shutdown continues its existing order; reporting does not change ownership or
replace the first failure. Repeated failures in a shutdown pump can produce repeated
records; ordinary category/rate configuration follows the [logging contract](logging.md).

## Emergency reporting

`CE::Diagnostics::report_failure` is the emergency path for cleanup, logger destruction,
and fatal internal release failures. It writes a bounded 1024-byte record using C
stdio without C++ allocation, formatting, symbolization, user callbacks, or logger
initialization. Generated Cheryl exceptions provide their bounded cause/location
summary instead of spending that record on a trace prefix. Output errors and
truncation are best effort and never raise another exception. This is independent
of ordinary logger severity/destination configuration and cannot promise delivery
under process termination or broken output. It may wait on stdio serialization.
With an attached [Linux output terminal](../modules/debug-terminal-linux.md),
stderr writes enter its process capture even when the game fails before rendering.
That module's independent viewer can retain available output after abnormal process
loss; failure reporting itself does not own or launch the viewer.

## Traces and exception storage

Trace sinks reserve termination, mark truncation, copy only the written prefix,
and reset before reuse. Symbol resolution can allocate. `CE::Diagnostics::stack_trace()` returns
empty when resolution/copying fails. Cheryl exception construction prepares an
owned 512-byte cause/location fallback before attempting trace/format allocation.
Copies preserve it if full message allocation fails. Caller argument expressions
and the language runtime's thrown-object allocation are outside that guarantee.
Exception storage and window virtual interfaces are public ABI boundaries; rebuild
the engine and its consumers together when those definitions change.

## Destruction failures

Trusted shared-pointer release methods retain the existing terminate-on-invariant-
or-allocation-failure policy, reporting the caught cause first. Failure to settle
owned workers during `CE::Runtime::Context` destruction likewise reports before termination;
continuing into adapter destruction would leave live work with invalid dependencies.
Ordinary public operations can still throw. A user destructor that itself violates
noexcept can terminate before a surrounding reporting guard can run.

## Validation

Regression sources cover resize listener capture/consumption, stopping during a
failed pump, procedure lookup plus release failure, primary-plus-cleanup failure,
bounded trace reuse, owned fallback storage, and bounded emergency records. These
are finite failure scenarios; isolated allocation-failure and native/sanitizer
acceptance retain separate coverage. Use [architecture validation](../development/architecture-validation.md)
for reusable procedures and platform limits.
