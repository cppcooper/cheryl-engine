(assets)=
# Assets and animation

Load application content, author graphics manifests, and keep animation or tile
selection in game-owned state. The [Assets system](../engine-systems/assets.md)
introduces the public types. [First Game's sprite walkthrough](../getting-started/first-game.md#put-a-sprite-on-screen)
covers loading, placement and submission; [Play sprite animation](../getting-started/how-to/animate-sprite.md)
adds a per-actor playback cursor.
For a background PNG without a sprite definition, use the
[whole-image path](../getting-started/how-to/draw-sprite.md#draw-a-whole-image).

```{toctree}
:maxdepth: 1
:titlesonly:

Load and publish assets <asset-loading>
Author graphics manifests <asset-manifests>
Play sprite animation <../getting-started/how-to/animate-sprite>
Animate sprites and select tiles <asset-values-and-playback>
Discover files and font candidates <file-and-font-discovery>
Install optional artwork packages <catalog>
```

For uploads during play, use the ownership transfers in
[Threads and Data Flow](../architecture/threads-data-flow.md#prepare-and-publish-a-replacement).
Font selection and shaped labels have a separate [Text](../text/index.md) path.
