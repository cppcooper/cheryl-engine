# Diagram explorer

Choose a subsystem, then compare its engine dependencies, application use and
focused views. Arrows point from the consumer toward what it uses. Select a type
or arrow for its source evidence and connections; public types also link to the
C++ API reference.

Application views list the most relevant methods beneath their owning types.
These are selected entry points, not complete method inventories or a call
sequence. **Implement hooks** marks methods your game overrides for the engine
to call. Select a box for each method's purpose, source and available API link;
search also finds method names.

Use **Docs navigation** to keep the documentation sidebar, or **Wide explorer**
for a subsystem sidebar and more room to inspect a diagram. The selected view
is preserved when changing layouts or returning from a guide.

Names retain their namespaces without the leading `CE::`. Primitive and `std::`
types are omitted. Rounded cream boxes describe illustrative application roles;
they are not Engine types. These are curated relationships rather than complete
declaration inventories. Detailed guides own the execution, lifetime and failure
contracts.

The interactive explorer needs JavaScript and HTTP, as do the manual's other
Mermaid diagrams. The view index below remains available without JavaScript.

```{engine-explorer}
```
