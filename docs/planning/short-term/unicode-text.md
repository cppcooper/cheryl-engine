# Unicode text layout and native acceptance

The initial grayscale [Unicode scope](../../assets/text-layout.md#native-acceptance-scope)
has an accepted Linux native baseline for appearance, alignment, fallback and
local-width wrapping. Dedicated font-style/weight fixtures and regressions remain
unfinished below. The subject guide owns font, layout and resource contracts and
the rationale for a separate service. Color emoji, wider CJK acceptance and
editing/IME remain outside this task.

## Progress

- [x] Add owned UTF-8 scalar/source records with maximal-subpart replacement and
  preserved NUL/BOM/unassigned scalars.
- [x] Add decoder regressions and a neutral first-include probe.
- [x] Make STBFont consume scalars while preserving ASCII metrics and FFont's byte API.
- [x] Cover malformed/multilingual fallback and CPU submission.
- [x] Settle script, direction, wrapping, fallback and grayscale scope.
- [x] Add immutable file/family selection, inspected coverage and licensed embedded
  fallback through private FreeType implementation.
- [x] Add HarfBuzz shaping and ICU bidi/grapheme/line boundaries with owned clusters,
  face-qualified glyphs and measured accepted-line widths.
- [x] Prepare CPU grayscale pages and upload complete immutable retained generations;
  failure preserves the preceding application generation.
- [x] Integrate worker preparation/platform uploads into the builtin demo, with
  multilingual, wrapping, failure/retention checks and a visual harness.
- [x] Correct bidi-control source mapping while preserving shaping joiners/selectors.
- [x] Accept Linux Release Engine-only decoder, selection, shaping/bidi/wrapping,
  resource/submission and header coverage.
- [x] Compile the native Linux/X11/OpenGL demo with HID and both UI adapters disabled.
- [x] Rank installed faces by declared weight/style and exclude heavy automatic defaults.
- [x] Accept existing font/layout/resource checks after the font-style selection changes
  in the Linux Release Engine-only selection.
- [ ] Supply licensed style/weight fixtures and dedicated regressions for the
  automatic limit, heavy-only family skipping and unrestricted explicit preferences
  in separately authorized testing phases. Existing family cases cover baseline
  discovery/order rather than those numeric-weight boundaries.
- [x] Accept reported sequential/concurrent visual, fallback and replacement
  observations within the [native scope](../../assets/text-layout.md#native-acceptance-scope).

Possible [color emoji and measured layout/cache work](../mid-term/README.md)
follow this task without changing its acceptance scope.
