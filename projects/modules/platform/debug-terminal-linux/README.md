# Linux Debug terminal

This optional module opens a separate native terminal for process stdout/stderr,
including C stdio, C++ streams, native writes, logging and emergency reports. It
works independently of a rendering context. Engine has no dependency on this
module; root application/test composition chooses it.

## Selection and attachment

`module_debug_terminal_linux` is an interface target, publicly named
`Cheryl::DebugTerminal::Linux`. The root also supplies `Cheryl::DebugTerminal` for
the selected implementation. Root selection is `CHERYL_BUILD_DEBUG_TERMINAL_LINUX`,
defaulting to `ON` for Linux with GNU/Clang and `OFF` elsewhere. Other platforms
have no implementation. A consuming application selects the module by linking
its public target; the root demo and shared test-main composition do so automatically.
Build the consuming executable rather than naming the interface as a build target;
included consumers bring in the implementation and viewer dependencies.

`Cheryl::DebugTerminal::Startup` is the small optional Engine support library
(`cengine_debug_terminal_startup`). Its `CE::DebugTerminal::run(argc, argv,
application, start)` accepts an application entry point and a module-owned startup
callback. Without a callback it invokes the application with the original arguments
and performs no terminal work. The support owns argument policy only; it does not
discover modules, own native sessions or require Engine to call a platform backend.

The Linux implementation uses the GNU-compatible linker `--wrap=main` attachment,
supplying its callback before application startup. It owns session shutdown through
`atexit`. Hosts with another entry-point wrapper must resolve their composition
explicitly; installed-package deployment and another platform module are unimplemented.

## Build and runtime policy

| `CHERYL_DEBUG_TERMINAL` | Linked configurations | Display policy |
| --- | --- | --- |
| `AUTO` (default) | Actual Debug only | Automatic in Debug; `--no-debug-terminal` disables it. |
| `ON` | Every configuration | Automatic in Debug; other configurations require `--debug-terminal`. |
| `OFF` | None | No attachment or argument consumption. |

These rules are independent of logging profile and severity. Omitted configurations
do not link the native implementation or viewer dependency. Linking plain Engine
does not select this module. `CHERYL_DEBUG_TERMINAL_AVAILABLE` describes whether the
interface selects its implementation in the current configuration.

When attached, `--debug-terminal` and `--no-debug-terminal` are consumed before the
application parser; the last occurrence wins. Arguments after `--` pass through.
Help, GoogleTest listing and re-executed death-test discovery suppress display.
Headless execution without `DISPLAY` or `WAYLAND_DISPLAY` uses inherited output.
When the implementation is omitted, these options remain application arguments;
they cannot enable a facility that was not linked.

`CHERYL_DEBUG_TERMINAL_EMULATOR` accepts `auto`, `konsole` or `xterm`. Automatic
selection tries Konsole, then xterm when the launcher is unavailable. The viewer
is `cheryl-debug-terminal-viewer`, built as a dependency of included consumers;
launch failure, invalid handshake or unavailable support reports to inherited
stderr and preserves the application's inherited output path.

## Output and lifetime

An owned temporary directory contains an append-only capture file and a Unix
control socket. stdout/stderr share the file, so their stream identity is not
retained. A separately launched viewer drains it live; newline-free buffered
output still follows producer flushing. The viewer inherits no runner output
descriptors, so a retained crash viewer cannot hold CTest capture channels open.
File logging and severity filters remain independent.

Normal exit flushes captured output, signals the viewer to drain/close and removes
the temporary capture. Loss of the control connection without normal shutdown
retains available output until manual viewer closure. This detects abnormal process
loss, not the semantic difference between a signal, `_Exit` or a specific crash.
Normal nonzero return and handled startup errors still use normal shutdown.

Manual viewer closure leaves the application running. Subsequent output continues
in the capture file until shutdown, with a notice and its path on inherited stderr;
file logs continue. There is no automatic reopening or mirroring to the launch
terminal. File-backed capture avoids dependence on a live pipe reader, but is not
a disk quota or infallible output-delivery guarantee.

## Test reporting

Repository-owned GoogleTest uses generated build-tree routing copies. Framework
reports, assertion progress, help/listing and diagnostics use separately saved
inherited output channels; tested engine/module writes use terminal capture.
The dependency source checkout is unchanged. Internal stdout/stderr capture and
death-test output retain their framework semantics, and XML remains independent.

A supplied GoogleTest target without the routing capability causes terminal startup
to fall back to inherited output. Discovery creates no viewer. Each included test
process owns its own viewer; parallel processes do not share a window or producer
identity. Interactive Debug test launches therefore create separate viewers unless
disabled. Release `AUTO` runs keep their existing reporting/output paths.

## Checks and acceptance limits

The explicitly buildable targets `tests-acceptance-engine-terminal` and
`tests-acceptance-engine-terminal-reports` supply raw-output/lifetime and framework
probes. They are not ordinary standalone test runs: some scenarios deliberately
fail or terminate. The [terminal.py driver](tests/acceptance/terminal.py) selects
them, controls fake emulator launchers and checks output, rollback/reaping, crash
retention, manual closure, discovery, captures, death tests and XML.

The [testing queue](../../../../docs/testing-requests.md) contains the pending
configuration matrix and real-desktop QA. Source presence and controlled launcher
checks do not establish actual Konsole/xterm window behavior, desktop/IDE launch,
or another operating system. Native implementation and executable acceptance remain
separate.
