# Memory, state and utilities

Allocate application objects, choose a state representation, and use the engine's
CPU helpers. The [Memory](../engine-systems/memory.md) and
[State](../engine-systems/state.md) system pages provide the type-selection tables.
Use the allocation guide when objects must outlive a batch or return reserved
slots; use the utility guide when choosing state tracking, timing, packed values
or geometric calculations without an allocation-lifetime problem.

```{toctree}
:maxdepth: 1
:titlesonly:

Use pools, reservations and release contexts <../resources/resource-lifetime>
Track state and use time, number and geometry helpers <../runtime/utility-contracts>
```

For relationships between pooled handles, borrowed access and native resources,
use [Ownership and Cleanup](../architecture/ownership-cleanup.md).
