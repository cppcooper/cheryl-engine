# Asset loading

`ManifestLoader` parses graphics manifests 2.0 into CPU-only sprite/tileset and
shader/material definitions. An owned `Loader` keeps one immutable root:
preparation selects indexed documents, decodes owned RGBA pixels and snapshots
shader source bytes without a provider; `upload()` constructs resources and
publishes retained metadata on the provider's loading thread.

## Runtime entry point

After initializing the selected backend, load a root with its provider:

```cpp
CE::Assets::Loader loader("assets");
loader.load_assets(engine.resources());
auto actor = CE::Assets::SpriteMgr::get().get_asset("miniworld:soldier_swordsman_cyan");
auto documents = loader.manifests(); // shared_ptr<const vector<AssetManifest>>
```

Each preparation scans again. Only exact `graphics-manifests.json` indexes select
documents; schemas and unlisted JSON are not parsed. Index, texture and shader
references resolve from the enclosing `graphics/` directory, and asset keys use
`namespace:name`. `prepare()` selects every discovered index and decodes referenced
images plus standalone PNGs below the root, deduplicated in sorted order.
`prepare_graphics(indexes)` selects explicit indexes relative to the Loader root
(or supplied absolute paths), decoding only their referenced images. A shader-only
selection decodes no images. Cross-document
IDs and every grid's bounds are checked against those exact decoded dimensions
before any upload. Upload consumes the owned pixels without reopening image files.
Pixels are four-channel RGBA in the decoder's default top-to-bottom row order.
OpenGL reverses those rows in transient upload storage to match the atlas
geometry's upward-positive UV coordinates; the supplied pixels remain unchanged.
The separate stb alpha-atlas path preserves its baked row/UV convention.

Shader sources resolve through exact registered paths, including when basenames
repeat. Preparation owns the bytes, so upload does not reopen shader files either.
Program/material kinds, references and asset selections are checked within the
selected batch; an unrelated cached material cannot satisfy a missing selection.
`register_files()` performs discovery/publication of unmanaged paths only, without
opening JSON, decoding images or requiring a provider.

Preparation can use a tracked context WorkerGroup. Own both the Loader and
PreparedAssets through the platform handoff; simulation only retrieves ready
futures, and never blocks on CPU preparation or GPU upload:

```cpp
auto workers = engine.make_worker_group();
auto platform = engine.platform_dispatcher().submission();
auto loader = std::make_shared<CE::Assets::Loader>("assets");
auto preparing = workers.submit([loader, platform] {
    auto prepared = loader->prepare();
    return platform.submit(
        [loader, prepared = std::move(prepared)](CE::Engine::EngineContext& owner) mutable {
            loader->upload(std::move(prepared), owner.resources());
            return loader->manifests();
        }
    );
});
// A later update: only after preparing.wait_for(0s) reports ready, get the
// returned upload future. Check that future's readiness before reading metadata.
```

An owned root or injected shared root supplies physical capacity; this group
belongs to the context shutdown domain. Accepted CPU jobs finish while the
platform dispatcher remains available. Any final unexecuted upload is cancelled
before resource teardown, so its future reports failure instead of hanging.
Use platform submission endpoints rather than borrowed dispatcher pointers.

## Publication and retry

`PreparedAssets` owns decoded pixels, shader bytes and definitions, with no provider or native
handles. Preparation failure changes no caches or published metadata. Upload
consumes the supplied value on the provider's loading owner; moving it into a
request transfers that ownership. Retry needs a fresh preparation or a preserved
copy. Pending platform requests can cancel before execution under the
[dispatcher contract](../runtime/thread-dispatch.md); executing uploads have no
rollback or mid-batch cancellation contract.

Each created cache entry can become visible before the next entry is created.
Upload failure preserves completed entries and the last successful metadata
snapshot. Allocation of that final snapshot can fail even after all entries exist.
Retry skips existing keys rather than replacing them, so published manifests
describe submitted definitions rather than an atomic view of current caches.
Changed files under the same keys do not provide hot reload. Readers retain old
metadata snapshots after another upload or loader destruction.

Upload checks the optional provider-owned `shader_asset_builder()` capability,
then publishes programs/materials before images and sprite/tileset resources.
Providers without that capability reject selected shader recipes explicitly.
`ShaderAssetMgr` retains each recipe together with its executable generation;
material construction uses the retained program generation even when an ordinary
load preserves an existing program. Asset selections retain their material handle.

`upload(prepared, provider, true)` explicitly replaces shader generations only.
Each candidate is complete before publication; failure preserves that key's old
generation, while earlier successful replacements remain published. Existing
assets and frames keep their old handles rather than automatically rebinding.
`shader_manifests()` and `manifests()` describe the last successful submitted
definitions, not an atomic cache snapshot. `diagnostics()` reports the last attempt,
including partial publications/replacements. There is no watch-based reload or
whole-batch rollback.

`load_assets(provider)` is the synchronous prepare/upload convenience path.
Construct separate loaders for separate roots. Legacy `Loader::get(root)` remains
available but rejects a different root after its first initialization; `get()` only
retrieves an already-initialized singleton.

## Resources and application bootstrap

The generic loader loads indexed shaders/materials, images, sprites and tilesets.
The application still chooses fonts explicitly; the
[file/font discovery guide](file-and-font-discovery.md) defines candidate selection,
index freshness and FontMgr's default/collection-face policy. The demo selects
`graphics/shaders/graphics-manifests.json` and retrieves `main:text` and `main:images`
from `ShaderAssetMgr`, including with `--full-assets`. F5 prepares that same index
and requests explicit replacement. The demo adopts its pair only after both
replacements succeed; failed reload preserves its previous application pair even
if the shared catalogue has published an earlier candidate. Fonts remain outside
the graphics manifest contract. Manual material builders remain supported; the
[rendering guide](../rendering/pipelines-and-materials.md#manual-material-construction)
shows the UI example that deliberately retains this path.

`ResourceProvider::create_image` accepts decoded RGBA pixels.
`upload_geometry(span<const Vertex2D>, topology)` copies the transient view before
returning. The older shared-pointer/count overload keeps its CPU owner only through
that upload. Atlas cells use independent four-vertex strips; images and glyphs use
six-vertex triangle quads. Backend handles never retain these CPU buffers. OpenGL
checks dimension, byte-count, vertex-count, and draw-range limits before use.

## Typed manifest data and consumers

Engine definition types retain grid origins, frame sizes, spacing, pivots, views,
orientations, timed clips and facings, animated tile targets, Wang terrain metadata,
weighted variants, and neighbor-mask lookup tables. `Sprite::definition()` and
`Tileset::definition()` expose them after construction. Entity-owned
`SpriteAnimation` already advances timed clips and publishes resolved cells; the
cached sprite has no mutable playback cursor. Tile-map neighbor selection and
application meanings for views/orientations remain gameplay work.
The [asset value and playback contract](asset-values-and-playback.md) defines direct
construction limits, borrowed metadata, lookup failure and submission ownership.

Use `ManifestLoader::load(file)` or `parse(stream, source)` for document-only tools.
Use the owning focused runners or `all-tests` for regressions;
[architecture validation](../development/architecture-validation.md) describes
selection and coverage limits.
