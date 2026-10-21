# Assets

## Purpose

Assets discovers source files and manifests, prepares owned CPU data and
publishes reusable images, sprites, tilesets, shaders and materials. Application
classes keep their own position, behavior and animation state while sharing
immutable assets.

## Interface

Types below are in `CE::Assets` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:struct}`Loading::Loader <CE::Assets::Loading::Loader>`, {cpp:struct}`Loading::Prepared <CE::Assets::Loading::Prepared>`<br>Prepare owned CPU input, upload on the provider owner, or synchronously `load_assets()`. [Loading entry point](../assets/asset-loading.md#runtime-entry-point); [publication and retry](../assets/asset-loading.md#publication-and-retry). |
| {cpp:class}`Loading::FileRegistry <CE::Assets::Loading::FileRegistry>`, {cpp:struct}`Manifests::Graphics <CE::Assets::Manifests::Graphics>`<br>Discover indexed files and retain parsed manifest metadata. [Index selection](../assets/asset-manifests.md#index-selection-and-paths); [typed consumers](../assets/asset-loading.md#typed-manifest-data-and-consumers). |
| {cpp:struct}`Sprites::Manager <CE::Assets::Sprites::Manager>`, {cpp:struct}`Tilesets::Manager <CE::Assets::Tilesets::Manager>`<br>Retrieve shared sprite/tile assets; validate handles before use. [Resources and caches](../assets/asset-values-and-playback.md#resources-and-caches); [sprite walkthrough](../getting-started/first-game.md#put-a-sprite-on-screen). |
| [Catalogue sources and exact-key changes](../assets/asset-loading.md#catalogue-changes)<br>Observe complete Published/Replaced/Reset commits, retain generation metadata and read native-domain provenance separately. Upload settlement remains one outcome per executed batch. |
| {cpp:struct}`Graphics::Asset <CE::Assets::Graphics::Asset>`<br>Load a whole image or wrap an uploaded image as a retained quad with a default material. [Draw a whole image](../getting-started/how-to/draw-sprite.md#draw-a-whole-image). |
| {cpp:struct}`Sprites::Definition <CE::Assets::Sprites::Definition>`, {cpp:struct}`Tilesets::Definition <CE::Assets::Tilesets::Definition>`, {cpp:struct}`Grids::Definition <CE::Assets::Grids::Definition>`<br>Describe immutable atlas grids, cells, clips and tile rules. [Grid coordinates](../assets/asset-values-and-playback.md#coordinates-and-grid-geometry); [manifest schema](../assets/asset-manifests.md). |
| {cpp:class}`Sprites::Animation <CE::Assets::Sprites::Animation>`, {cpp:struct}`Tilesets::Animation <CE::Assets::Tilesets::Animation>`<br>Keep independent playback cursors over shared clip metadata. [Sprite playback](../assets/asset-values-and-playback.md#sprite-playback); [tile playback](../assets/asset-values-and-playback.md#tile-playback-and-rules). |

### Selection values and enums

| Type | Choices and details |
| --- | --- |
| {cpp:type}`Grids::CellIndex <CE::Assets::Grids::CellIndex>`, {cpp:struct}`Grids::View <CE::Assets::Grids::View>` | Address cells and resolve authored grid geometry/UVs. [Grid and cell addressing](../assets/asset-manifests.md#grid-and-cell-addressing); [coordinates](../assets/asset-values-and-playback.md#coordinates-and-grid-geometry). |
| {cpp:enum}`Tilesets::Direction <CE::Assets::Tilesets::Direction>` | Eight compass neighbors used by tile selection. [Tile selection](../assets/asset-values-and-playback.md#tile-selection). |
| {cpp:enum}`Tilesets::BitmaskType <CE::Assets::Tilesets::BitmaskType>`, {cpp:enum}`Tilesets::WangType <CE::Assets::Tilesets::WangType>` | Four/eight-neighbor masks or corner/edge Wang rules. [Tile rules](../assets/asset-values-and-playback.md#tile-playback-and-rules); [autotile schema](../assets/asset-manifests.md#autotiles). |
| {cpp:struct}`Sprites::Clip <CE::Assets::Sprites::Clip>`, {cpp:struct}`Sprites::Profile <CE::Assets::Sprites::Profile>` | Select named clip timing and application-selected animation profiles. [Animations](../assets/asset-manifests.md#animations); [animation task](../getting-started/how-to/animate-sprite.md). |

## Usage

Load a root during platform initialization, then retain the resulting asset
handles in application-owned objects. For larger loads, split CPU preparation
from provider-owned upload; the application selects and schedules that split.

```{engine-diagram} assets-focus-loading
```

The [application map](diagram-explorer.md#assets-application) includes the worker
and dispatch handoff; the [engine dependency map](diagram-explorer.md#assets-internal)
shows the caches and retained content behind this small loading path.

### Loading stages and results

| Stage | Result and owner | What becomes visible |
| --- | --- | --- |
| `Loader::prepare()` or `prepare_graphics(indexes)` | An owned {cpp:struct}`Assets::Loading::Prepared <CE::Assets::Loading::Prepared>` containing definitions, decoded pixels and shader bytes; CPU work can run on a worker. | No cache entries or native resources. Focused preparation decodes only images referenced by the selected indexes. |
| `Loader::upload()` | Consumes the prepared value on the active provider/loading owner. | Complete entries publish incrementally; only a successful batch replaces the loader's submitted-definition snapshots. Failure can leave earlier entries published. |
| Manager lookup | A retained sprite, tileset, image or shader/material generation. | The selected handle remains the same generation after later cache changes. Validate missing lookups before building game objects. |

`load_assets(provider)` performs preparation and upload synchronously. Use
`prepare_graphics(indexes)` when only selected installed packages are needed.
Keep a prepared copy before moving it into upload if retry needs the exact same
input. The [loading guide](../assets/asset-loading.md#publication-and-retry)
owns the failure and shader-replacement rules.

### Uses

| Consumer | Dependency | Integration boundary |
| --- | --- | --- |
| `Loading::Loader` preparation | {cpp:class}`Files::FileIndex <CE::Files::FileIndex>` from [Utilities](utilities.md), manifest readers and image decoding | Discovers paths and owns the CPU inputs before graphics work begins. `FileRegistry` publishes discovered paths; it is separate from the discovery index. |
| Loader and asset managers | {cpp:struct}`Rendering::Resources::Provider <CE::Rendering::Resources::Provider>` from [Resources](resources.md) | Upload images/geometry and build compatible materials on the provider owner. Constructing another loader does not create another cache domain. |
| Loader and cache observation | {cpp:class}`Events::EventBus::Publisher <CE::Events::EventBus::Publisher>` from [Events](events.md) | Optional weak routes report one upload settlement per executed batch and separate committed catalogue changes per key. [Observation contracts](../assets/asset-loading.md#catalogue-changes). |

[Workers](workers.md) and [Dispatch](dispatch.md) are application composition
choices for loading. `Loader` itself does not submit CPU jobs or platform requests.

### Used by

| Consumer | Retained input | Responsibility |
| --- | --- | --- |
| Application-owned actors and maps | {cpp:struct}`Assets::Sprites::Asset <CE::Assets::Sprites::Asset>`, {cpp:struct}`Assets::Tilesets::Asset <CE::Assets::Tilesets::Asset>` | Keep position, map cells and animation cursors outside shared definitions. [Asset values and playback](../assets/asset-values-and-playback.md). |
| [Rendering submission](rendering.md#usage) | Sprite/tile assets or {cpp:struct}`Assets::Graphics::Asset <CE::Assets::Graphics::Asset>` | Resolve selected cells and styles into retained draw packets; resolution does not advance animation. |
| Loading tools and resource browsers | Prepared definitions, `manifests()` and catalogue snapshots | Inspect CPU metadata and exact-key changes. A successful metadata snapshot is not an atomic view of all caches. |

### Shared assets, independent playback

Give each moving actor its own {cpp:class}`Assets::Sprites::Animation <CE::Assets::Sprites::Animation>`
while sharing the sprite asset. Advance it during game update, then copy the
selected cell into frame preparation. A clip entry index selects an animation
entry; that entry's cell selects atlas geometry. Tile playback similarly wraps
shared tileset data in an application-owned cursor.

The [playback diagram](diagram-explorer.md#assets-focus-playback) separates
shared content from per-object state. Follow [animate a sprite](../getting-started/how-to/animate-sprite.md)
for the complete update and drawing route. Fonts use the separate [Text](text.md)
selection/layout path.

## Contracts

Preparation changes no global caches or native state. Upload is serialized on
the active provider/loading owner. Successful entries can become visible before
the whole upload finishes; failure does not imply rollback. Retain a prepared
copy before consuming it when retry needs that exact input. Existing keys and
explicit shader replacement have different update rules.

An owned loader binds one immutable root, but another loader does not create a
second resource domain. The shared caches currently support one active
provider/loading-owner domain. Retained definitions and handles remain distinct
from a live backend's ability to use them.

## Lifecycle

Create an owned loader for an asset root, prepare CPU input and upload after
renderer initialization. Validate retrieved handles before constructing game
objects. Release the game's handles during cleanup; published frames may retain
their own copies until recycled. Native resource retirement follows the backend
domain, not loader destruction or the last manager lookup.

## Execution

CPU preparation may run on workers. Cache publication and resource upload obey
provider thread/current-context requirements. Advance animation and select
visible state on simulation; frame preparation copies selected cells and styles
without changing essential game state.

## Integration

On the platform owner during `init_platform()`:

```cpp
CE::Assets::Loading::Loader loader(asset_root_);
loader.load_assets(engine_.resources());
auto sprite = CE::Assets::Sprites::Manager::get().get_asset("first-game", "actor");
```

Check `sprite` and required materials before constructing your player; a lookup
can return an empty handle. The [loading failure contract](../assets/asset-loading.md#publication-and-retry)
explains which earlier entries remain published after an upload error. Keep
player position and animation state in the player, not in shared asset metadata. See
[loading and drawing a sprite](../getting-started/first-game.md#put-a-sprite-on-screen).

## Limits and further reading

There is no engine-owned map/world or prescribed controller/Character class. Tile selection
operates on application-owned map data. Read [loading](../assets/asset-loading.md),
[manifests](../assets/asset-manifests.md),
[asset values and playback](../assets/asset-values-and-playback.md),
[file/font discovery](../assets/file-and-font-discovery.md),
[asset catalog](../assets/catalog.md) and
[residency](../resources/resource-residency.md). The
[Loader header](../../../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h)
defines preparation/publication entry points.
