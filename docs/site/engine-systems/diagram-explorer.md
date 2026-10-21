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

Signatures use `method(arguments): ResultType`, with qualified Engine types and
public aliases for arguments and results. They are abridged: a missing result
suffix can mean a scalar or standard-library result and does not imply `void`.
`[]` marks a collection. Short notes such as `[shared]`, `[unique]`, `[future]`
and `[borrowed]` retain ownership, asynchronous-result and borrowing information
when their library wrappers are omitted. The API reference has the full types.
When a view names a local namespace, its prefix is omitted in the diagram:
within Input, `Actions::Context` means `Input::Actions::Context`. Other
namespaces remain explicit, and the inspector retains the full names.

Application arrows include the argument and result types used by selected
methods. In the inspector, **Accepts** and **Returns** connect a method to those
types in the current view. Select an arrow to see the supporting method
relationship. Both use the same consumer-to-dependency direction; these arrows
do not describe data flow. The API reference supplies the complete signature,
including types omitted from the selected view.

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
