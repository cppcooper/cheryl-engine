# Mid-term work

Color emoji follows initial Unicode acceptance. Optimization requires representative
workload evidence and settled render/text contracts. Neither has an active checklist
yet; promote one coherent unit when its prerequisites and scope are settled.

## Color emoji

Select required color-font formats, fonts and emoji sequences beyond the accepted
initial [grayscale scope](../../assets/text-layout.md#native-acceptance-scope).
Establish RGBA page orientation,
material/alpha handling and sequence/fallback policy while preserving source clusters
and immutable generations. Keep grayscale text independently usable. Color glyphs
and sequence appearance need their own CPU/native acceptance.

## Measured optimization

Collect representative update, draw/state-switch, publication and backlog metrics.
Settle UI clipping and glyph-page semantics before caching render compatibility keys;
keys include retained resource generations, parameters/image units, geometry
ranges/topology and pipeline state. Batch compatible contiguous packets first.
Sorting requires explicit reorder-safe regions; authored order remains the default.
Comparative measurements and visual/order/generation checks must justify each change;
defer batching if another bottleneck dominates.

Measure candidate-prefix shaping in constrained paragraphs and message-specific
glyph preparation/upload before adding reusable glyph caches or faster fitting.
Preserve measured accepted-line widths, source clusters and bidi behavior.

A timing adviser explains suggestions based on measured workloads and leaves fixed
steps, recovery and input retention under application control. It must not silently
change simulation or interpolation policy. Renderer, text and timing optimizations
are independent development units.
