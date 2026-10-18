(diagnostics)=
# Logging and diagnostics

Configure useful output, inspect subsystem observations, and preserve failure
context through cleanup. The [Diagnostics system](../engine-systems/diagnostics.md)
introduces the available snapshots and their observation boundaries. Start with
logging for missing records or destination/filter settings, subsystem diagnostics
for a stalled or unexpected runtime/resource state, and failure reporting when an
operation throws or cleanup also fails. The output viewer displays process output;
it does not replace the log files or saved failure report.

```{toctree}
:maxdepth: 1
:titlesonly:

Configure logging and investigate missing records <../runtime/logging>
Investigate runtime and resource symptoms <../runtime/subsystem-diagnostics>
Handle native and cleanup failures <../runtime/failure-reporting>
Linux output viewer <../modules/debug-terminal-linux>
```

A log record or counter is evidence about an operation, not a completion barrier.
Use the owning operation's future or lifecycle when another step depends on its
completion.
