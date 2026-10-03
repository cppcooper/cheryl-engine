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
default. Collection loading currently selects face zero. Unicode shaping remains
separate work in the develop plan's U11.

FFont's normalized advances and 1/128 local newline step differ from STBFont's
baked advances and line height. Callers migrating their text must choose the font
size and draw scale deliberately. An alternate bank has no STBFont equivalent;
select a suitable separate font instead. STBFont currently rejects that legacy
layout option. Deprecation does not silently reinterpret existing layout calls.

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
inventing a replacement metrics format is no longer required U3 work. Existing
complete-read regression sources remain relevant; their recent changes have not
been compiled or run. Future class removal or migration of the bank option
requires a separate scope decision.
