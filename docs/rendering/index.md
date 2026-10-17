# Rendering and Resources

Resolve game appearance into ordered frame packets, select materials and sampling,
and release resource generations safely. [Rendering](../engine-systems/rendering.md)
introduces the frame, camera and submission interfaces.

```{toctree}
:maxdepth: 1

Submit frames and configure materials <pipelines-and-materials>
Manage caches, publication and retirement <../resources/resource-residency>
```

For cross-system lifetime decisions, use
[Ownership and Cleanup](../architecture/ownership-cleanup.md). The native backend
must remain usable while retained resources are drawn, even when their C++ handles
can survive closure.
