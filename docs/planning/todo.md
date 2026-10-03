# Unfinished engine work

These items remain unresolved in the current source. Runtime and backend contracts
are documented in [runtime-architecture.md](../runtime/runtime-architecture.md).
Unresolved artwork metadata is tracked separately in
[asset-manifest-todo.md](asset-manifest-todo.md).

## Resource and utility fixes

- Validate FFont widths-file opening, complete reads, and expected format before
  constructing the font. The loader still has explicit placeholders and reads a
  fixed array without checking the read result. See
  [ffont.cpp](../../src/assets/types/2d/ffont.cpp).
- Retain a safe byte-manager release context when a final backing handle can race
  manager destruction. The weak lifetime token checks liveness but does not
  serialize raw-manager access. Both
  [managed-block.hpp](../../include/cheryl/core/resources/memory/managed-block.hpp) and
  [pool.hpp](../../include/cheryl/core/resources/objects/pool.hpp) retain this TODO.
- Audit multi-container BlockManagement transitions before promising concurrent
  memory-manager use. See
  [mem-mgr.h](../../include/cheryl/core/resources/memory/mem-mgr.h).
- Define the memory statistics result when total allocation is zero before dividing
  by that total. See [mem-mgr.hpp](../../include/cheryl/core/resources/memory/mem-mgr.hpp).
- Define initialization/operation contracts for argument-bearing singletons;
  `std::call_once` only serializes construction, and competing first arguments
  currently select whichever caller wins. See
  [singleton.h](../../include/cheryl/templates/singleton.h).
- Decide/fix exact-power-of-1024 boundaries and TiB-and-larger suffix behavior in
  `human_readable`, then add the deferred boundary cases. See
  [bytes.h](../../include/cheryl/math/bytes.h) and
  [math.cpp](../../tests/executables/gtest/math/math.cpp).
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
