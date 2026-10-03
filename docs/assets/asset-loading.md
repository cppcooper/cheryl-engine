# Asset loading

`ManifestLoader` parses manifest 1.0 into CPU-only definitions. An owned `Loader`
keeps one immutable root: `prepare()` discovers documents and decodes owned RGBA
pixels without a resource provider; `upload()` creates missing backend assets and
publishes a retained metadata snapshot on the provider's loading thread.

## Runtime entry point

After initializing the selected backend, load a root with its provider:

```cpp
CE::Assets::Loader loader("assets");
loader.load_assets(engine.resources());
auto actor = CE::Assets::SpriteMgr::get().get_asset("miniworld:soldier_swordsman_cyan");
auto documents = loader.manifests(); // shared_ptr<const vector<AssetManifest>>
```

Each preparation scans again. Only root-level JSON files are manifests;
`assets/schemas/*.json` is excluded. Image paths are resolved relative to each
manifest, and asset keys use `namespace:name`. Referenced images and standalone
PNGs below the root are deduplicated and decoded in sorted order. Cross-document
IDs and every grid's bounds are checked against those exact decoded dimensions
before any upload. Upload consumes the owned pixels without reopening image files.
Pixels are four-channel RGBA in the decoder's default top-to-bottom row order.
OpenGL reverses those rows in transient upload storage to match the atlas
geometry's upward-positive UV coordinates; the supplied pixels remain unchanged.
The separate stb alpha-atlas path preserves its baked row/UV convention.

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

`load_assets(provider)` is the synchronous convenience path. Preparation failure
changes no caches or published metadata. Upload failure may retain already-created
cache entries, but metadata stays at the previous successful snapshot. Repeated
loads preserve existing asset keys; this is not an atomic asset hot-reload API.
Readers retain old metadata snapshots even after another upload or loader destruction.
Construct separate loaders for separate roots. Legacy `Loader::get(root)` remains
available but rejects a different root after its first initialization; `get()` only
retrieves an already-initialized singleton.

## Resources and application bootstrap

The generic loader loads images, sprites, and tilesets. The application explicitly
chooses system fonts and shader recipes. The demo always selects its font and
`shader2d` material recipe, including with `--full-assets`; F5 queues material
replacement built from the shader recipe. Failed replacement keeps the previous
generation, and published packets retain their selected generation. Fonts and
shader/material recipes remain outside manifest 1.0.

`ResourceProvider::create_image` accepts decoded RGBA pixels.
`upload_geometry(span<const Vertex2D>, topology)` copies the transient view before
returning. The older shared-pointer/count overload keeps its CPU owner only through
that upload. Atlas cells use independent four-vertex strips; images and glyphs use
six-vertex triangle quads. Backend handles never retain these CPU buffers. OpenGL
checks dimension, byte-count, vertex-count, and draw-range limits before use.

## Typed manifest data and consumers

`assets/definitions/` retains grid origins, frame sizes, spacing, pivots, views,
orientations, timed clips and facings, animated tile targets, Wang terrain metadata,
weighted variants, and neighbor-mask lookup tables. `Sprite::definition()` and
`Tileset::definition()` expose them after construction. Entity-owned
`SpriteAnimation` already advances timed clips and publishes resolved cells; the
cached sprite has no mutable playback cursor. Tile-map neighbor selection and
application meanings for views/orientations remain gameplay work.

Use `ManifestLoader::load(file)` or `parse(stream, source)` for document-only tools.
Manifest, preparation/upload, and runtime-adapter regressions belong to the
aggregated `all-tests` target. Recorded build and execution scopes are in
[architecture-validation.md](../development/architecture-validation.md).
