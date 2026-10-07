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

Shaping, bidi, wrapping, prepared glyph pages and retained submission remain in the
active [U11 implementation checklist](../planning/develop-review-and-development-plan.md#u11--unicode-text-layout-and-glyph-resources).
Font-selection regressions and the neutral header probe have pending Linux acceptance
in [TR7](../testing-requests.md#tr7-automated-engine-asset-preparation).
