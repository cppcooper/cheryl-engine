#include "maze.h"

#include <algorithm>
#include <climits>
#include <limits>
#include <utility>

namespace ProcGen {
    static_assert(CHAR_BIT == 8 && sizeof(Index) == 8 && sizeof(std::uint32_t) == 4);
    static_assert(std::numeric_limits<std::size_t>::digits >= 64, "U1 requires 64-bit container sizes");

    namespace {
        constexpr Failure invalid(Reason reason) {
            return {FailureKind::invalid_input, reason};
        }
        constexpr Failure refused(Reason reason) {
            return {FailureKind::budget_refused, reason};
        }

        bool multiply(Index left, Index right, Index& result) {
            if (right != 0 && left > std::numeric_limits<Index>::max() / right) {
                return false;
            }
            result = left * right;
            return true;
        }

        bool add(Index left, Index right, Index& result) {
            if (left > std::numeric_limits<Index>::max() - right) {
                return false;
            }
            result = left + right;
            return true;
        }

        struct Candidate {
            Index cell;
            std::uint32_t direction;
        };

        // Conservative fixed allowance covers candidates, metadata, return-value
        // construction and bounded helper locals. Dynamic buffers are separate.
        static_assert(
            2 * sizeof(Maze) + 2 * sizeof(Replay) + sizeof(Domain) + sizeof(std::array<Candidate, 2 * max_dimensions>) +
                sizeof(std::vector<Index>) + sizeof(IntegerStream) + sizeof(Work) + 256 <=
            fixed_storage_allowance
        );
    }

    std::string_view reason_text(Reason reason) {
        switch (reason) {
            case Reason::dimensions:
                return "dimension count must be in [1,16]";
            case Reason::extent:
                return "extents must be positive";
            case Reason::overflow:
                return "domain/index/storage/work arithmetic or container capacity overflow";
            case Reason::coordinate:
                return "coordinate count or value outside domain";
            case Reason::index:
                return "index outside domain";
            case Reason::axis:
                return "axis outside domain";
            case Reason::direction:
                return "unsupported direction";
            case Reason::bound:
                return "random bound must be positive";
            case Reason::algorithm:
                return "unsupported maze algorithm";
            case Reason::version:
                return "unsupported maze replay version";
            case Reason::cells:
                return "cell budget refused";
            case Reason::edges:
                return "edge budget refused";
            case Reason::storage:
                return "accounted peak storage budget refused";
            case Reason::work:
                return "work budget refused or exhausted";
            case Reason::topology:
                return "invalid passage topology or mask count";
            case Reason::cost:
                return "invalid positive-axis cost data";
            case Reason::endpoint:
                return "entrance or goal outside active domain";
            case Reason::heuristic:
                return "unsupported built-in heuristic";
            case Reason::expansions:
                return "settled expansion budget exhausted";
            case Reason::queue_entries:
                return "live queue entry budget refused or exhausted";
            case Reason::queue_pushes:
                return "total queue push budget refused or exhausted";
            case Reason::path:
                return "chosen path must be simple and join the selected endpoints";
            case Reason::traversability:
                return "traversability must be finite and in [0,1]";
            case Reason::coverage:
                return "only full rectangular coverage is supported";
            case Reason::endpoint_policy:
                return "fixed endpoints contradict the route contract or are unsupported by this construction";
            case Reason::cave_dimensions:
                return "caves require exactly two axes: width, height";
            case Reason::cave_fill:
                return "initial solid fill must be an integer in [0,10000]";
            case Reason::cave_steps:
                return "cave smoothing steps must be in [0,64]";
            case Reason::cave_rules:
                return "cave rule masks may contain only neighbor-count bits 0 through 8";
            case Reason::cave_connectivity:
                return "unsupported cave connectivity policy";
            case Reason::cave_algorithm:
                return "unsupported cave algorithm";
            case Reason::cave_version:
                return "unsupported cave replay version";
            case Reason::clearance_dimensions:
                return "clearance requires exactly two axes: width, height";
            case Reason::occupancy:
                return "occupancy requires exactly one binary floor0/solid1 byte per cell";
            case Reason::clearance_threshold:
                return "minimum cardinal distance must be a nonnegative signed 64-bit integer";
            case Reason::tunnel_dimensions:
                return "cave tunnels require exactly two axes: width, height";
            case Reason::tunnel_boundary:
                return "preserving a boundary floor contradicts a solid outer boundary";
            case Reason::tunnel_version:
                return "unsupported cave tunnel transformation version";
            case Reason::dungeon_dimensions:
                return "dungeons require exactly two axes: width, height";
            case Reason::dungeon_options:
                return "dungeon room counts/attempts exceed caps, size ranges are invalid, or margin is zero";
            case Reason::dungeon_fit:
                return "minimum room cannot fit inside the requested domain/margin";
            case Reason::dungeon_algorithm:
                return "unsupported dungeon algorithm";
            case Reason::dungeon_version:
                return "unsupported dungeon replay version";
        }
        return "unknown failure reason";
    }

    Result<Domain> Domain::checked(std::span<const Index> extents) {
        if (extents.empty() || extents.size() > max_dimensions) {
            return std::unexpected(invalid(Reason::dimensions));
        }
        if (std::ranges::find(extents, Index{0}) != extents.end()) {
            return std::unexpected(invalid(Reason::extent));
        }
        Domain domain;
        domain.dimensions_ = extents.size();
        for (std::size_t axis = 0; axis < extents.size(); ++axis) {
            domain.extents_[axis] = extents[axis];
            domain.strides_[axis] = domain.cells_;
            if (!multiply(domain.cells_, extents[axis], domain.cells_)) {
                return std::unexpected(invalid(Reason::overflow));
            }
        }
        return domain;
    }

    Result<Coordinate> Domain::coordinates(Index index) const {
        if (index >= cells_) {
            return std::unexpected(invalid(Reason::index));
        }
        Coordinate coordinate{};
        for (std::size_t axis = 0; axis < dimensions_; ++axis) {
            coordinate[axis] = (index / strides_[axis]) % extents_[axis];
        }
        return coordinate;
    }

    Result<Index> Domain::index(std::span<const Index> coordinate) const {
        if (coordinate.size() != dimensions_) {
            return std::unexpected(invalid(Reason::coordinate));
        }
        Index index = 0;
        for (std::size_t axis = 0; axis < dimensions_; ++axis) {
            if (coordinate[axis] >= extents_[axis]) {
                return std::unexpected(invalid(Reason::coordinate));
            }
            // Validated product and bounded coordinates imply sum < cells_.
            index += coordinate[axis] * strides_[axis];
        }
        return index;
    }

    Result<std::optional<Index>> Domain::neighbor(Index index, std::size_t axis, Direction direction) const {
        if (index >= cells_) {
            return std::unexpected(invalid(Reason::index));
        }
        if (axis >= dimensions_) {
            return std::unexpected(invalid(Reason::axis));
        }
        if (direction != Direction::negative && direction != Direction::positive) {
            return std::unexpected(invalid(Reason::direction));
        }
        const auto coordinate = (index / strides_[axis]) % extents_[axis];
        if (direction == Direction::negative && coordinate > 0) {
            return std::optional<Index>{index - strides_[axis]};
        }
        if (direction == Direction::positive && coordinate < extents_[axis] - 1) {
            return std::optional<Index>{index + strides_[axis]};
        }
        return std::optional<Index>{};
    }

    Result<Requirements> requirements(const Domain& domain) {
        const auto cells = domain.cells();
        Index storage = 0;
        Index twice_cells = 0;
        Index traversal = 0;
        Index minimum_work = 0;
        if (!multiply(cells, sizeof(std::uint32_t) + sizeof(Index), storage) || !add(storage, fixed_storage_allowance, storage) ||
            cells > std::vector<Index>{}.max_size() || cells > std::vector<std::uint32_t>{}.max_size() ||
            !multiply(cells, 2, twice_cells) || !multiply(twice_cells - 1, 2 * domain.dimensions() + 1, traversal) ||
            !add(twice_cells, traversal, minimum_work) || !add(minimum_work, cells - 1, minimum_work)) {
            return std::unexpected(invalid(Reason::overflow));
        }
        return Requirements{cells, cells - 1, storage, minimum_work};
    }

    Maze::Maze(Replay replay, std::vector<std::uint32_t> passages, Usage usage)
    : replay_(std::move(replay)), passages_(std::move(passages)), usage_(usage) {}

    Result<std::optional<Index>> Maze::neighbor(Index index, std::size_t axis, Direction direction) const {
        auto adjacent = domain().neighbor(index, axis, direction);
        if (!adjacent) {
            return std::unexpected(adjacent.error());
        }
        const auto bit = std::uint32_t{1} << (2 * axis + static_cast<std::uint32_t>(direction));
        if ((*adjacent).has_value() && (passages_[static_cast<std::size_t>(index)] & bit) != 0) {
            return adjacent;
        }
        return std::optional<Index>{};
    }

    Result<Maze> replay(const Replay& descriptor) {
        if (descriptor.algorithm != MazeAlgorithm::depth_first) {
            return std::unexpected(invalid(Reason::algorithm));
        }
        if (descriptor.version != 1) {
            return std::unexpected(invalid(Reason::version));
        }
        const auto needed = requirements(descriptor.domain);
        if (!needed) {
            return std::unexpected(needed.error());
        }
        const auto budget = descriptor.budgets;
        if (needed->cells > std::min(budget.cells, hard_limits.cells)) {
            return std::unexpected(refused(Reason::cells));
        }
        if (needed->edges > std::min(budget.edges, hard_limits.edges)) {
            return std::unexpected(refused(Reason::edges));
        }
        if (needed->storage_bytes > std::min(budget.storage_bytes, hard_limits.storage_bytes)) {
            return std::unexpected(refused(Reason::storage));
        }
        Work work(std::min(budget.work, hard_limits.work));
        if (needed->minimum_work > std::min(budget.work, hard_limits.work)) {
            return std::unexpected(refused(Reason::work));
        }
        // Charge both zero-initialized arrays before allocating either one.
        if (!work.consume(2 * needed->cells)) {
            return std::unexpected(refused(Reason::work));
        }
        std::vector<std::uint32_t> passages(static_cast<std::size_t>(needed->cells), 0);
        std::vector<Index> stack(static_cast<std::size_t>(needed->cells), 0);
        std::array<Candidate, 2 * max_dimensions> candidates{};
        auto stream = IntegerStream::for_stage(descriptor.random);
        Index draws = 0;
        std::size_t depth = 1;
        while (depth != 0) {
            if (!work.consume(1)) {
                return std::unexpected(refused(Reason::work));
            }
            const auto current = stack[depth - 1];
            std::size_t count = 0;
            for (std::size_t direction = 0; direction < 2 * descriptor.domain.dimensions(); ++direction) {
                if (!work.consume(1)) {
                    return std::unexpected(refused(Reason::work));
                }
                const auto next = descriptor.domain.neighbor(current, direction / 2, static_cast<Direction>(direction % 2)).value();
                // Every discovered non-root cell immediately receives a passage.
                // Index 0 is the initially visited root, including a one-cell maze.
                if (next && *next != 0 && passages[static_cast<std::size_t>(*next)] == 0) {
                    candidates[count++] = Candidate{*next, static_cast<std::uint32_t>(direction)};
                }
            }
            if (count == 0) {
                --depth;
                continue;
            }
            const auto before = work.used();
            const auto chosen = sample_below(stream, count, work);
            if (!chosen) {
                return std::unexpected(chosen.error());
            }
            draws += work.used() - before;
            const auto candidate = candidates[static_cast<std::size_t>(*chosen)];
            passages[static_cast<std::size_t>(current)] |= std::uint32_t{1} << candidate.direction;
            passages[static_cast<std::size_t>(candidate.cell)] |= std::uint32_t{1} << (candidate.direction ^ 1U);
            stack[depth++] = candidate.cell;
        }
        return Maze(descriptor, std::move(passages), Usage{work.used(), draws});
    }

    Result<Maze> generate(std::span<const Index> extents, StreamKey random, Budgets budgets) {
        auto domain = Domain::checked(extents);
        if (!domain) {
            return std::unexpected(domain.error());
        }
        return replay(Replay{*domain, random, budgets});
    }
}
