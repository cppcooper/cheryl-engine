#include "configuration.h"
#include "traversal-internal.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ProcGen {
    namespace {
        using namespace TraversalDetail;
        static_assert(
            std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53 &&
                std::numeric_limits<double>::max_exponent == 1024,
            "Normalization requires IEEE binary64"
        );
        struct Candidate {
            Index cell;
            std::size_t direction;
        };
        static_assert(
            2 * sizeof(ConfiguredMaze) + 2 * sizeof(ConfiguredReplay) + 2 * sizeof(Measurements) +
                sizeof(std::array<Candidate, 2 * max_dimensions>) + 4 * sizeof(Coordinate) + 1024 <=
            configured_fixed_allowance
        );

        [[noreturn]] void unavailable(Reason reason) {
            throw Stop{{FailureKind::unsupported, reason}};
        }
        [[noreturn]] void contradiction() {
            throw Stop{{FailureKind::contradiction, Reason::endpoint_policy}};
        }

        EffectiveConfiguration normalize(const Domain& domain, Configuration requested) {
            const auto value = requested.traversability;
            if (!std::isfinite(value) || value < 0 || value > 1) {
                invalid(Reason::traversability);
            }
            if (requested.coverage != Coverage::full) {
                unavailable(Reason::coverage);
            }
            // Power-of-two scaling is exact for binary64 in this range. Subtract
            // the truncated integer rather than adding .5 before truncation.
            const auto scaled = value * traversal_scale;
            const auto integral = static_cast<std::uint32_t>(scaled);
            const auto ticks = integral + static_cast<std::uint32_t>(scaled - integral >= 0.5);
            const auto cells = domain.cells();
            const auto length = cells == 1 ? 1 : add(2, multiply(ticks, cells - 2) / traversal_scale);
            if (requested.endpoints) {
                const auto [entrance, goal] = *requested.endpoints;
                if (entrance >= cells || goal >= cells) {
                    invalid(Reason::endpoint);
                }
                if (cells > 1 && entrance == goal) {
                    contradiction();
                }
                if (length == cells && cells > 1) {
                    const auto parity = [&](Index index) {
                        unsigned result = 0;
                        for (const auto extent : domain.extents()) {
                            result ^= static_cast<unsigned>((index % extent) & 1U);
                            index /= extent;
                        }
                        return result;
                    };
                    const auto first = parity(entrance);
                    const auto last = parity(goal);
                    // Even rectangular domains need opposite colors. With odd
                    // cell count every extent is odd, and color 0 has one more
                    // cell: both ends of a spanning path must have that color.
                    if (cells % 2 == 0 ? first == last : first != 0 || last != 0) {
                        contradiction();
                    }
                }
            }
            return {ticks, length};
        }

        ConfiguredRequirements preflight(const Domain& domain, Configuration requested) {
            const auto profile = normalize(domain, requested);
            const auto cells = domain.cells();
            const auto length = profile.route_cells;
            const auto dimensions = domain.dimensions();
            if (cells > std::vector<Index>{}.max_size() || cells > std::vector<std::uint32_t>{}.max_size()) {
                invalid(Reason::overflow);
            }
            const auto metric = metric_requirements(domain);
            if (!metric) {
                throw Stop{metric.error()};
            }
            const auto output = add(multiply(cells, sizeof(std::uint32_t)), multiply(length, sizeof(Index)));
            const auto scratch = std::max(multiply(cells, sizeof(Index)), metric->storage_bytes);
            const auto storage = add(configured_fixed_allowance, add(output, scratch));
            // All initialized elements, mapping actions/axes, route copies,
            // minimum core link probes, tail probes/links/draws, stage setup,
            // U2 validation/flood and minimum selected-route/tree analysis work.
            auto minimum = add(multiply(2, cells), length);
            minimum = add(minimum, multiply(cells, dimensions + 1));
            minimum = add(minimum, add(length, multiply(2, length - 1)));
            minimum = add(minimum, multiply(cells - length, 2 * dimensions + 3));
            minimum = add(minimum, 6 * dimensions + 1);
            minimum = add(minimum, metric->minimum_work);
            minimum = add(minimum, add(add(length, length - 1), multiply(cells, 2 + 2 * dimensions)));
            return {profile, {cells, cells - 1, storage, minimum}};
        }

        struct Serpentine {
            const Domain& domain;
            std::array<std::size_t, max_dimensions> axes{};
            Coordinate lower{};
            std::array<bool, max_dimensions> reflected{};

            Index at(Index ordinal, Work& work) const {
                charge(work);
                Index index = 0;
                for (std::size_t rank = 0; rank < domain.dimensions(); ++rank) {
                    charge(work);
                    const auto axis = axes[rank];
                    const auto extent = domain.extents()[axis];
                    const auto upper = lower[rank] * extent; // <= validated C
                    auto coordinate = (ordinal / lower[rank]) % extent;
                    if ((ordinal / upper) % 2 != 0) {
                        coordinate = extent - 1 - coordinate;
                    }
                    if (reflected[rank]) {
                        coordinate = extent - 1 - coordinate;
                    }
                    index += coordinate * domain.strides()[axis]; // sum < C
                }
                return index;
            }
        };

        void open(std::vector<std::uint32_t>& passages, Index from, Index to, std::size_t direction) {
            passages[static_cast<std::size_t>(from)] |= std::uint32_t{1} << direction;
            passages[static_cast<std::size_t>(to)] |= std::uint32_t{1} << (direction ^ 1U);
        }
        void join(const Domain& domain, std::vector<std::uint32_t>& passages, Index from, Index to, Work& work) {
            charge(work);
            for (std::size_t direction = 0; direction < 2 * domain.dimensions(); ++direction) {
                charge(work);
                if (domain.neighbor(from, direction / 2, static_cast<Direction>(direction % 2)).value() == to) {
                    open(passages, from, to, direction);
                    return;
                }
            }
            throw std::logic_error("Serpentine order contains a non-axis step");
        }
    }

    Result<ConfiguredRequirements> configured_requirements(const Domain& domain, Configuration requested) {
        try {
            return preflight(domain, requested);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }

    ConfiguredMaze::ConfiguredMaze(
        ConfiguredReplay replay,
        EffectiveConfiguration effective,
        Endpoints endpoints,
        std::vector<std::uint32_t> passages,
        std::vector<Index> route,
        Measurements measured,
        Usage usage
    )
    : replay_(std::move(replay)),
      effective_(effective),
      endpoints_(endpoints),
      passages_(std::move(passages)),
      route_(std::move(route)),
      measured_(measured),
      usage_(usage) {}

    MazeView maze_view(const ConfiguredMaze& maze) {
        return {maze.domain(), maze.passages(), {}};
    }

    Result<ConfiguredMaze> replay_configured(const ConfiguredReplay& descriptor) {
        try {
            if (descriptor.algorithm != ConfiguredAlgorithm::serpentine_tree) {
                invalid(Reason::algorithm);
            }
            if (descriptor.version != 1) {
                invalid(Reason::version);
            }
            const auto& domain = descriptor.domain;
            const auto needed = preflight(domain, descriptor.requested);
            const auto budget = effective(descriptor.budgets, configured_hard_limits);
            admit(domain, {needed.resources.storage_bytes, needed.resources.minimum_work}, budget);
            if (needed.resources.edges > budget.edges) {
                refused(Reason::edges);
            }
            Work work(budget.work);
            auto random = IntegerStream::for_stage(descriptor.random);
            Index draws = 0;
            const auto draw = [&](Index bound) {
                const auto before = work.used();
                const auto result = sample_below(random, bound, work);
                if (!result) {
                    throw Stop{result.error()};
                }
                draws += work.used() - before;
                return *result;
            };
            Serpentine order{domain};
            const auto dimensions = domain.dimensions();
            for (std::size_t axis = 0; axis < dimensions; ++axis) {
                charge(work);
                order.axes[axis] = axis;
            }
            for (auto count = dimensions; count > 1; --count) {
                const auto selected = static_cast<std::size_t>(draw(count));
                std::swap(order.axes[count - 1], order.axes[selected]);
            }
            Index stride = 1;
            for (std::size_t rank = 0; rank < dimensions; ++rank) {
                charge(work);
                order.lower[rank] = stride;
                stride *= domain.extents()[order.axes[rank]]; // <= C
                order.reflected[rank] = draw(2) != 0;
            }
            const auto length = needed.effective.route_cells;
            Endpoints endpoints{order.at(0, work), order.at(length - 1, work)};
            bool reverse = false;
            if (descriptor.requested.endpoints) {
                const auto fixed = *descriptor.requested.endpoints;
                if (fixed == Endpoints{endpoints.goal, endpoints.entrance}) {
                    reverse = true;
                } else if (fixed != endpoints) {
                    unavailable(Reason::endpoint_policy);
                }
                endpoints = fixed;
            }
            charge(work, add(multiply(2, domain.cells()), length));
            std::vector<std::uint32_t> passages(static_cast<std::size_t>(domain.cells()), 0);
            std::vector<Index> route(static_cast<std::size_t>(length), 0);
            {
                std::vector<Index> sequence(static_cast<std::size_t>(domain.cells()), 0);
                for (Index position = 0; position < domain.cells(); ++position) {
                    sequence[static_cast<std::size_t>(position)] = order.at(position, work);
                }
                for (Index position = 0; position < length; ++position) {
                    charge(work);
                    route[static_cast<std::size_t>(position)] =
                        sequence[static_cast<std::size_t>(reverse ? length - 1 - position : position)];
                    if (position > 0) {
                        join(
                            domain, passages, route[static_cast<std::size_t>(position - 1)], route[static_cast<std::size_t>(position)], work
                        );
                    }
                }
                for (Index position = length; position < domain.cells(); ++position) {
                    charge(work);
                    const auto current = sequence[static_cast<std::size_t>(position)];
                    std::array<Candidate, 2 * max_dimensions> candidates{};
                    std::size_t count = 0;
                    for (std::size_t direction = 0; direction < 2 * dimensions; ++direction) {
                        charge(work);
                        const auto neighbor = domain.neighbor(current, direction / 2, static_cast<Direction>(direction % 2)).value();
                        if (neighbor && passages[static_cast<std::size_t>(*neighbor)] != 0) {
                            candidates[count++] = {*neighbor, direction};
                        }
                    }
                    // C>1 has L>=2, so every core cell has a passage. The
                    // preceding serpentine cell is already visited: count>0.
                    if (count == 0) {
                        throw std::logic_error("Serpentine attachment lost its predecessor");
                    }
                    const auto chosen = candidates[static_cast<std::size_t>(draw(count))];
                    charge(work);
                    open(passages, current, chosen.cell, chosen.direction);
                }
            } // Release sequence before U2 metric scratch is allocated.
            const auto metric = metric_requirements(domain).value();
            const auto measured = measure(
                {domain, passages, {}}, endpoints.entrance, endpoints.goal, route,
                {domain.cells(), domain.cells() - 1, metric.storage_bytes, budget.work - work.used()}
            );
            if (!measured) {
                throw Stop{measured.error()};
            }
            charge(work, measured->work);
            if (!measured->route || !measured->tree_branches || measured->route->cells != length ||
                measured->tree_branches->off_route_cells != domain.cells() - length ||
                (length == domain.cells() && !measured->strict_labyrinth)) {
                throw std::logic_error("Configured tree violated its route profile");
            }
            return ConfiguredMaze(
                descriptor, needed.effective, endpoints, std::move(passages), std::move(route), *measured, {work.used(), draws}
            );
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }

    Result<ConfiguredMaze> generate_configured(std::span<const Index> extents, StreamKey random, Configuration requested, Budgets budgets) {
        const auto domain = Domain::checked(extents);
        if (!domain) {
            return std::unexpected(domain.error());
        }
        return replay_configured({*domain, random, budgets, requested});
    }
}
