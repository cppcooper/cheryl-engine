# Diagnostics

Configure useful output, inspect subsystem observations, and preserve failure
context through cleanup. The [Diagnostics system](../engine-systems/diagnostics.md)
introduces the available snapshots and their observation boundaries.

```{toctree}
:maxdepth: 1

Configure logging and investigate missing records <../runtime/logging>
Investigate runtime and resource symptoms <../runtime/subsystem-diagnostics>
Handle native and cleanup failures <../runtime/failure-reporting>
```

A log record or counter is evidence about an operation, not a completion barrier.
Use the owning operation's future or lifecycle when another step depends on its
completion.
