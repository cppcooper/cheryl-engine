# Assets

Load application content, author graphics manifests, and keep animation or tile
selection in game-owned state. The [Assets system](../engine-systems/assets.md)
introduces the public types; the sprite and animation tasks cover first use.

```{toctree}
:maxdepth: 1

Load and publish assets <asset-loading>
Author graphics manifests <asset-manifests>
Animate sprites and select tiles <asset-values-and-playback>
Discover files and font candidates <file-and-font-discovery>
Install optional artwork packages <catalog>
```

For uploads during play, use the ownership transfers in
[Threads and Data Flow](../architecture/threads-data-flow.md#prepare-and-publish-a-replacement).
Font selection and shaped labels have a separate [Text](../text/index.md) path.
