# File indexing and font discovery

File indexing, font discovery and asset loading have separate policies. The
[asset loader](asset-loading.md) constructs a fresh `CE::Files::FileIndex` for each
preparation. Unicode font selection and text preparation run on the CPU; text
upload uses the resource provider separately. Legacy fonts retain their
provider-owned loading path.

Automatic graphics preparation registers JSON, WAV and shader paths in a fresh
private `CE::Assets::Loading::FileRegistry`; only exact `graphics-manifests.json` indexes select documents.
Unlisted JSON remains unopened. `Loader::register_files()` can publish unmanaged
paths without parsing/decoding; it does not initialize fonts or load BitmapFont. The
[graphics path contract](asset-manifests.md#index-selection-and-paths) uses the
enclosing `graphics/` directory for index, texture and source references.

## File index

[CE::Files::FileIndex](../../projects/engine/include/cheryl/core/resources/fileio/file-mgr.h)
is an independent, incremental index. Construction scans the supplied root;
`search_directory()` appends an unvisited directory tree. Missing and non-directory
roots add nothing. Directories encountered during a scan are marked visited, so
searching the same root or an already encountered descendant does not refresh it.
Adding new files, deleting files or changing extensions does not update existing
buckets. Create a new index when a fresh scan is required; no refresh API is selected.

Keys use `std::filesystem::path` equality without canonicalization. Different aliases
or path forms may index the same physical files more than once. Recursion does not
follow directory symlinks, while file status follows symlinks to regular files.
Returned paths retain the walk's path form. Enumeration order is unspecified and
buckets are neither sorted nor deduplicated.

Extension keys include their leading dot and are lowercased using the current C
character locale. Files with no extension use `file_no_ext`.
`get_files_of_type(".PNG")` looks up `.png`; `get_files_of_type("png")` selects a
different bucket. A missing lookup creates an empty bucket.

The returned vector is borrowed from the manager and survives other bucket insertions,
but subsequent indexing can append to it and invalidate element references or
iterators. Reborrow after moving or assigning the manager; destruction or replacement
of the owning index invalidates the reference. All access requires external
serialization, including lookups, because they can insert buckets.

Filesystem and allocation failures propagate. A failed search can retain a partial
index and visited-directory markers, including the root recorded before traversal;
reconstruct the index to retry that root. There is no atomic discovery snapshot or
automatic recovery from changed files.

## Font names and application assets

[`CE::Text::FontSelection::preferred`](text-layout.md#font-selection) accepts an
ordered list of names such as `{"Arial", "Noto Sans", "DejaVuSans.ttf"}`.
Family names come from font metadata. Filename aliases, including their extension,
also select application fonts, regardless of where they are nested under an asset
root. Both forms compare ASCII letters without case. Application discovery scans
`asset_directories` recursively, defaulting to `{"assets"}` relative to the working
directory. Games using a different asset root set that list once; preferred names
contain no directories.

Names resolve against application fonts before system fonts, so a matching asset
font overrides an installed copy. The engine discovers system roots automatically;
system files match family names rather than filename aliases. Missing names and
unusable discovery candidates are skipped. `FontFile` remains available for direct
path/face selection and `SystemFontFamily` for an explicitly system-only choice.
The [text guide](text-layout.md#font-selection) defines ordering, style ranking,
failure behavior and the embedded fallback. The lower-level functions below return
candidate paths; their filename heuristic is separate from this name resolution.

## System-font candidates

[fonts-system.h](../../projects/engine/include/cheryl/core/resources/fileio/fonts-system.h)
returns owned paths and performs no provider work. `CE::Text::system_font_directories()` lists
`/usr/share/fonts`, `/usr/local/share/fonts`, `/Library/Fonts` and
`/System/Library/Fonts`, then appends paths derived from nonempty environment values:

| Variable | Appended paths |
| --- | --- |
| `HOME` | `.fonts`, `.local/share/fonts`, `Library/Fonts` |
| `XDG_DATA_HOME` | `fonts` |
| `WINDIR` | `Fonts` |
| `LOCALAPPDATA` | `Microsoft/Windows/Fonts` |

These are candidates on every platform; the list does not check existence or remove
duplicates. Keep environment mutation serialized with calls that read it.

`CE::Text::find_system_fonts()` scans those roots. The explicit-root overload recursively
collects regular files with case-insensitive `.ttf`, `.otf`, `.ttc` or `.otc`
extensions. It skips missing, non-directory and inaccessible roots and permission-
denied subtrees. Other filesystem errors may truncate a root's results without
diagnostics; the returned list can be incomplete. Allocation and path-conversion
failures still propagate. Directory symlinks are not followed during recursion;
symlinks to regular font files may appear.

Paths are lexically normalized, sorted by path ordering and deduplicated by path
equality. This does not canonicalize aliases or establish that two paths refer to
different files. Discovery validates neither file contents nor supported glyphs.
Files may change between discovery and loading.

`CE::Text::select_default_system_font()` checks case-insensitive filenames in this order:
`arial.ttf`, `helvetica.ttc`, `dejavusans.ttf`, `liberationsans-regular.ttf`,
`nimbussans-regular.otf`, `notosans-regular.ttf`, `freesans.ttf`, `segoeui.ttf`,
`roboto-regular.ttf`, `calibri.ttf`, `consola.ttf`, `proggyvector regular.ttf`.
For a preferred filename occurring more than once, the first supplied path wins.
Without a preferred name, the minimum path wins regardless of supplied order; an
empty list produces `nullopt`. This heuristic performs no I/O or family/coverage
inspection and does not try another candidate if later loading fails.

## Legacy font cache

[CE::Text::Legacy::Manager](../../projects/engine/include/cheryl/core/resources/asset-management/font-mgr.h)
loads on the active provider's loading owner under the
[cache ownership contract](../resources/resource-residency.md#cache-ownership). It uses
lexically normalized paths as cache keys, processes the supplied order and retains
already loaded entries. The first successful publication when no default exists
selects the default; later loads do not replace it. A later failure retains earlier
cache publications and their default selection.

`load(paths, provider)` loads `CE::Text::Legacy::AsciiFont` at size 32 with
printable ASCII and collection face zero.
Discovery includes collection paths, but this legacy API does not select other
faces. The separate [Unicode service](text-layout.md#font-selection) selects family
metadata, collection faces and whole-grapheme fallback; the filename heuristic
does not establish those capabilities.

`default_font()` returns a shared retained handle or `nullptr` before the first load
and after `clear_assets()`. Clearing removes cache/default ownership while callers'
shared handles retain their resources. It does not switch the shared provider domain;
provider teardown owns that release. Lookup callers use the same lexically normalized
key when calling the inherited `get_asset()`.
