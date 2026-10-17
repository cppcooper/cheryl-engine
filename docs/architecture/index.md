# Architecture

Connect engine systems while preserving their ownership and execution rules.
Start with the interfaces available to implement, assemble compatible components,
decide who keeps them alive, then coordinate their work.

| Need | Start here |
| --- | --- |
| Implement or replace an integration | [Interfaces and Adapters](interfaces-adapters.md) |
| Select public entry points and construct a compatible context | [Assembling the Engine](assembling-engine.md) |
| Keep borrowed targets and retained resources alive through cleanup | [Ownership and Cleanup](ownership-cleanup.md) |
| Choose an execution owner and transfer work or results | [Threads and Data Flow](threads-data-flow.md) |

[Engine Systems](../engine-systems/index.md) introduces each system and its public
types. [Detailed Guides](../guides/index.md) supplies the operating procedures and
exact contracts for a particular task.
