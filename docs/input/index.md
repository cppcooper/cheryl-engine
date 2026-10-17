# Input

Bind physical controls to game actions, consume ordered records, and route
keyboard/text focus. [Bind input actions](../getting-started/how-to/bind-input.md)
provides the initialization, update and cleanup sequence;
[Input](../engine-systems/input.md) lists the controls and public types.

```{toctree}
:maxdepth: 1

Actions, records, capture and focus <../runtime/input-state-model>
```

Use the [GUI guide](../gui/index.md) when a toolkit session should interpret the
records. Capture requests and focus leases have separate lifetimes and neither
retains a native window.
