# Rendering and resources

Resolve game appearance into ordered frame packets, select materials and sampling,
and release resource generations safely. [Rendering](../engine-systems/rendering.md)
introduces the frame, camera and submission interfaces.

For a first visible draw, follow [First Game's sprite walkthrough](../getting-started/first-game.md#put-a-sprite-on-screen).
It loads an included image and sprite definition, uses a default material and
submits a frame. The whole-image task below covers PNGs without sprite metadata.

```{toctree}
:maxdepth: 1
:titlesonly:

Draw a whole image <../getting-started/how-to/draw-sprite>
Position objects with a camera <../getting-started/how-to/use-camera>
Submit frames and configure materials <pipelines-and-materials>
Manage caches, publication and retirement <../resources/resource-residency>
OpenGL backend reference <../modules/opengl>
```

For cross-system lifetime decisions, use
[Ownership and Cleanup](../architecture/ownership-cleanup.md). The native backend
must remain usable while retained resources are drawn, even when their C++ handles
can survive closure.
