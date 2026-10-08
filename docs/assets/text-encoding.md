# Text encoding and source indices

`CE::Text::decode_utf8_scalar(text, offset)` reads one scalar from a complete UTF-8
view. `decode_utf8(text)` returns owned `Utf8Scalar` records for the entire view.
Each record stores the scalar, original byte offset/count and a `replaced` flag;
destroying or changing the source after the call does not change those records.
The single-scalar operation allocates nothing and returns no value at or beyond
the view's end. Keep input stable during either call. Vector allocation failure
propagates from complete-view decoding.

The decoder accepts the well-formed ranges in
[Unicode 16.0, section 3.9](https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-3/)
and uses the documented substitution of maximal subparts for ill-formed input.
It emits U+FFFD for the longest prefix that could begin a well-formed sequence,
or one byte when no such prefix exists. A rejected successor remains available to
the next decode. Overlong encodings, surrogate encodings, values above U+10FFFF and
orphan continuation bytes are never interpreted as valid scalars.

For example, `E1 80 41` produces a replacement covering bytes 0–1, followed by `A`
at byte 2. `ED A0 80` produces three replacements because none of its bytes can
continue a valid prefix after the restricted second-byte range fails. An explicitly
encoded U+FFFD has `replaced == false`; malformed-input substitutions have it set.
Consumers can inspect that flag to reject malformed text before using the result.

Truncated input is handled at the supplied view's end. This API has no streaming
state; a caller receiving separate byte chunks must retain an incomplete suffix
until the complete text is available. Starting at a continuation byte produces a
replacement for that byte rather than searching backward for a prior scalar.

Embedded NUL, BOM, unassigned values and noncharacters remain scalar values. The
decoder performs no BOM stripping, normalization or segmentation. Byte offsets
address the original view; scalar indices address record order. Combining marks,
ligatures and emoji sequences require separate grapheme/shaping-cluster mappings,
and glyph indices are not interchangeable with either source index.

`STBFont::layout()` and `for_each_glyph()` use the scalar decoder without changing
the baked printable-ASCII atlas. Each unsupported scalar or malformed subpart draws
one `?`, so a valid two-, three- or four-byte character no longer expands into one
fallback per byte. Printable ASCII and newline/CR/tab handling retain their previous
metrics. Embedded NUL remains an unsupported scalar rather than ending the view.
FFont retains its deprecated byte-oriented contract. See
[font submission](asset-values-and-playback.md#fonts-and-cpu-submission).

The separate [Unicode layout API](text-layout.md) supplies owned font selection,
shaping, bidi, source-cluster ranges and optional wrapping for the selected initial
scope, including immutable retained glyph generations. The legacy ASCII atlas
does not establish multilingual glyph
coverage, combining-mark placement, bidi order or grapheme-aware editing. Reusable
[asset/text checks](../development/architecture-validation.md#engine-asset-and-text-regressions)
remain separate from native rendering and editing acceptance.
