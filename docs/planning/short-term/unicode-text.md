# Unicode text layout and native acceptance

Complete the initial grayscale [Unicode scope](../../assets/text-layout.md),
including Latin/Cyrillic, paragraph direction, fallback and local-width wrapping.
The subject guide owns the font, layout and resource contracts and the rationale
for a separate service. Color emoji, wider CJK acceptance and editing/IME remain
outside this task.

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
- [ ] Repeat existing font/layout/resource checks at the current source through
  [TR13](../../testing-requests.md#tr13-automated-startup-compilation-and-existing-regressions).
- [ ] Supply licensed style/weight fixtures and dedicated regressions for the
  automatic limit, heavy-only family skipping and unrestricted explicit preferences
  in separately authorized testing phases. Existing family cases cover baseline
  discovery/order rather than those numeric-weight boundaries.
- [ ] Accept sequential/concurrent visual, fallback and replacement observations
  through [TR9](../../testing-requests.md#tr9-qa-unicode-text-rendering).

Possible [color emoji and measured layout/cache work](../mid-term/README.md)
follow this task without changing its acceptance scope.
