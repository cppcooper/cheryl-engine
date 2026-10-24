#include "clearance.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ProcGen {
    namespace {
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
            2 * sizeof(Clearance) + 2 * sizeof(ClearanceDescriptor) + sizeof(ClearanceRequirements) + 512 <= fixed_storage_allowance
        );

        ClearanceRequirements requirements_for(const Domain& domain, std::int64_t minimum) {
            if (domain.dimensions() != 2) {
                invalid(Reason::clearance_dimensions);
            }
            if (minimum < 0) {
                invalid(Reason::clearance_threshold);
            }
            const auto cells = domain.cells();
            if (cells > std::vector<Index>{}.max_size() || cells > std::vector<std::uint8_t>{}.max_size()) {
                invalid(Reason::overflow);
            }
            return {
                cells, add(fixed_storage_allowance, multiply(cells, 2 * sizeof(Index) + sizeof(std::uint8_t))), multiply(15, cells),
                multiply(21, cells)
            };
        }
    }

    Result<ClearanceRequirements> clearance_requirements(const Domain& domain, std::int64_t minimum) {
        try {
            return requirements_for(domain, minimum);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }

    Clearance::Clearance(
        ClearanceDescriptor descriptor,
        std::vector<Index> distances,
        std::vector<std::uint8_t> usable,
        ClearanceMeasurements measured,
        Index work
    )
    : descriptor_(std::move(descriptor)), distances_(std::move(distances)), usable_(std::move(usable)), measured_(measured), work_(work) {}

    Result<Clearance> analyze_clearance(OccupancyView input, std::int64_t minimum, CaveBudgets budget) {
        try {
            const auto& domain = input.domain;
            const auto needed = requirements_for(domain, minimum);
            if (input.occupancy.size() != needed.cells) {
                invalid(Reason::occupancy);
            }
            budget = {
                std::min(budget.cells, clearance_hard_limits.cells), std::min(budget.storage_bytes, clearance_hard_limits.storage_bytes),
                std::min(budget.work, clearance_hard_limits.work)
            };
            if (needed.cells > budget.cells) {
                refused(Reason::cells);
            }
            if (needed.storage_bytes > budget.storage_bytes) {
                refused(Reason::storage);
            }
            if (needed.minimum_work > budget.work) {
                refused(Reason::work);
            }
            Work work(budget.work);
            ClearanceMeasurements measured;
            for (const auto cell : input.occupancy) {
                charge(work);
                if (cell > 1) {
                    invalid(Reason::occupancy);
                }
                if (cell == 0) {
                    ++measured.floor;
                }
            }
            measured.solid = needed.cells - measured.floor;
            // These thresholds select every floor, so exact complete work is
            // known from validation and can be refused before owned allocation.
            if (minimum <= 1 && add(needed.minimum_work, multiply(6, measured.floor)) > budget.work) {
                refused(Reason::work);
            }
            charge(work, multiply(3, needed.cells));
            constexpr auto unseen = std::numeric_limits<Index>::max();
            const auto size = static_cast<std::size_t>(needed.cells);
            std::vector<Index> distances(size, unseen), queue(size, 0);
            std::vector<std::uint8_t> usable(size, 0);
            Index head = 0, tail = 0;
            const auto push = [&](Index cell, Index distance) {
                charge(work);
                if (tail >= needed.cells) {
                    throw std::logic_error("Clearance distance queue invariant failed");
                }
                distances[static_cast<std::size_t>(cell)] = distance;
                queue[static_cast<std::size_t>(tail++)] = cell;
            };
            for (Index cell = 0; cell < needed.cells; ++cell) {
                charge(work);
                if (input.occupancy[static_cast<std::size_t>(cell)] == 1) {
                    push(cell, 0);
                }
            }
            const auto width = domain.extents()[0], height = domain.extents()[1];
            // All distance0 sources MUST precede distance1 boundary sources.
            // Mixing them in one scan could finalize a longer distance first.
            for (Index cell = 0; cell < needed.cells; ++cell) {
                charge(work);
                const auto x = cell % width, y = cell / width;
                if (input.occupancy[static_cast<std::size_t>(cell)] == 0 && (x == 0 || x == width - 1 || y == 0 || y == height - 1)) {
                    push(cell, 1);
                }
            }
            while (head < tail) {
                charge(work);
                const auto current = queue[static_cast<std::size_t>(head++)];
                for (std::size_t direction = 0; direction < 4; ++direction) {
                    charge(work);
                    const auto neighbor = domain.neighbor(current, direction / 2, static_cast<Direction>(direction % 2)).value();
                    if (neighbor && distances[static_cast<std::size_t>(*neighbor)] == unseen) {
                        push(*neighbor, add(distances[static_cast<std::size_t>(current)], 1));
                    }
                }
            }
            if (tail != needed.cells) {
                throw std::logic_error("Clearance transform missed a finite cell");
            }
            const auto threshold = static_cast<Index>(minimum);
            for (Index cell = 0; cell < needed.cells; ++cell) {
                charge(work);
                const auto distance = distances[static_cast<std::size_t>(cell)];
                measured.maximum = std::max(measured.maximum, distance);
                const bool selected = input.occupancy[static_cast<std::size_t>(cell)] == 0 && distance >= threshold;
                usable[static_cast<std::size_t>(cell)] = static_cast<std::uint8_t>(selected);
                measured.usable += static_cast<Index>(selected);
            }
            for (Index first = 0; first < needed.cells; ++first) {
                charge(work);
                if (usable[static_cast<std::size_t>(first)] != 1) {
                    continue;
                }
                head = 0;
                tail = 0;
                const auto visit = [&](Index cell) {
                    charge(work);
                    if (tail >= needed.cells) {
                        throw std::logic_error("Clearance region queue invariant failed");
                    }
                    usable[static_cast<std::size_t>(cell)] = 3; // selected + seen
                    queue[static_cast<std::size_t>(tail++)] = cell;
                };
                visit(first);
                while (head < tail) {
                    charge(work);
                    const auto current = queue[static_cast<std::size_t>(head++)];
                    for (std::size_t direction = 0; direction < 4; ++direction) {
                        charge(work);
                        const auto neighbor = domain.neighbor(current, direction / 2, static_cast<Direction>(direction % 2)).value();
                        if (neighbor && usable[static_cast<std::size_t>(*neighbor)] == 1) {
                            visit(*neighbor);
                        }
                    }
                }
                ++measured.components;
                measured.largest_region = std::max(measured.largest_region, tail);
            }
            for (auto& flags : usable) {
                charge(work);
                flags &= 1U;
            }
            return Clearance({domain, threshold}, std::move(distances), std::move(usable), measured, work.used());
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
}
