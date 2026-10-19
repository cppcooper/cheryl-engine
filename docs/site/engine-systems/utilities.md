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

```{graphviz}
:caption: Utilities integration
:alt: Applications use Utilities for local timing, bits and parsing. Assets uses file discovery, Memory uses address and fit operations, and text or rendering preparation uses CPU geometry. Utilities relies on standard clocks, filesystem operations and containers.

digraph utilities_usage {
    rankdir=LR;
    utilities [label="Utilities", penwidth=2];
    application [label="Application tools"];
    assets [label="Assets"];
    memory [label="Memory"];
    visuals [label="Text / Rendering\nCPU preparation"];
    standard [label="Standard clocks /\nfilesystem / containers"];
    application -> utilities [label="measures local time;\nuses bits / numeric parsing"];
    assets -> utilities [label="discovers indexed files"];
    memory -> utilities [label="computes addresses / fit"];
    visuals -> utilities [label="builds CPU geometry"];
    utilities -> standard [label="uses local value operations"];
}
```

### Uses

Standard clocks, filesystem/numeric operations and containers supply local
values. Template arguments and caller-provided storage determine additional
ownership; ordinary helpers need no runtime, graphics or worker service.

| Types | Systems |
| --- | --- |
| [`std::chrono::steady_clock` / durations](../runtime/utility-contracts.md#timing-and-numeric-helpers)<br>[`std::filesystem::path`](../assets/file-and-font-discovery.md#file-index)<br>[Caller-supplied spans / pointers](../runtime/utility-contracts.md#packed-bits-addresses-and-geometry) | — |

### Used by

Applications use local bookkeeping and preparation helpers. Assets discovers
files, Memory uses address/fit operations, and text/rendering preparation uses
CPU geometry. Runtime simulation timing keeps its own policy.

| Types | Systems |
| --- | --- |
| {cpp:class}`CE::Assets::Loading::FileRegistry`<br>{cpp:struct}`CE::Memory::Manager`<br>{cpp:struct}`CE::Text::Legacy::BitmapFont`<br>[Application-owned loading tools](../assets/file-and-font-discovery.md#file-index) | [Assets](assets.md)<br>[Memory](memory.md)<br>[Text](text.md)<br>[Rendering](rendering.md) |

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
