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
| {cpp:struct}`Sprites::Manager <CE::Assets::Sprites::Manager>`, {cpp:struct}`Tilesets::Manager <CE::Assets::Tilesets::Manager>`<br>Retrieve shared sprite/tile assets; validate handles before use. [Resources and caches](../assets/asset-values-and-playback.md#resources-and-caches); [sprite task](../getting-started/how-to/draw-sprite.md). |
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

### Uses

File discovery and manifests supply CPU inputs; Resources uploads and publishes
them. Applications can place CPU preparation on Workers and transfer owned
results through Dispatch.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Files::FileIndex`<br>{cpp:struct}`CE::Rendering::Resources::Provider`<br>{cpp:struct}`CE::Rendering::Resources::Image`<br>{cpp:struct}`CE::Rendering::Geometry::Buffer2D`<br>{cpp:class}`CE::Rendering::Resources::Material`<br>{cpp:struct}`CE::Memory::Objects::Pool` | [Resources](resources.md)<br>[Memory](memory.md)<br>[Utilities](utilities.md)<br>[State](state.md)<br>[Diagnostics](diagnostics.md)<br>[Workers](workers.md)<br>[Dispatch](dispatch.md) |

### Used by

Application-owned objects retain visible assets; submission resolves them into
draw packets. Tools can inspect prepared manifests without graphics. Text keeps
its own font-selection/layout path.

| Types | Systems |
| --- | --- |
| [`Tutorial::Player`](../getting-started/create-a/player.md)<br>[`Tutorial::Enemy`](../getting-started/create-a/enemy.md)<br>[`FirstGame::Game`](../getting-started/first-game.md)<br>{cpp:struct}`CE::Rendering::DrawPacket2D` | [Rendering](rendering.md)<br>[Text](text.md) |

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

On the platform owner during `init()`:

```cpp
CE::Assets::Loading::Loader loader(asset_root_);
loader.load_assets(engine_.resources());
auto sprite = CE::Assets::Sprites::Manager::get().get_asset("first-game", "actor");
```

Check `sprite` and required materials before constructing your player. Keep its
position/animation cursor in the player, not in shared asset metadata. See
[loading and drawing a sprite](../getting-started/how-to/draw-sprite.md).

## Limits and further reading

There is no engine-owned map/world or built-in Player/Enemy class. Tile selection
operates on application-owned map data. Read [loading](../assets/asset-loading.md),
[manifests](../assets/asset-manifests.md),
[asset values and playback](../assets/asset-values-and-playback.md),
[file/font discovery](../assets/file-and-font-discovery.md),
[asset catalog](../assets/catalog.md) and
[residency](../resources/resource-residency.md). The
[Loader header](../../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h)
defines preparation/publication entry points.
