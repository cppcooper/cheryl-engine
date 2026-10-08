# Unicode text layout and native acceptance

The Unicode service is implemented, with initial Linux Release Engine-only
regressions/header probes and native demo compilation accepted. The subsequent
font-weight selection change still needs compilation and dedicated
regressions; Linux appearance and replacement observations remain pending.
The [Unicode service](../../assets/text-layout.md) owns the current
font, layout and resource contracts; legacy Font/STBFont/FFont and toolkit text APIs
retain their own roles.

The selected scope is English, accented Latin including French/German,
Russian/Cyrillic, automatic/explicit paragraph direction, mixed runs and optional
local-width wrapping. Color emoji, wider CJK acceptance and grapheme/bidi editing
or IME are outside this task. The separate run service pairs face-qualified glyph
placements with complete immutable page generations; the legacy Font interface's
single atlas/geometry pair cannot represent that fallback model.

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

## Remaining acceptance

Observe supported glyphs and combining marks, mixed/pure RTL with natural digit
order, width changes without split graphemes, explicit file/family and embedded
fallback selection, and retained text through rapid replacement/shutdown.
Refresh the toolkit-free demo using the queue's shared preparation, then use its
launch variants and report unavailable layouts or preferences;
automatic absence proves graceful fallback, not successful family loading.
Scalar editing does not establish a grapheme/bidi caret contract. A skipped/native
unselected run leaves appearance pending even when CPU checks pass.

Possible [color emoji and measured layout/cache work](../mid-term/README.md)
follow this task without changing its acceptance scope.
