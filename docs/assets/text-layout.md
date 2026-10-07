# Unicode text layout and font resources

The initial rendering scope is accented Latin (including English, French and German),
Russian/Cyrillic, mixed paragraph direction and optional width-constrained wrapping.
Glyph coverage and shaping are font/script concerns rather than a separate renderer
for every language. Wider CJK/script acceptance and color emoji are deferred in the
[long-term plan](../planning/long-term-plan.md#other-engine-extensions).
Display source maps do not establish caret movement, selection or IME/preedit.

## Font selection

`CE::Text::FontCollection::load(selection)` creates an immutable CPU snapshot of font
bytes. `FontSelection::preferred` interleaves application `FontFile` paths/collection
face indices and `SystemFontFamily` names in the application's desired order. File
entries are strict: missing, unreadable, corrupt, nonscalable or non-Unicode faces
fail loading. An unavailable installed family is skipped. Nothing is published on
failure, and earlier font collections remain usable. Allocation failures propagate.

System discovery uses the existing
[root traversal](file-and-font-discovery.md#system-font-candidates), but family
selection inspects font metadata rather than guessing from filenames. Family names
compare ASCII letters without case; regular faces precede bold/italic faces, with
path/face order breaking ties. Discovery is best effort. Set `system_directories`
to an explicit root list for application-controlled inventory, or an empty list for
no installed-font search; unset uses the system roots. Keep environment/files stable
during loading. A snapshot no longer needs those files afterward.

With `automatic_system_fonts`, append available Arial, Segoe UI, Helvetica, Noto Sans,
DejaVu Sans and Liberation Sans families in that order. These are common preferences,
not fonts guaranteed to exist across operating systems or regions. Repeated path/face
entries are deduplicated. The terminal fallback is always an
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

`layout_text(fonts, utf8, options)` returns immutable `ShapedText`: original
[decoded scalar records](text-encoding.md), face-qualified glyph IDs/positions,
shaping-cluster byte/scalar ranges and line metrics. It retains its font collection
and copied options but no borrowed text, provider or uploaded resource. The font
size is an em size in local pixels, with Y-up baseline coordinates. It is independent
of camera/screen units and does not change the legacy STBFont/FFont metric contract.

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
replacement boxes. With a width, RTL paragraphs align to its right edge; without a
width, visual lines start at local X=0. CR/LF/CRLF and Unicode paragraph separators
start paragraphs; U+2028 forces a line within its existing paragraph.

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

Prepared glyph pages, retained submission and the demo integration remain in the
active [U11 implementation checklist](../planning/develop-review-and-development-plan.md#u11--unicode-text-layout-and-glyph-resources).
Font-selection/layout regressions and neutral header probes have pending Linux
acceptance in [TR7](../testing-requests.md#tr7-automated-engine-asset-preparation).
