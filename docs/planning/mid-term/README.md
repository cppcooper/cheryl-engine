# Mid-term work

Color emoji and reorder-safe sorting are planned in the
[selected roadmap](../develop-review-and-development-plan.md#selected-planned-work).
General render batching and text/timing optimization remain deferred. Sorting and
optimization retain their representative-workload and settled-contract prerequisites.
These tasks have no active implementation checklist yet; activate one coherent unit
after its investigation/design checkpoint. Opt-in
[positioned text runs](../../assets/text-layout.md#positioned-text-runs) already
prepare glyph placements once and preserve contiguous page order.

## Color emoji

This is selected planned work beyond the current grayscale implementation.

Select required color-font formats, fonts and emoji sequences beyond the accepted
initial [grayscale scope](../../assets/text-layout.md#native-acceptance-scope).
Establish RGBA page orientation,
material/alpha handling and sequence/fallback policy while preserving source clusters
and immutable generations. Keep grayscale text independently usable. Color glyphs
and sequence appearance need their own CPU/native acceptance.

## General render batching

General contiguous packet merging remains deferred and distinct from implemented
positioned-text submission. The current renderer submits each general packet
individually; the selected text path emits one draw per contiguous atlas-page run.
Require representative update, draw/state-switch, publication and backlog evidence
and a design discussion before selecting another implementation. Compatibility
must include UI clipping, glyph pages, immutable sampling/resource generations,
material/pipeline state, parameters/image units and geometry ranges/topology.
Preserve authored order and in-flight lifetime; batching does not authorize sorting.
Comparative CPU/native order/generation checks must justify the selected change.
If another bottleneck dominates, report it before accumulating dependent work.

## Reorder-safe render sorting

Sorting is selected planned work and remains separate from contiguous batching.
Identify explicitly reorder-safe regions and preserve required authored order,
clipping and retained generations.
Use workload measurements and visual/order/generation acceptance to justify
reordering; batching alone does not authorize it.

## Measured optimization

Collect representative update, shaping/upload, publication and backlog metrics
before selecting text or timing optimizations.

Measure candidate-prefix shaping in constrained paragraphs and message-specific
glyph preparation/upload before adding reusable glyph caches or faster fitting.
Preserve measured accepted-line widths, source clusters and bidi behavior.

A timing adviser explains suggestions based on measured workloads and leaves fixed
steps, recovery and input retention under application control. It must not silently
change simulation or interpolation policy. Sorting, text and timing optimizations
are independent development units.
