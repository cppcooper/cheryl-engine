# Unicode text layout and font resources

## Choose a text path

| Content | Use | Working sequence |
| --- | --- | --- |
| A game-owned label or HUD message | Engine's configured default or a registered font, retained text updater | [Display and update text](../getting-started/how-to/update-text.md): supply content/options, advance replacement, and submit its current generation in a frame. |
| Text inside a native widget or RML document | The selected toolkit's authoring interfaces with Engine fonts | A Context-aware UI session installs Engine's default. Use the [TGUI](../modules/tgui.md#fonts) or [RmlUi](../modules/rmlui.md#fonts) bridge to select a registered font for an element. |
| Existing ASCII or bitmap-atlas consumers | The legacy Font interfaces and their metrics | Keep their atlas, scale and bank compatibility contract during migration. |

The [Text system](../engine-systems/text.md) identifies the public types, while
[Threads and Data Flow](../architecture/threads-data-flow.md#prepare-and-publish-a-replacement)
explains replacing a message without changing already published frames.

## Supported rendering scope

The initial rendering scope is accented Latin (including English, French and German),
Russian/Cyrillic, mixed paragraph direction and optional width-constrained wrapping.
Glyph coverage and shaping are font/script concerns rather than a separate renderer
for every language. Color emoji is a planned
[next text batch](../../planning/mid-term/README.md#color-emoji);
wider CJK/script acceptance remains in the
[long-term plan](../../planning/long-term/README.md#other-engine-extensions).
Display source maps do not establish caret movement, selection or IME/preedit.

The service pairs face-qualified glyph placements with complete immutable page
generations. Its public stages have separate owners:

| Namespace | Ownership |
| --- | --- |
| `CE::Text` | Immutable font collections, UTF-8/source records and shaped layouts; no uploaded resources. |
| `CE::Text::Rendering` | `Prepared` owns CPU layout/raster pages; `Generation` retains their uploaded geometry/atlas handles. |
| `CE::Rendering::Submission` | `resolve_text()` and `resolve_text_runs()` copy draw values into retained packets. |

`CE::Text::Legacy::Font` exposes one atlas/geometry pair, which cannot represent
that fallback model. Its `AsciiFont`/`BitmapFont` implementations and toolkit-owned
text retain their separate roles.

Engine also owns the opaque
[`CE::Text::FontRasterizer`](../../../projects/engine/include/cheryl/text/font-rasterizer.h)
CPU service for loading copied font bytes, properties, metrics, glyph pixels and
kerning. Copies retain and serialize native face state; returned data owns its
storage and needs no provider/context. TGUI and RmlUi adapters use this shared
implementation while retaining their own toolkit layout/atlas/effect logic.
FreeType rasterization, HarfBuzz shaping and ICU Unicode processing remain
distinct responsibilities, all implemented inside Engine. Toolkit color-glyph
handling does not establish color-emoji acceptance for this Engine layout path.

Those native libraries use shared linkage. Linux uses distribution ICU libraries;
Windows 10 version 1903+ uses the operating system's ICU C API. Applications use
the same Cheryl text interfaces on both platforms; see
[dependency settings](../development/building.md#dependency-and-toolchain-settings)
for build and runtime requirements.

## Configure fonts once

Include `<cheryl/text/font-service.h>` for catalogue configuration and retained
fonts, and `<cheryl/text/layout.h>` when laying out direct Engine text.

`engine.fonts()` owns a `CE::Text::FontService`. With no configuration,
`default_font()` returns Engine's embedded DejaVu Sans face; displaying a label
does not require a font file or installed system font:

```cpp
auto font = engine.fonts().default_font();
auto layout = CE::Text::layout_text(font, "Score: 0", {.pixel_height = 24});
```

Configure application fonts at startup, before creating UI sessions or other
font consumers. Existing consumers retain their selected font snapshots.

### Set an ordered application default

Use `FontSelection` when the game has an ordered list of preferred default fonts:

```cpp
CE::Text::FontSelection selection;
selection.asset_directories = {asset_root};
selection.automatic_system_fonts = true;
selection.preferred = {
    "ProggyVector", "Cantarell", "FreeSans",
    "Adwaita Mono", "FreeSerif", "DejaVu Sans"
};
engine.fonts().configure(selection);
auto font = engine.fonts().default_font();
```

Keep the game's existing `asset_root` and preference list. Missing names are
skipped. The first available face becomes primary; later faces provide glyph
fallback in preference order, followed by automatic choices when enabled and the
embedded safety face. Each name resolves application assets before installed fonts.

For code previously calling `FontCollection::load(selection)`, configure the
service with that same selection to establish Engine and context-aware GUI
defaults. New consumers can retain the returned `Font`; collection-based callers
can use `font.collection()`. The standalone loader remains useful for independent
CPU font processing.

This overload keeps the stable reference `application-default`. Use
`configure(selection, "interface")` to give the ordered policy an application
reference. Selected source faces retain path/collection-index identities,
independent of preference positions; the embedded face uses `cheryl:embedded`.
The chosen reference cannot collide with embedded or selected source references.
See [Font selection](#font-selection) for source forms and discovery details.

### Register named families and groups

Use `FontConfiguration` when the application needs separately named font choices,
style variants, or selection groups. A `FontFamily` gives a font a stable application reference.
Its sources supply the family's regular, bold and italic faces. A named group
contains alternative family references to choose from; it is separate from
style variants and glyph fallback.

```cpp
CE::Text::FontConfiguration configuration;
configuration.asset_directories = {asset_root};
configuration.families = {
    {"interface", {"Interface.ttf"}},
    {"book-serif", {"Book Serif"}},
    {"handwriting", {"Handwriting.ttf"}}
};
configuration.default_font = "interface";
configuration.groups = {{"books", {"book-serif", "handwriting"}}};
configuration.fallbacks = {"interface"};
engine.fonts().configure(configuration);
```

The filenames and family names above are application choices. Sources use the
same `FontSource` forms and discovery roots as [Font selection](#font-selection).
Named-family registration retains available style variants, including explicit
collection-face indices. Explicit files can list separate regular/bold/italic
sources under one reference. Registering a specialty font does not add it to
other fonts' fallback chains; `fallbacks` selects that common chain explicitly.
The embedded face remains its terminal fallback.
Adapters obtain the complete ordered chain through `FontService::fallbacks()`;
each returned handle contributes its primary family, including when the default
family also appears in the configured fallback list.

`default_font()` returns the configured default when available, otherwise the
embedded font. `default_font` selects one family reference; the registration order
and `fallbacks` list do not select another default when that family is unavailable.
The common fallback list supplies missing glyphs for available fonts.
`get("handwriting")` selects that registered font; an unknown or
unavailable reference resolves to the default. `all()` lists the available
registered handles and the embedded handle. References are case-sensitive
application identifiers, independent of registration order and physical family
names. Empty references, embedded NUL and the reserved `cheryl:embedded` reference
are invalid for application registrations. Duplicate family references reject
configuration.

Context-aware TGUI and RmlUi sessions obtain the configured fonts directly.
Their default inherits through the toolkit's widgets/documents. For a specific
element, use TGUI's `font()`/`set_font()` or RmlUi's `set_font()` bridge with an Engine
`Font`, or the RmlUi alias returned by `font_family()`. Native widget authoring remains in
the selected toolkit; see [GUI integration](../engine-systems/gui.md) for ownership
and the [TGUI](../modules/tgui.md#fonts) or [RmlUi font contract](../modules/rmlui.md#fonts)
for native style and alias rules.

### Retained font handles and style

`CE::Text::Font` is a copyable, resolved CPU handle. It retains font bytes, style
metadata, collection indices and its ordered fallback families. It remains usable
after the service or Context is destroyed. `layout_text(font, text, options)`
does not need a current Engine context. Select a style through layout options:

```cpp
auto book_font = engine.fonts().get("book-serif");
auto heading = CE::Text::layout_text(
    book_font, "Chapter One",
    {.pixel_height = 32, .font_style = {.weight = 700, .italic = false}}
);
```

`font.collection(style)` selects the closest available style from each retained
family for the existing collection-based APIs. Selection prefers matching italic
state, then the closest numeric weight. Adapter-facing `faces()` exposes borrowed
bytes and face/style metadata; retain the `Font` while using those views.
`FontRasterizer::load(font, style)` opens the selected primary style, while
`load(face)` opens an exposed face directly. A single rasterizer does not perform
the collection's fallback selection.

### Choose and save a font

Random choice is explicit. `choose_random("books")` returns a resolved `Font`;
ordinary font lookup and rendering do not choose again. To restore a saved choice,
pass its reference to `restore_or_choose()`:

```cpp
// These two save functions belong to the application.
const std::string saved_reference = load_book_font(book_definition_id);
auto book_font = engine.fonts().restore_or_choose("books", saved_reference);
save_book_font(book_definition_id, book_font.reference());

auto page = CE::Text::layout_text(book_font, contents, {.pixel_height = 24});
```

Run this at the application's shared book initialization boundary. Keying saved
choices by book definition makes other copies of the same book reuse the choice;
an instance key permits individually styled copies. The book keeps the returned
font handle. The application owns these identities and its save format.

Restoration keeps a reference only while it is still in the selected group and
its font loaded successfully. Otherwise it chooses among the group's available
members. A missing/empty group or one with no available members uses the
application default, then the embedded fallback. Save the returned `reference()`
to persist replacements. Group changes do not reroll an existing choice that
remains available and belongs to the group.

The simple overload uses the service's random generator. Overloads taking a
caller-owned `std::mt19937_64&` support controlled selection; restoring an available
group member does not advance it. Save the chosen reference, not the random seed,
runtime handle, or font bytes. Changing group contents or random call order cannot
reinterpret an already saved reference.

### Loading and availability

Configuration loads CPU snapshots and publishes the complete catalogue together.
Missing or unloadable optional families are unavailable for this configuration;
invalid source syntax and allocation failures propagate. Failed configuration
leaves the previous catalogue usable. Catalogue operations are synchronized,
and existing font handles remain valid when configuration is replaced. Configure
before consumers: replacement does not update existing widgets or font handles.

Lookup does not rescan files or detect source replacement. Removing a source file
after configuration does not invalidate its retained bytes. A later explicit
configuration evaluates availability again. References identify the application's
font choices; they do not identify a particular revision of a font file.

For an ordered default policy, use the [selection overload](#set-an-ordered-application-default).
Its selected bytes are also retained for toolkit consumers without reopening paths.

## Font selection

`CE::Text::FontCollection::load(selection)` creates an immutable CPU snapshot of font
bytes. Put font names in `FontSelection::preferred`, in the order the game should
try them. Each name searches application fonts first, then installed system fonts;
the engine discovers system directories without application setup. For example:

```cpp
CE::Text::FontSelection font_selection;
font_selection.preferred = {"Arial", "Noto Sans", "DejaVuSans.ttf"};
font_selection.automatic_system_fonts = false;
auto fonts = CE::Text::FontCollection::load(font_selection);
```

Here `Arial` and `Noto Sans` are family names read from font metadata, independent
of their filenames. `DejaVuSans.ttf` is a filename alias for an application font;
it can be nested anywhere under the application asset roots. Names compare ASCII
letters without case. System fonts resolve by family name; filename aliases apply
only to application fonts. A missing name is skipped, so other preferences still
load. Empty names and names containing NUL are invalid. The embedded fallback
remains available even if none of these names resolve.

`asset_directories` defaults to `{"assets"}`, relative to the process's working
directory. Font discovery recursively scans these roots for `.ttf`, `.otf`, `.ttc`
and `.otc` files, without requiring a `fonts/` subdirectory or graphics manifest.
If the game already has a selected asset root, set it once:

```cpp
font_selection.asset_directories = {asset_root};
```

Set that before calling `load()`. An empty list disables application-font discovery.
When both an application font and a system font match a name, the application
version wins. Within the chosen inventory, selection first prefers faces that are
neither bold-flagged nor heavier than Medium (weight 500), then upright faces, then
the declared OpenType weight closest to Regular (400). When numeric weight metadata
is absent, the bold flag supplies a 700/400 estimate. Sorted path/face order breaks
ties, including repeated application filenames. Discovery skips missing, unreadable
and unsupported candidates; allocation failures propagate.

For explicit control, the same `preferred` list also accepts
`CE::Text::FontFile{path, face_index}` and `CE::Text::SystemFontFamily{name}`.
`FontFile` loads that path directly; a relative path uses the working directory,
not the asset roots. Its zero-based face index selects an ordinary collection face
with a variable font's default coordinates, rather than a named variation instance.
Missing explicit files are skipped; existing unreadable, corrupt, nonscalable or
non-Unicode faces fail the whole load. `SystemFontFamily` restricts lookup to the
installed inventory, bypassing application overrides; unavailable families are
skipped. Nothing is published on failure, and earlier font collections remain usable.

System discovery uses the existing
[root traversal](file-and-font-discovery.md#system-font-candidates). Leave
`system_directories` unset for automatic system roots; an explicit list replaces
them, and an empty list disables installed-font search. Keep environment/files
stable during loading. A snapshot no longer needs those files afterward.

With `automatic_system_fonts`, append available Arial, Segoe UI, Helvetica, Noto Sans,
DejaVu Sans and Liberation Sans families in that order, preferring application
fonts over system fonts and skipping a family if its selected face is bold-flagged
or heavier than 500. A heavy asset override therefore skips that automatic family
even when a lighter system copy exists. Disabling it leaves explicitly preferred
names, files and families active. Preferred sources remain unrestricted by this automatic-weight
limit; name/family lookup can select a heavy face when no lighter one exists.
These preferences are not guaranteed to exist across operating systems or regions.
Repeated path/face entries are deduplicated. The terminal fallback is always an
[embedded, licensed DejaVu Sans](../../../assets/fonts/README.md) face independent of the
application's fonts, UI modules and installed inventory. The fallback covers the
initial alphabets and contains a visible U+FFFD replacement; it cannot supply the
original glyph for every Unicode character.

`faces()` returns metadata in selection order. Its indices identify faces only
within that retained collection. `covers(scalar)` checks whether any selected cmap
contains the scalar; it does not prove whole-grapheme coverage or successful shaping.
Each query opens local FreeType state, so immutable collections do not share mutable
face/size state between CPU workers. Library-native types stay out of public headers.

## Shaping, direction and width

`CE::Text::layout_text(fonts, utf8, options)` returns immutable `CE::Text::ShapedText`: original
[decoded scalar records](text-encoding.md), face-qualified glyph IDs/positions,
shaping-cluster byte/scalar ranges and line metrics. It retains its font collection
and copied options but no borrowed text, provider or uploaded resource. The font
size is an em size in local pixels, with Y-up baseline coordinates. It is independent
of camera/screen units and does not change the legacy AsciiFont/BitmapFont metric contract.

[HarfBuzz](https://harfbuzz.github.io/what-is-harfbuzz.html) handles OpenType shaping,
kerning and ligatures. A precomposed accent and its combining sequence can produce
the same glyph while retaining different source ranges. Multiple glyphs may share a
cluster; a ligature can cover multiple scalars/graphemes. Cluster ranges describe
display associations, not selectable caret stops. Language is an optional BCP 47
hint (default `und`); it does not filter scripts or translate text. Invalid tags fail.

ICU resolves [paragraph bidi](https://unicode-org.github.io/icu/userguide/transforms/bidi.html)
before visual run shaping, and applies line direction after breaking. Direction is
automatic (first strong character, otherwise LTR), explicit LTR or explicit RTL.
Latin/digits retain their natural run direction in an RTL paragraph. Unicode overrides,
isolates and default-ignorable formatting participate in bidi without drawing visible
replacement boxes. ICU resolves bidi controls before shaping; those controls stay
in the owned scalar records and line ranges but are omitted from the shaping buffer
so their removal cannot replace a visible glyph's source index. Joiners and variation
selectors remain available to HarfBuzz. With a width, RTL paragraphs align to its
right edge; without a width, visual lines start at local X=0. CR/LF/CRLF and Unicode
paragraph separators start paragraphs; U+2028 forces a line within its existing paragraph.

ICU [grapheme boundaries](https://unicode-org.github.io/icu/userguide/boundaryanalysis/)
define fallback units. Select the first face covering all visible scalars of each
grapheme, keeping combining marks with their base. An uncovered grapheme becomes
one U+FFFD from the embedded face, with `ShapedGlyph::missing` set and its original
source range retained. A notdef returned by shaping also becomes a visible embedded
replacement. Literal U+FFFD and malformed UTF-8 remain distinguishable through scalar
records; encoding replacement and missing font coverage are separate properties.
Embedded NUL is a scalar requiring fallback, rather than an end marker.

An optional positive finite `maximum_width` enables greedy Unicode line breaking.
The UI/application derives that local width from its container and text scale; layout
does not query a window or UI object. Chosen lines are shaped with their actual line
boundaries and accepted by measured advance, preserving paragraph bidi across wraps.
Oversized words break at grapheme boundaries. A single too-wide grapheme is admitted
on its own line and marks `overflow`; clipping/scaling remains a caller decision.
Tabs expand to four spaces. Trailing ASCII spaces/tabs and line separators have no
visual advance, while line source ranges retain their consumed bytes. Other spaces
follow font/Unicode break rules. No automatic hyphenation, justification or vertical
writing is supplied.

Text byte/UTF-16 and font size limits are checked before narrowing into library
indices/metrics. Invalid options fail before publication, allocation/library failures
propagate, and previous layouts are unchanged. Each layout uses local FreeType,
HarfBuzz and ICU state, allowing concurrent queries against one font collection.
Line fitting currently measures candidate prefixes; very long constrained paragraphs
can require repeated shaping. Optimize that only with representative measurements,
while retaining the actual accepted-line width and source/bidi contracts.

## Preparation, upload and retained submission

The CPU and provider-owner stages produce complete values. Frame preparation
then resolves packets from the selected uploaded generation.

```mermaid
flowchart LR
    accTitle: From shaped text to retained draw packets
    accDescr: A shaped layout is prepared into owned CPU glyph pages and geometry, uploaded into a complete generation, then resolved into packets retaining matching page resources.
    layout["Shaped layout"] -->|prepare| pages["CPU pages and quads"]
    pages -->|upload| generation["Uploaded generation"]
    generation -->|resolve| packets["Retained packets"]
```

`CE::Text::Rendering::prepare(shaped, raster_options)` owns the layout and prepares
grayscale alpha pages and six-vertex glyph quads on the CPU. It rasterizes each
distinct face-qualified glyph once for that message, retaining glyph bearings and
shaped placement offsets. Spaces/empty outlines advance without drawing. Pages use
one transparent border pixel, are cropped to used padded bounds and preserve the
font provider's row/UV order. The configured maximum page extent is 4..4096 pixels
(default 1024); a glyph too large for one padded page fails before a bitmap allocation.
There is no shared glyph cache, in-place repack, eviction or residency budget.

`CE::Text::Rendering::upload(prepared, provider)` copies CPU buffers into fresh immutable page
geometry/atlas handles on the provider's upload owner. A nonempty generation also
retains the provider's default textured-triangle material; an optional third
material argument selects an explicit handle instead. No application shader
files or material factory are required for the default path. Empty text needs no
built-in material capability. Passing a copy preserves CPU
preparation for retry; moving it into an owned
[dispatcher request](../runtime/thread-dispatch.md) transfers that
value. A successful call returns one `CE::Text::Rendering::Generation` with its matching
placements and resources. A failure returns no generation and publishes no replacement.
Already created transient resources follow backend retirement; this is not an atomic
native-resource transaction. Callers publish a complete candidate only after success.

`CE::Rendering::Submission::resolve_text(rendered, style, context)` performs CPU packet resolution alongside
the legacy font overload. An absent `style.material` uses the generation's retained
material. A default `Context2D` supplies the public `image` key at unit zero so
each glyph selects its own generation page. Custom contexts can choose another
nonempty key/unit; that key must be absent from draw parameters. Glyph offsets are scaled and transformed by the supplied model exactly
as in legacy submission. Packets copy values and retain both page handles. Replacing
or releasing the text/font/preparation cannot pair old quad ranges with a newer atlas.
Old submitted frames retain their resources, subject to the original backend domain
remaining alive for native use. Submission performs no shaping, discovery, rasterization,
upload, binding or drawing.

### Retained text updates

`CE::Text::Rendering::Updater` in `<cheryl/text/text-updater.h>` owns the
replacement sequence for a changing label. Construct it with a `Context` during
platform initialization, then call `request()` and nonblocking `update()` on the
simulation owner. The application supplies a value-owned `Request`: font, UTF-8
text, layout/raster options and optional material. An empty font selects the
default captured at construction. A synchronous initial-request constructor
provides `current()` immediately; the constructor without a request starts empty.

Preparation uses a context-tracked worker group; upload uses its saved platform
submission endpoint. Jobs retain owned values and never borrow the updater or
game. Runtime's existing worker/dispatcher shutdown remains responsible for
settling or cancelling accepted work. Destroying the updater does not wait for
jobs. Serialize updater calls and stop simulation use before platform cleanup.

One active request and the newest desired request bound outstanding work.
Equal requests compare all layout/raster fields, material identity and retained
font-snapshot identity; they submit no duplicate work. A completed active result
is adopted even if a newer request is waiting, then that newest request starts.
Intermediate desired requests are coalesced. Older frames retain their resources.

Asynchronous submission/execution failures leave the previous `current()` intact.
`failure()` retains the preparation/upload stage, exception and failed revision
for application reporting. A distinct request clears that report; retrying a
settled failed request clears it too. An older active request can subsequently
fail while a newer desired request proceeds. Unchanged failed work stays settled
until explicit retry.
Initial synchronous construction failures propagate. No automatic retry loop or
background mutation of the game's current generation occurs.

The [text task](../getting-started/how-to/update-text.md) gives the complete
initialization/update/submission/cleanup sequence. Direct `layout_text()`,
`prepare()` and `upload()` remain available for application-owned scheduling.

### Positioned text runs

Set `CE::Text::Rendering::RasterOptions::positioned_runs = true` during `CE::Text::Rendering::prepare()` to retain
additional geometry with glyph placements baked into its vertices. Ordinary page
geometry remains available for per-glyph submission; opting in trades preparation,
upload and geometry storage for fewer per-frame packets. The extra geometry repeats
quads in authored glyph order and records each contiguous atlas-page range. A page
sequence A/B/A remains three runs rather than grouping both A ranges together.

After upload, call `CE::Rendering::Submission::resolve_text_runs(rendered, style, context)` to emit one packet
per run. Preparation must have opted in; an ordinary generation rejects this call.
The image key/unit, immutable sampling, clipping, material and whole-text model/scale
remain those selected by the caller. Packets retain the positioned geometry and
matching atlas generation through replacement/release; empty text emits no draws.

The default material supports this overload. Custom materials require a shader applying the common scale/model transform to the
positioned vertices, without per-glyph or draw-identity semantics. The API cannot
infer that contract from arbitrary shaders. Use `CE::Rendering::Submission::resolve_text()` for those shaders
or when the original per-glyph raster evaluation is required: regrouping floating-point
transforms can change coverage of exact boundary pixels. The default preparation
and submission remain per glyph. This facility does not merge arbitrary frame
packets, reorder draws or change either UI adapter's batching policy.

The [demo](../../../projects/apps/demo/README.md#builtin-unicode-text) submits immutable
HUD/camera text and uses the retained updater for changing HUD content. Its preview supports builtin-only, automatic, explicit file/
family and paragraph-direction choices. The demo guide preserves
[native preview procedures](../../../projects/apps/demo/README.md#unicode-preview-validation).
Its `--text-runs` option selects positioned submission for builtin text and sample
labels; toolkit-owned text keeps its separate path.

## Native acceptance scope

Linux/GLFW/X11/OpenGL sequential/concurrent preview appearance, wrapping, paragraph
alignment and resource replacement/shutdown are accepted for the initial grayscale
scope. Similar output across runtime/font variants is compatible with the same
effective glyph faces and placements. Right-aligned Latin under explicit RTL and
unchanged Hebrew-only output between automatic/RTL modes match the direction
contract above; this native report does not independently confirm Hebrew glyph order.
The accepted [CPU bidi case](../../../projects/engine/tests/unit/src/text-layout.cpp)
checks Hebrew/mixed visual source order and preserves Latin order under explicit RTL.
Alignment alone is not proof of every bidi sequence.

Linux OpenGL run acceptance compares the test application's 64×64 backbuffers with
`glReadPixels`, drawing the same retained generation through per-glyph and run paths.
It covers nearest/linear sampling, clipping, signed scale/rotation, page transitions
and replacement lifetime. It does not capture the desktop or other applications.
These fixture comparisons and CPU submission measurements do not establish every
boundary-pixel identity, physical-GPU timing or other-platform appearance.

CPU and adapter fixtures cover registered weight/style selection and collection
face indices. Automatic discovery's numeric-weight boundaries remain in the
[font-selection plan](../../planning/short-term/README.md#font-selection-regressions). These fixtures do
not establish native visual correctness of distinct authored style faces. Color/CJK rendering,
grapheme/bidi editing, IME and additional platforms retain separate scope. Reusable
[CPU checks](../development/architecture-validation.md#engine-asset-and-text-regressions)
and these Linux observations do not establish other-platform coverage.
