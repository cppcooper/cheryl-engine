# World integration decisions

This independent branch combines the exact committed feature heads listed in [sources.json](integration/sources.json). Complete source decisions and contracts remain feature-specific:

- [tilemap](features/tilemap/decisions.md), [detailed contracts](features/tilemap/agent-notes.md), [handoff/evidence](features/tilemap/next.md), [provenance](features/tilemap/provenance.json).
- [map-storage](features/map-storage/decisions.md), [detailed contracts](features/map-storage/agent-notes.md), [handoff/evidence](features/map-storage/next.md), [provenance](features/map-storage/provenance.json).

## WINT-001 — Preserve feature history and records

Merge the completed tilemap and map-storage histories before procedural generation, preserving both parents. Keep complete conflicting feature records under feature-specific directories; rewrite only relative Markdown links and retain source/blob/content hashes. This prevents one feature's records from replacing another's. The cost is explicit archived entry points. Confidence is high in the history/content checks; combined native acceptance remains pending.

## WINT-002 — Prove the application mapping privately

Keep storage string keys, semantic TerrainLabel values, runtime TileId values and atlas indices distinct. A private consumer will map them explicitly, link the real shared Engine for tile/storage, and use the private procedural producer without exporting a new public API. This adds adapter code while preserving each feature's boundary. Confidence is high in the separation; the combined caller is unverified.
