# Unfinished engine work

These items remain unresolved in the current source. Runtime and backend contracts
are documented in [runtime-architecture.md](../runtime/runtime-architecture.md).
Unresolved artwork metadata is tracked separately in
[asset-manifest-todo.md](asset-manifest-todo.md).

The source review's classification, accepted initial consumers/platforms, and
ordered work are in [the develop plan's U0](develop-review-and-development-plan.md#u0-establish-scope-and-acceptance-baselines).
That plan also tracks findings without TODO comments, including numeric-header,
native callback, exception-reporting, logging, and library-consumer work. Completed
source changes remain distinct from unexecuted acceptance checks.

## Resource and utility fixes

- Resolve FFont's legacy encoding, width bounds, atlas identity, and trailing-data
  contract from an authoritative fixture/writer. Binary input and complete-read
  rejection now precede upload; semantic validation remains at
  [the legacy format boundary](../resources/legacy-ffont.md).
- Complete the event-reporting part of shader diagnostics. Structured uniform/
  attribute reflection queries already exist; the printing APIs still write their
  results to standard output. See
  [glslprogram.h](../../include/cheryl/backends/opengl/glslprogram.h) and
  [glslprogram.cpp](../../src/backends/opengl/glslprogram.cpp).

## Gameplay and execution extensions

- Add a tile-map selection layer: derive neighbor signatures/masks, choose weighted
  candidates, then apply animated-target substitution using simulation-owned time.
  The current Tileset APIs expose metadata without selecting world neighbors. See
  [tileset.h](../../include/cheryl/assets/types/2d/tileset.h).
- Derive render compatibility keys and an explicit ordering policy before batching
  or sorting. Packet playback currently preserves authored order. See
  [draw-packet.h](../../include/cheryl/core/rendering/draw-packet.h).
- Add a profiling-based timing configurer that suggests pacing/recovery limits
  without replacing explicit timing/input policy. See
  [simulation-scheduler.h](../../include/cheryl/core/game-framework/simulation-scheduler.h).
- Add typed event channels separately from registration lifetime and delivery.
  The current bus uses string channels and `std::any` payloads. See
  [event-bus.h](../../include/cheryl/core/subsystems/event-bus.h).
- Introduce Unicode decoding and glyph-run shaping before expanding the fonts'
  ASCII range. Committed input text does not supply font shaping. See
  [stbfont.h](../../include/cheryl/assets/types/2d/stbfont.h).
