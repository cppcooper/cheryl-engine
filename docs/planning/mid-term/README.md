# Mid-term work

Color emoji follows the accepted initial grayscale Unicode baseline. Sorting and
text/timing optimization require representative workload evidence and settled
contracts. These tasks have no active checklist yet; promote one coherent unit when
its prerequisites and scope are settled. Contiguous render batching is now
[near-term work](../develop-review-and-development-plan.md#render-batching).

## Color emoji

Select required color-font formats, fonts and emoji sequences beyond the accepted
initial [grayscale scope](../../assets/text-layout.md#native-acceptance-scope).
Establish RGBA page orientation,
material/alpha handling and sequence/fallback policy while preserving source clusters
and immutable generations. Keep grayscale text independently usable. Color glyphs
and sequence appearance need their own CPU/native acceptance.

## Reorder-safe render sorting

Sorting remains separate from contiguous batching. Identify explicitly reorder-safe
regions and preserve required authored order, clipping and retained generations.
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
