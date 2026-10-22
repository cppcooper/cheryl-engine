# Legacy bitmap fonts

Use this reference to maintain existing bitmap/ASCII font consumers and migrate
their metrics deliberately. New multilingual or shaped text should start with the
[Unicode text service](../assets/text-layout.md); recovered bitmap artwork is a
compatibility asset, not the default text path.

`CE::Text::Legacy::BitmapFont` (formerly `FFont`) is deprecated in favor of
`CE::Text::Legacy::AsciiFont` (formerly `STBFont`) with a caller-supplied font file. Its
deprecation originally followed the loss of its metrics/atlas inputs; those inputs
have since been recovered in the assets submodule. Recovery does not reverse the
deprecation or change the loader contract.
The class, singleton setup, typed bank selection, and loading behavior remain
available to legacy consumers that supply compatible metrics and artwork. This
decision does not remove the Font resource/layout interface.

`AsciiFont::load_font(path, font_size, provider)` reads the supplied font file and bakes
printable-ASCII geometry, advances, line height, and an alpha atlas. It also retains
the provider's default textured-triangle material, so ordinary submission needs
no application shader setup. An optional material argument selects a custom
retained handle, and `DrawStyle2D::material` can override it for one submission.
System-font
discovery only helps locate a default; it does not restrict this loader. A bundled
font uses the same entry point. The
[legacy font cache guide](../assets/file-and-font-discovery.md#legacy-font-cache)
owns collection-face, default selection and loading policy. The separate
[Unicode service](../assets/text-layout.md) supports shaped text and fallback while
preserving legacy font interfaces.

The deprecated BitmapFont loader preserves its existing three-element
`BitmapFontData` contract and does not acquire a default material. Its callers
continue to select a compatible triangle material through `DrawStyle2D`. This
compatibility path is separate from automatic defaults for AsciiFont and Unicode
text; it does not change the stored bank metrics or artwork selection.

BitmapFont's normalized advances and 1/128 local newline step differ from AsciiFont's
baked advances and line height. Callers migrating their text must choose the font
size and draw scale deliberately. An alternate bank has no AsciiFont equivalent;
select a suitable separate font instead. AsciiFont currently rejects that legacy
layout option. Deprecation does not silently reinterpret existing layout calls.

## Legacy layout

| Behavior | Contract |
| --- | --- |
| Glyph selection | Printable ASCII selects glyph `letter - 32`; typed alternate-bank selection adds 128. Both banks retain their own immutable widths. |
| Pen advance | Glyph/space width is divided by 128, then multiplied by DrawStyle2D.scale during submission. Space advances without a packet. |
| Newline | Reset local x and subtract 1/128 from local y; submission applies the caller's scale and model to every line. |
| Rotation | One caller model controls all glyphs/lines. Newlines follow the model's local axes. |
| Unsupported bytes | Controls/high bytes use `?`, except newline and ignored carriage return. A tab also uses fallback in BitmapFont; AsciiFont uses a four-space advance. |
| State and lifetime | Layout does not store caller text/format. Each resolved glyph packet retains geometry, material and any supplied atlas binding. |

Synthetic recording/native fixtures check those metrics and atlas rows. They do
not establish native appearance of the recovered artwork.

## Recovered evidence and preserved legacy behavior

The recoverable loader contract is 256 host-native `short` widths converted
directly to float advances. Both the original `0329cff` loader from September 25,
2024 and the current synthetic native fixture use that representation. Files open
as binary input and must supply the entire array before geometry is allocated or
uploaded. Incomplete or failed reads are rejected; trailing bytes remain ignored.

The recovered `graphics/fonts/font.dat`, `fontblack.fdat`, `fontwhite.fdat` and
`whitefont.fdat` are identical 512-byte tables. Interpreting them as 256 little-endian
signed 16-bit values gives a range of 0–113. That is evidence about these files,
not a universal width limit or a portable format guarantee. Their SHA-256 is
`8757d19f205ee0e9464e497997a136b1e568863a17daf6b2fa429c394f1d3568`.
It also matches the earlier externally recovered
[v3 fontMetrics.dat](https://bitbucket.org/cppcooper/cheryl-upgrade/src/5c28678cb0bc425bad51ff39d04e53cc104bb50f/cheryl-v3/fontMetrics.dat).

Printable ASCII selects entries 0–94; the alternate bank selects 128–222. Geometry
uses a fixed 16-by-16 glyph grid, and loading requires a cached `whitefont.png`
image. The recovered [font inputs](../../../assets/graphics/fonts) now include that
2048×2048 atlas and related variants. `CE::Text::Legacy::Manager` does not load BitmapFont automatically:
the caller must upload/cache the required image and initialize BitmapFont explicitly.
Full asset preparation includes standalone PNGs; a focused shader-only selection
does not load this atlas.

[font-widths.json](../../../assets/graphics/fonts/font-widths.json) preserves all 256
widths, source encoding/hash, atlas grid, bank offsets and normalization without
loss. It is compatibility metadata outside the graphics index, not a supported
font manifest. BitmapFont still reads the original binary input and selects cached
`whitefont.png`, regardless of which width-table filename was passed. The recorded
same-basename texture pairs describe the recovered legacy files, not an automatic
current-loader selection. Existing fixtures remain synthetic; recovered-data
appearance and initialization have no newly recorded runtime acceptance.

Semantic width limits, explicit byte order, exact-size/trailing-data policy, and
atlas validation remain unverified legacy limitations. Reconstructing them or
inventing a replacement metrics format would require a separate compatibility
decision, as would class removal or migration of the bank option.
