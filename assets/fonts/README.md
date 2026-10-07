# Builtin text fallback

`DejaVuSans.ttf` is the unmodified DejaVu Sans 2.33 font copied from TGUI's test
resources at `f3c3fafd9c2c8a7e17565fa052ed8cdf4c8499e1`. Its SHA-256 is
`4a2438171a9144a2c698a36a64b10116c4c9db9112cd12361296f9eb0bd8cbed`.
The accompanying [license](DejaVuSansLicense.txt) covers redistribution; include it
when distributing the font or an application containing its embedded bytes.

The Engine embeds these repository-owned bytes during configuration. Runtime font
loading does not depend on TGUI, this source path, or an installed system font.
This face covers the initial Latin/Cyrillic alphabets, combining marks and a visible
U+FFFD replacement; it does not cover every Unicode scalar. See the
[font collection contract](../../docs/assets/text-layout.md#font-selection).
