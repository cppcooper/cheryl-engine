# Utilities

## Purpose

Time, math, numeric parsing and file helpers supply local value operations.
Their namespaces describe responsibility; they do not imply a shared service,
singleton manager or engine-wide initialization/shutdown phase.

## Interface

Types below are in `CE` unless another namespace is shown.

### Types and operations

| API and purpose |
| --- |
| {cpp:class}`Time::DeltaTime <CE::Time::DeltaTime>` and duration helpers<br>Measure local wall-clock intervals and convert duration units. [Timing helpers](../runtime/utility-contracts.md#timing-and-numeric-helpers). |
| {cpp:class}`Math::BitArray <CE::Math::BitArray>`<br>Own packed bits with borrowed proxies/native word spans. [Packed bit representation](../runtime/utility-contracts.md#packed-bits-addresses-and-geometry). |
| `CE::Text::Numbers` parsing functions<br>Parse floating/signed/unsigned numeric text with explicit syntax and range failures. [Numeric parsing](../runtime/utility-contracts.md#timing-and-numeric-helpers); [declarations](../../../projects/engine/include/cheryl/math/string-numbers.h). |
| {cpp:class}`Files::FileIndex <CE::Files::FileIndex>`<br>Own normalized file-discovery results without automatic refresh. [File index](../assets/file-and-font-discovery.md#file-index). |
| `CE::Memory::Pointers` and fit/byte helpers<br>Calculate addresses/alignment, growth and byte formatting without owning storage. [Address preconditions](../runtime/utility-contracts.md#packed-bits-addresses-and-geometry); [fit arithmetic](../runtime/utility-contracts.md#timing-and-numeric-helpers). |

### Values and selection rules

| Choice | Purpose and details |
| --- | --- |
| Duration aliases / casts | Standard duration truncation and representability rules; no scheduling policy. [Time units](../runtime/utility-contracts.md#timing-and-numeric-helpers). |
| {cpp:enum}`Memory::FitPolicy <CE::Memory::FitPolicy>` | `exact`, `larger`, `greedy` choose length adjustment independently of managed allocation. [Growth rules](../runtime/utility-contracts.md#timing-and-numeric-helpers). |
| {cpp:enum}`Rendering::Geometry::AnchorType <CE::Rendering::Geometry::AnchorType>`, {cpp:struct}`Rendering::Geometry::Pivot <CE::Rendering::Geometry::Pivot>` | Choose named CPU quad placement or a normalized pivot; texture dimensions have explicit preconditions. [Anchor geometry](../runtime/utility-contracts.md#packed-bits-addresses-and-geometry); [coordinate conventions](../assets/asset-values-and-playback.md#coordinates-and-grid-geometry). |
| Local timer / runtime delta | Measure CPU work with `DeltaTime`; advance game simulation with the tick's selected delta. [Local timing](../runtime/utility-contracts.md#timing-and-numeric-helpers); [simulation time](../runtime/simulation-timing.md#input-and-tick-time). |

## Usage

Pick a helper for the local value operation you need. A loading tool can use
file discovery and elapsed-time measurement without creating a runtime or a
graphics context; a game update still uses the runtime's selected simulation
delta for gameplay progression.

```{engine-diagram} utilities-application
```

The [engine consumer view](diagram-explorer.md#utilities-internal) shows
where these helpers participate in loading, allocation and CPU geometry.

### Choose the operation

| Task | Start with | Boundary to keep explicit |
| --- | --- | --- |
| Measure a CPU preparation interval | {cpp:class}`Time::DeltaTime <CE::Time::DeltaTime>` | `operator()` returns seconds and starts the next interval; `elapsed()` only observes. This is independent of `tick.delta_seconds`. |
| Index files by extension | {cpp:class}`Files::FileIndex <CE::Files::FileIndex>` | Own the index, serialize access and construct a fresh one to rescan visited directories. Results borrow the index's containers. |
| Parse a known numeric syntax | `Text::Numbers::parse_floats()` or `parse_integers()` | Floating and decimal integer syntax differ. Use an explicit parser when format is known; the compatibility selector chooses floats by `.`, `e` or `E`. |
| Keep compact flags | {cpp:class}`Math::BitArray <CE::Math::BitArray>` | Valid indices and synchronization are caller responsibilities, including different bits sharing a word. Proxies/spans borrow the array. |
| Calculate capacity/alignment | `Memory::adjust_length()`, {cpp:enum}`Memory::FitPolicy <CE::Memory::FitPolicy>` and pointer helpers | Results describe arithmetic; they do not allocate storage or establish a live object's bounds/lifetime. |
| Build CPU quad geometry | {cpp:struct}`Rendering::Geometry::Pivot <CE::Rendering::Geometry::Pivot>` and `Rendering::Geometry::Anchor` operations | Supply valid normalized placement and nonzero texture dimensions. Resource upload/rendering is a later owner-bound operation. |

The [utility contract guide](../runtime/utility-contracts.md) owns syntax,
range, borrowing and failure rules for these choices.

### Uses

| Helper | Input/dependency | What it retains or returns |
| --- | --- | --- |
| `Time::DeltaTime<Clock>` | Selected clock; steady by default | Retains its last sample and returns local elapsed values; no scheduler or runtime service. |
| `Files::FileIndex` | Filesystem root | Owns discovery results; no automatic freshness or worker scheduling. |
| Numeric parsers and capacity helpers | Borrowed text or supplied numeric values/policy | Return parsed/adjusted values; no service lifetime is created. |
| `Math::BitArray` and address/geometry helpers | Owned bit words or caller-provided values/storage | Borrowed proxies/ranges stay within their owner; address helpers never own the pointees. |

These helpers use standard clocks, filesystem and value machinery internally.
Their engine-facing dependencies are local values and caller-owned storage,
rather than a common Utilities manager.

### Used by

| Consumer | Utility | Relationship |
| --- | --- | --- |
| {cpp:struct}`Assets::Loading::Loader <CE::Assets::Loading::Loader>` | `Files::FileIndex` | Automatic preparation creates a fresh discovery index before preparing file/manifest metadata. `Assets::Loading::FileRegistry` retains registered paths; it does not own the index. [File discovery](../assets/file-and-font-discovery.md#file-index). |
| {cpp:struct}`Memory::Manager <CE::Memory::Manager>` | `Memory::adjust_length()` and pointer/alignment helpers | Calculates growth and valid aligned request ranges before storage transactions. [Memory](memory.md). |
| `Assets::Grids::make_geometry()` and {cpp:struct}`Assets::Graphics::Asset <CE::Assets::Graphics::Asset>` | `Rendering::Geometry::Anchor` / `Rendering::Geometry::Pivot` | Builds CPU vertices from source rectangles and placement. [Asset coordinates](../assets/asset-values-and-playback.md#coordinates-and-grid-geometry). |
| {cpp:struct}`Text::Legacy::BitmapFont <CE::Text::Legacy::BitmapFont>` preparation | `Rendering::Geometry::Anchor` | Builds the fixed bitmap font's CPU glyph quads before upload. [Text](text.md). |
| Application tools/components | Timers, parsers, indices and packed bits | Own local preparation/bookkeeping; choose when blocking discovery runs and how results reach another owner. |

### Keep timing and work ownership separate

Use `Time::DeltaTime` to observe how long preparation took. Use
`tick.delta_seconds` to advance runtime simulation; the runtime may select a
fixed step, clamp elapsed time or discard accumulated lag according to its
[timing policy](../runtime/simulation-timing.md).

Filesystem discovery can block. If the application moves it to
[Workers](workers.md), the job owns its inputs/results and transfers a completed
result to the appropriate consumer. A helper's local nature does not make its
borrowed results safe to share or extend them past owner destruction.

## Contracts

Duration conversions follow standard truncation/representability rules. Pointer
helpers do not allocate or prove storage bounds/object lifetime; supply valid
alignment and representable addresses. Packed bit access requires valid indices
and synchronization, even for different bits sharing a word. Native word layout
is not a portable serialized format.

Numeric parsing has distinct floating/integer syntax and failure rules. File
index references borrow its container storage and do not refresh automatically.
Use each helper's actual contract rather than assuming a universal checked-value
or service-lifecycle guarantee.

## Lifecycle

Construct timers, indices and bit arrays as application-owned values. Keep their
borrowed references/spans within the owner's lifetime and invalidation rules.
Caller-supplied pointer ranges and pointees remain the caller's responsibility.
There is no runtime shutdown hook for a stateless conversion function.

## Execution

These local values generally require caller synchronization. A custom timer
clock defines backward-jump behavior; the default steady clock is monotonic.
Filesystem discovery may block and should run at a loading boundary or on an
appropriate worker, with owned results transferred to consumers.

## Integration

A standalone loading tool can measure one CPU preparation interval:

```cpp
CE::Time::DeltaTime timer;
auto prepared = loader.prepare();
const double seconds = timer();
```

The returned `prepared` value owns its CPU input; the loader retains its root
configuration and any previously published metadata. The timer owns only its last clock sample.
In a game update, use `tick.delta_seconds` for runtime-selected simulation time
instead of substituting this independent wall-clock measurement.

## Limits and further reading

Helpers do not provide scheduling, automatic thread safety, portable pointer
identity or transactional behavior for arbitrary template types. Read the
[complete state/time/math/file contracts](../runtime/utility-contracts.md),
[file discovery guide](../assets/file-and-font-discovery.md) and
[DeltaTime header](../../../projects/engine/include/cheryl/templates/delta.h).
