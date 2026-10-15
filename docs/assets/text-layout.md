# Unicode text layout and font resources

The initial rendering scope is accented Latin (including English, French and German),
Russian/Cyrillic, mixed paragraph direction and optional width-constrained wrapping.
Glyph coverage and shaping are font/script concerns rather than a separate renderer
for every language. Color emoji is a planned
[next text batch](../planning/mid-term/README.md#color-emoji);
wider CJK/script acceptance remains in the
[long-term plan](../planning/long-term/README.md#other-engine-extensions).
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
[embedded, licensed DejaVu Sans](../../assets/fonts/README.md) face independent of the
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

`CE::Text::Rendering::prepare(shaped, raster_options)` owns the layout and prepares
grayscale alpha pages and six-vertex glyph quads on the CPU. It rasterizes each
distinct face-qualified glyph once for that message, retaining glyph bearings and
shaped placement offsets. Spaces/empty outlines advance without drawing. Pages use
one transparent border pixel, are cropped to used padded bounds and preserve the
font provider's row/UV order. The configured maximum page extent is 4..4096 pixels
(default 1024); a glyph too large for one padded page fails before a bitmap allocation.
There is no shared glyph cache, in-place repack, eviction or residency budget.

`CE::Text::Rendering::upload(prepared, provider)` copies CPU buffers into fresh immutable page
geometry/atlas handles on the provider's upload owner. Passing a copy preserves CPU
preparation for retry; moving it into an owned
[dispatcher request](../runtime/thread-dispatch.md) transfers that
value. A successful call returns one `CE::Text::Rendering::Generation` with its matching
placements and resources. A failure returns no generation and publishes no replacement.
Already created transient resources follow backend retirement; this is not an atomic
native-resource transaction. Callers publish a complete candidate only after success.

`CE::Rendering::Submission::resolve_text(rendered, style, context)` performs CPU packet resolution alongside
the legacy font overload. Supply a nonempty `context.image` pipeline key/unit so
each glyph selects its own generation page; that key must be absent from draw
parameters. Glyph offsets are scaled and transformed by the supplied model exactly
as in legacy submission. Packets copy values and retain both page handles. Replacing
or releasing the text/font/preparation cannot pair old quad ranges with a newer atlas.
Old submitted frames retain their resources, subject to the original backend domain
remaining alive for native use. Submission performs no shaping, discovery, rasterization,
upload, binding or drawing.

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

This overload requires a shader applying the common scale/model transform to the
positioned vertices, without per-glyph or draw-identity semantics. The API cannot
infer that contract from arbitrary shaders. Use `CE::Rendering::Submission::resolve_text()` for those shaders
or when the original per-glyph raster evaluation is required: regrouping floating-point
transforms can change coverage of exact boundary pixels. The default preparation
and submission remain per glyph. This facility does not merge arbitrary frame
packets, reorder draws or change either UI adapter's batching policy.

The [demo](../../projects/apps/demo/README.md#builtin-unicode-text) submits immutable
HUD/camera text, prepares replacements on a CPU worker and uploads through the
platform dispatcher. Its preview supports builtin-only, automatic, explicit file/
family and paragraph-direction choices. The demo guide preserves
[native preview procedures](../../projects/apps/demo/README.md#unicode-preview-validation).
Its `--text-runs` option selects positioned submission for builtin text and sample
labels; toolkit-owned text keeps its separate path.

## Native acceptance scope

Linux/GLFW/X11/OpenGL sequential/concurrent preview appearance, wrapping, paragraph
alignment and resource replacement/shutdown are accepted for the initial grayscale
scope. Similar output across runtime/font variants is compatible with the same
effective glyph faces and placements. Right-aligned Latin under explicit RTL and
unchanged Hebrew-only output between automatic/RTL modes match the direction
contract above; this native report does not independently confirm Hebrew glyph order.
The accepted [CPU bidi case](../../projects/engine/tests/unit/src/text-layout.cpp)
checks Hebrew/mixed visual source order and preserves Latin order under explicit RTL.
Alignment alone is not proof of every bidi sequence.

Linux OpenGL run acceptance compares the test application's 64×64 backbuffers with
`glReadPixels`, drawing the same retained generation through per-glyph and run paths.
It covers nearest/linear sampling, clipping, signed scale/rotation, page transitions
and replacement lifetime. It does not capture the desktop or other applications.
These fixture comparisons and CPU submission measurements do not establish every
boundary-pixel identity, physical-GPU timing or other-platform appearance.

Dedicated numeric font-weight/style fixtures remain in the
[Unicode plan](../planning/short-term/unicode-text.md#progress). Color/CJK rendering,
grapheme/bidi editing, IME and additional platforms retain separate scope. Reusable
[CPU checks](../development/architecture-validation.md#engine-asset-and-text-regressions)
and these Linux observations do not establish other-platform coverage.
