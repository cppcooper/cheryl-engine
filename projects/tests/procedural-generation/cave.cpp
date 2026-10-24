#include "cave.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace ProcGen {
    namespace {
        // Local expected-failure transport only. Allocation and programming
        // exceptions are not caught or relabeled. No traversal/maze state reused.
        struct Stop {
            Failure failure;
        };
        [[noreturn]] void invalid(Reason reason) {
            throw Stop{{FailureKind::invalid_input, reason}};
        }
        [[noreturn]] void refused(Reason reason) {
            throw Stop{{FailureKind::budget_refused, reason}};
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b) {
                invalid(Reason::overflow);
            }
            return a + b;
        }
        Index multiply(Index a, Index b) {
            if (b != 0 && a > std::numeric_limits<Index>::max() / b) {
                invalid(Reason::overflow);
            }
            return a * b;
        }
        void charge(Work& work, Index count = 1) {
            if (!work.consume(count)) {
                refused(Reason::work);
            }
        }
        static_assert(
            2 * sizeof(Cave) + 2 * sizeof(CaveReplay) + sizeof(Domain) + sizeof(CaveRequirements) + 512 <= fixed_storage_allowance
        );

        CaveRequirements preflight(const Domain& domain, CaveConfiguration config) {
            if (domain.dimensions() != 2) {
                invalid(Reason::cave_dimensions);
            }
            if (config.solid_fill > cave_fill_scale) {
                invalid(Reason::cave_fill);
            }
            if (config.steps > cave_max_steps) {
                invalid(Reason::cave_steps);
            }
            if ((static_cast<unsigned>(config.birth | config.survival) & ~0x1ffU) != 0) {
                invalid(Reason::cave_rules);
            }
            if (config.connectivity != CaveConnectivity::retain_components && config.connectivity != CaveConnectivity::largest_floor) {
                invalid(Reason::cave_connectivity);
            }
            const auto cells = domain.cells();
            if (cells > std::vector<Index>{}.max_size() || cells > std::vector<std::uint8_t>{}.max_size()) {
                invalid(Reason::overflow);
            }
            const auto width = domain.extents()[0];
            const auto height = domain.extents()[1];
            const auto interior = width > 2 && height > 2 ? multiply(width - 2, height - 2) : 0;
            const auto storage = add(fixed_storage_allowance, multiply(cells, 2 * sizeof(std::uint8_t) + 2 * sizeof(Index)));
            auto reservation = add(multiply(6, cells), multiply(7, interior));
            reservation = add(reservation, multiply(config.steps, add(1, add(cells, multiply(8, interior)))));
            if (config.connectivity == CaveConnectivity::largest_floor) {
                reservation = add(reservation, cells);
            }
            return {cells, interior, storage, reservation};
        }

        bool boundary(Index cell, Index width, Index height) {
            const auto x = cell % width;
            const auto y = cell / width;
            return x == 0 || x == width - 1 || y == 0 || y == height - 1;
        }
    }

    Result<CaveRequirements> cave_requirements(const Domain& domain, CaveConfiguration config) {
        try {
            return preflight(domain, config);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }

    Cave::Cave(CaveReplay replay, std::vector<std::uint8_t> occupancy, CaveMeasurements measured, Usage usage)
    : replay_(std::move(replay)), occupancy_(std::move(occupancy)), measured_(measured), usage_(usage) {}

    Result<Cave> replay_cave(const CaveReplay& descriptor) {
        try {
            if (descriptor.algorithm != CaveAlgorithm::cellular_2d) {
                invalid(Reason::cave_algorithm);
            }
            if (descriptor.version != 1) {
                invalid(Reason::cave_version);
            }
            const auto& domain = descriptor.domain;
            const auto config = descriptor.requested;
            const auto needed = preflight(domain, config);
            const CaveBudgets budget{
                std::min(descriptor.budgets.cells, cave_hard_limits.cells),
                std::min(descriptor.budgets.storage_bytes, cave_hard_limits.storage_bytes),
                std::min(descriptor.budgets.work, cave_hard_limits.work)
            };
            if (needed.cells > budget.cells) {
                refused(Reason::cells);
            }
            if (needed.storage_bytes > budget.storage_bytes) {
                refused(Reason::storage);
            }
            if (needed.work_reservation > budget.work) {
                refused(Reason::work);
            }
            Work work(budget.work);
            charge(work, multiply(4, needed.cells));
            const auto size = static_cast<std::size_t>(needed.cells);
            std::vector<std::uint8_t> current(size, 1), next(size, 1);
            std::vector<Index> labels(size, 0), queue(size, 0);
            auto random = IntegerStream::for_stage(descriptor.random);
            Index draws = 0;
            const auto width = domain.extents()[0];
            const auto height = domain.extents()[1];
            CaveMeasurements measured{};
            for (Index cell = 0; cell < needed.cells; ++cell) {
                charge(work);
                if (boundary(cell, width, height)) {
                    continue;
                }
                const auto before = work.used();
                const auto sampled = sample_below(random, cave_fill_scale, work);
                if (!sampled) {
                    throw Stop{sampled.error()};
                }
                draws += work.used() - before;
                const bool solid = *sampled < config.solid_fill;
                current[static_cast<std::size_t>(cell)] = static_cast<std::uint8_t>(solid);
                measured.initial_interior_solid += static_cast<Index>(solid);
            }
            for (std::uint32_t step = 0; step < config.steps; ++step) {
                charge(work);
                for (Index cell = 0; cell < needed.cells; ++cell) {
                    charge(work);
                    if (boundary(cell, width, height)) {
                        next[static_cast<std::size_t>(cell)] = 1;
                        continue;
                    }
                    // Interior cell: all nine positions are within the finite
                    // domain; subtractions and base+2*width+2 stay in [0,C).
                    const auto base = cell - width - 1;
                    unsigned solid_neighbors = 0;
                    for (Index row = 0; row < 3; ++row) {
                        for (Index column = 0; column < 3; ++column) {
                            if (row == 1 && column == 1) {
                                continue;
                            }
                            charge(work);
                            solid_neighbors += current[static_cast<std::size_t>(base + row * width + column)];
                        }
                    }
                    const auto rule = current[static_cast<std::size_t>(cell)] != 0 ? config.survival : config.birth;
                    next[static_cast<std::size_t>(cell)] = static_cast<std::uint8_t>((rule >> solid_neighbors) & 1U);
                }
                current.swap(next); // only after EVERY cell was evaluated
            }
            Index largest_label = 0;
            for (Index first = 0; first < needed.cells; ++first) {
                charge(work);
                if (current[static_cast<std::size_t>(first)] != 0 || labels[static_cast<std::size_t>(first)] != 0) {
                    continue;
                }
                const auto label = first + 1; // <= admitted C, zero reserved for unseen/solid
                Index head = 0, tail = 1;
                charge(work);
                labels[static_cast<std::size_t>(first)] = label;
                queue[0] = first;
                while (head < tail) {
                    charge(work);
                    const auto cell = queue[static_cast<std::size_t>(head++)];
                    for (std::size_t direction = 0; direction < 4; ++direction) {
                        charge(work);
                        const auto neighbor = domain.neighbor(cell, direction / 2, static_cast<Direction>(direction % 2)).value();
                        if (!neighbor) {
                            continue;
                        }
                        const auto there = static_cast<std::size_t>(*neighbor);
                        if (current[there] != 0 || labels[there] != 0) {
                            continue;
                        }
                        charge(work);
                        labels[there] = label;
                        queue[static_cast<std::size_t>(tail++)] = *neighbor;
                    }
                }
                ++measured.generated_components;
                measured.generated_floor += tail;
                if (tail > measured.largest_floor) {
                    measured.largest_floor = tail;
                    largest_label = label;
                }
            }
            measured.connectivity_required = config.connectivity == CaveConnectivity::largest_floor;
            if (measured.connectivity_required) {
                for (Index cell = 0; cell < needed.cells; ++cell) {
                    charge(work);
                    if (labels[static_cast<std::size_t>(cell)] != largest_label) {
                        current[static_cast<std::size_t>(cell)] = 1;
                    }
                }
                measured.floor = measured.largest_floor;
                measured.components = measured.floor != 0 ? 1 : 0;
                measured.removed_floor = measured.generated_floor - measured.floor;
            } else {
                measured.floor = measured.generated_floor;
                measured.components = measured.generated_components;
            }
            measured.solid = needed.cells - measured.floor;
            measured.connected = measured.components == 1;
            return Cave(descriptor, std::move(current), measured, {work.used(), draws});
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }

    Result<Cave> generate_cave(std::span<const Index> extents, StreamKey random, CaveConfiguration config, CaveBudgets budgets) {
        if (extents.size() != 2) {
            return std::unexpected(Failure{FailureKind::invalid_input, Reason::cave_dimensions});
        }
        const auto domain = Domain::checked(extents);
        if (!domain) {
            return std::unexpected(domain.error());
        }
        return replay_cave({*domain, random, config, budgets});
    }
}
