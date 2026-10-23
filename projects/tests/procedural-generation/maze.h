#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// Private CPU prototype. No Engine/SDK export or serialization contract.
namespace ProcGen {
    using Index = std::uint64_t;
    inline constexpr std::size_t max_dimensions = 16;
    using Coordinate = std::array<Index, max_dimensions>;

    enum class FailureKind { invalid_input, budget_refused, unsupported, contradiction };
    enum class Reason {
        dimensions,
        extent,
        overflow,
        coordinate,
        index,
        axis,
        direction,
        bound,
        algorithm,
        version,
        cells,
        edges,
        storage,
        work,
        topology,
        cost,
        endpoint,
        heuristic,
        expansions,
        queue_entries,
        queue_pushes,
        path,
        traversability,
        coverage,
        endpoint_policy
    };

    struct Failure {
        FailureKind kind;
        Reason reason;
        bool operator==(const Failure&) const = default;
    };

    template <class T> using Result = std::expected<T, Failure>;

    [[nodiscard]] std::string_view reason_text(Reason reason);

    struct Budgets {
        Index cells;
        Index edges;
        Index storage_bytes;
        Index work;
        bool operator==(const Budgets&) const = default;
    };

    inline constexpr Budgets hard_limits{1'000'000, 999'999, 16 * 1024 * 1024, 100'000'000};
    inline constexpr Index fixed_storage_allowance = 4096;

    struct StreamKey {
        std::uint64_t seed;
        std::uint64_t stage;
        std::uint64_t stream;
        bool operator==(const StreamKey&) const = default;
    };

    // All three fields are numeric u64 words. Canonical encoding: LE64(seed),
    // LE64(stage), LE64(stream), in that order; no strings, padding or std::hash.
    [[nodiscard]] std::uint64_t derive_stream_state(StreamKey key);

    class IntegerStream {
        std::uint64_t state_;

    public:
        explicit IntegerStream(std::uint64_t state)
        : state_(state) {}
        [[nodiscard]] static IntegerStream for_stage(StreamKey key);
        [[nodiscard]] std::uint64_t next();
    };

    // Work is a deterministic operation count, not time or machine instructions.
    class Work {
        Index limit_;
        Index used_ = 0;

    public:
        explicit Work(Index limit)
        : limit_(limit) {}
        [[nodiscard]] bool consume(Index units);
        [[nodiscard]] Index used() const { return used_; }
    };

    // One work unit for EVERY raw draw, including bound==1 and rejected draws.
    // On refusal the stream/work may have advanced; there is no sampled value.
    [[nodiscard]] Result<std::uint64_t> sample_below(IntegerStream& stream, std::uint64_t bound, Work& work);

    enum class Direction : std::uint32_t { negative, positive };

    // Axis 0 is fastest. stride[a] = product(extents[0..a)). All inactive
    // coordinate entries are zero; index() takes exactly dimensions() entries.
    // Validated domains allocate nothing and may exceed generation admission caps.
    class Domain {
        Coordinate extents_{};
        Coordinate strides_{};
        std::size_t dimensions_ = 0;
        Index cells_ = 1;

        Domain() = default;

    public:
        [[nodiscard]] static Result<Domain> checked(std::span<const Index> extents);
        [[nodiscard]] std::span<const Index> extents() const { return {extents_.data(), dimensions_}; }
        [[nodiscard]] std::span<const Index> strides() const { return {strides_.data(), dimensions_}; }
        [[nodiscard]] std::size_t dimensions() const { return dimensions_; }
        [[nodiscard]] Index cells() const { return cells_; }
        [[nodiscard]] Result<Coordinate> coordinates(Index index) const;
        [[nodiscard]] Result<Index> index(std::span<const Index> coordinates) const;
        [[nodiscard]] Result<std::optional<Index>> neighbor(Index index, std::size_t axis, Direction direction) const;
        bool operator==(const Domain&) const = default;
    };

    struct Requirements {
        Index cells;
        Index edges;
        Index storage_bytes;
        Index minimum_work;
    };

    // Accounted peak: 4*C output masks + 8*C traversal stack + 4096 fixed bytes.
    // Excludes allocator/runtime overhead and existing caller-owned results; not RSS.
    // Minimum work: 2*C + (2*C-1)*(2*D+1) + (C-1); rejection draws add work.
    [[nodiscard]] Result<Requirements> requirements(const Domain& domain);

    enum class MazeAlgorithm : std::uint32_t { depth_first = 1 };

    struct Replay {
        Domain domain;
        StreamKey random;
        Budgets budgets;
        MazeAlgorithm algorithm = MazeAlgorithm::depth_first;
        std::uint32_t version = 1;
        bool operator==(const Replay&) const = default;
    };

    struct Usage {
        Index work;
        Index random_draws;
    };

    // Complete, independently owned value. Copies deep-copy the mask buffer.
    // A moved-from value supports destruction/assignment only. Returned const
    // views borrow this value and expire on its destruction, move or assignment.
    class Maze {
        Replay replay_;
        std::vector<std::uint32_t> passages_;
        Usage usage_;

        Maze(Replay replay, std::vector<std::uint32_t> passages, Usage usage);
        friend Result<Maze> replay(const Replay& descriptor);

    public:
        [[nodiscard]] const Replay& descriptor() const { return replay_; }
        [[nodiscard]] const Domain& domain() const { return replay_.domain; }
        // bit 2*a is negative axis a; bit 2*a+1 is positive. No other bits set.
        [[nodiscard]] std::span<const std::uint32_t> passages() const { return passages_; }
        [[nodiscard]] Usage usage() const { return usage_; }
        [[nodiscard]] Result<std::optional<Index>> neighbor(Index index, std::size_t axis, Direction direction) const;
    };

    // Replay v1 fixes stream derivation, SplitMix64, sampling and traversal order.
    // Budgets affect admission only; sufficient budgets reproduce logical masks.
    // Expected failures publish no partial Maze. Allocation/programming exceptions
    // propagate. Calls share no state and never mutate an existing Maze.
    [[nodiscard]] Result<Maze> replay(const Replay& descriptor);
    [[nodiscard]] Result<Maze> generate(std::span<const Index> extents, StreamKey random, Budgets budgets);
}
