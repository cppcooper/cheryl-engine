# Legacy FFont input boundary

The loader's recoverable historical contract is 256 host-native `short` widths,
converted directly to float advances. Both the original `0329cff` loader from
September 25, 2024 and the current synthetic native fixture use that representation.
Files open as binary input and must supply the entire array before geometry is
allocated or uploaded. Incomplete or failed reads are rejected. Trailing bytes
retain the historical ignored behavior pending a format decision.

Printable ASCII selects entries 0–94; the alternate bank selects 128–222. Geometry
uses a fixed 16-by-16 glyph grid. The texture is the cached `whitefont.png` image.
Those facts establish current indexing and cache identity, but do not identify
the intended artwork or the metadata writer.

The tracked tree and available path history contain no original widths fixture,
format specification, or writer. The generated acceptance fixture uses widths
128 and 64; it cannot establish a universal width limit. A width range, explicit
byte order, fixed portable integer encoding, exact-size/trailing-data rule, and
atlas validation therefore remain unresolved. Choosing them now could reject or
reinterpret existing assets.

Supply an authoritative legacy fixture/writer or choose a versioned replacement
format before completing semantic validation. Until then, preserve the encoding
and atlas lookup, reject unreadable/truncated data, and avoid claiming that every
full-size file has been validated as the intended font. Malformed-read source
cases assert zero provider uploads; the existing native fixture covers the current
successful representation when executable acceptance is authorized.
