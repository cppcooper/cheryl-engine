# FFont deprecation and font-file migration

FFont is deprecated in favor of STBFont with a caller-supplied font file. The
project owner confirms the original atlas is unavailable and chooses deprecation.
The class, singleton setup, typed bank selection, and loading behavior remain
available to legacy consumers that supply compatible metrics and artwork. This
decision does not remove the Font resource/layout interface.

STBFont::load_font(path, font_size, provider) reads the supplied font file and bakes
printable-ASCII geometry, advances, line height, and an alpha atlas. System-font
discovery only helps locate a default; it does not restrict this loader. A bundled
font uses the same entry point. FontMgr::load_assets accepts supplied paths too,
currently loading them at size 32 and selecting the first published path as its
default. Collection loading selects face zero. The separate
[Unicode service](../assets/text-layout.md) supports shaped text and fallback while
preserving legacy font interfaces.

FFont's normalized advances and 1/128 local newline step differ from STBFont's
baked advances and line height. Callers migrating their text must choose the font
size and draw scale deliberately. An alternate bank has no STBFont equivalent;
select a suitable separate font instead. STBFont currently rejects that legacy
layout option. Deprecation does not silently reinterpret existing layout calls.

## Legacy layout

| Behavior | Contract |
| --- | --- |
| Glyph selection | Printable ASCII selects glyph `letter - 32`; typed alternate-bank selection adds 128. Both banks retain their own immutable widths. |
| Pen advance | Glyph/space width is divided by 128, then multiplied by DrawStyle2D.scale during submission. Space advances without a packet. |
| Newline | Reset local x and subtract 1/128 from local y; submission applies the caller's scale and model to every line. |
| Rotation | One caller model controls all glyphs/lines. Newlines follow the model's local axes. |
| Unsupported bytes | Controls/high bytes use `?`, except newline and ignored carriage return. A tab also uses fallback in FFont; STBFont uses a four-space advance. |
| State and lifetime | Layout does not store caller text/format. Each resolved glyph packet retains geometry, material and any supplied atlas binding. |

Synthetic recording/native fixtures check those metrics and atlas rows; they do
not recover the original artwork.

## Recovered evidence and preserved legacy behavior

The recoverable loader contract is 256 host-native `short` widths converted
directly to float advances. Both the original `0329cff` loader from September 25,
2024 and the current synthetic native fixture use that representation. Files open
as binary input and must supply the entire array before geometry is allocated or
uploaded. Incomplete or failed reads are rejected; trailing bytes remain ignored.

The externally recovered
[fontMetrics.dat](https://bitbucket.org/cppcooper/cheryl-upgrade/src/5c28678cb0bc425bad51ff39d04e53cc104bb50f/cheryl-v3/fontMetrics.dat)
is 512 bytes. Interpreting it as 256 little-endian signed 16-bit values gives a
range of 0–113. That is evidence about this fixture, not a universal width limit
or a portable format guarantee. Its SHA-256 is
`8757d19f205ee0e9464e497997a136b1e568863a17daf6b2fa429c394f1d3568`.
The file is retained externally; it is not added as a runtime dependency.

Printable ASCII selects entries 0–94; the alternate bank selects 128–222. Geometry
uses a fixed 16-by-16 glyph grid, and loading requires a cached `whitefont.png`
image. The recovered widths cannot provide its missing artwork. Existing tests
supply synthetic widths and atlases; they establish behavior for those fixtures,
not recovery of the original font.

Semantic width limits, explicit byte order, exact-size/trailing-data policy, and
atlas validation remain unverified legacy limitations. Reconstructing them or
inventing a replacement metrics format would require a separate compatibility
decision, as would class removal or migration of the bank option.
