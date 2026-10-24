#include "drainage.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ProcGen {
    namespace {
        struct Stop {
            DrainageFailure failure;
        };
        [[noreturn]] void fail(FailureKind kind, DrainageReason reason) {
            throw Stop{{kind, reason}};
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b) {
                fail(FailureKind::invalid_input, DrainageReason::overflow);
            }
            return a + b;
        }
        Index multiply(Index a, Index b) {
            if (b != 0 && a > std::numeric_limits<Index>::max() / b) {
                fail(FailureKind::invalid_input, DrainageReason::overflow);
            }
            return a * b;
        }
        void charge(Work& work, Index count = 1) {
            if (!work.consume(count)) {
                fail(FailureKind::budget_refused, DrainageReason::work);
            }
        }
        DrainageRequirements needed_for(const Domain& domain, Index outlets) {
            if (domain.dimensions() != 2) {
                fail(FailureKind::invalid_input, DrainageReason::dimensions);
            }
            const auto cells = domain.cells();
            if (outlets > cells) {
                fail(FailureKind::invalid_input, DrainageReason::outlets);
            }
            if (cells > std::vector<DrainageCell>{}.max_size() || cells > std::vector<Height>{}.max_size() ||
                cells > std::vector<Index>{}.max_size() || cells > std::vector<std::uint32_t>{}.max_size()) {
                fail(FailureKind::invalid_input, DrainageReason::overflow);
            }
            const auto bytes =
                add(drainage_fixed_storage, multiply(cells, sizeof(DrainageCell) + sizeof(Height) + sizeof(std::uint32_t) + sizeof(Index)));
            const auto minimum = add(1024, multiply(13, cells) - multiply(2, outlets));
            return {cells, bytes, minimum, add(minimum, multiply(2, cells - outlets))};
        }
        static_assert(sizeof(Height) == 4 && sizeof(Index) == 8 && sizeof(DrainageCell) == 32);
        static_assert(2 * sizeof(Drainage) + 2 * sizeof(DrainageDescriptor) + 1024 <= fixed_storage_allowance);
    }
    std::string_view drainage_reason_text(DrainageReason reason) {
        switch (reason) {
            case DrainageReason::dimensions:
                return "drainage requires a positive checked two-axis domain";
            case DrainageReason::height_count:
                return "height count must equal domain cells";
            case DrainageReason::outlets:
                return "outlets must be distinct in-domain cell indexes";
            case DrainageReason::overflow:
                return "drainage index/storage/work arithmetic or capacity overflow";
            case DrainageReason::cells:
                return "drainage cell budget refused";
            case DrainageReason::storage:
                return "drainage accounted payload budget refused";
            case DrainageReason::work:
                return "drainage work budget refused or exhausted";
            case DrainageReason::algorithm:
                return "unsupported drainage algorithm";
            case DrainageReason::version:
                return "unsupported drainage version";
            case DrainageReason::index:
                return "drainage index outside domain";
        }
        return "unknown drainage failure";
    }
    DrainageResult<DrainageRequirements> drainage_requirements(const Domain& domain, Index outlet_count) {
        try {
            return needed_for(domain, outlet_count);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
    Drainage::Drainage(
        DrainageDescriptor descriptor,
        std::vector<Height> source,
        std::vector<DrainageCell> cells,
        DrainageMeasurements measured,
        Index work
    )
    : descriptor_(std::move(descriptor)), source_(std::move(source)), cells_(std::move(cells)), measured_(measured), work_(work) {}
    DrainageResult<DrainageCell> Drainage::cell(Index index) const {
        if (index >= cells_.size()) {
            return std::unexpected(DrainageFailure{FailureKind::invalid_input, DrainageReason::index});
        }
        return cells_[static_cast<std::size_t>(index)];
    }
    DrainageResult<Drainage> analyze_drainage(DrainageInput input, CaveBudgets budgets, DrainageOptions options) {
        try {
            if (options.algorithm != DrainageAlgorithm::steepest_cardinal) {
                fail(FailureKind::unsupported, DrainageReason::algorithm);
            }
            if (options.version != 1) {
                fail(FailureKind::unsupported, DrainageReason::version);
            }
            const auto& domain = input.domain;
            const auto needed = needed_for(domain, input.outlets.size());
            const CaveBudgets cap{
                std::min(budgets.cells, drainage_hard_limits.cells), std::min(budgets.storage_bytes, drainage_hard_limits.storage_bytes),
                std::min(budgets.work, drainage_hard_limits.work)
            };
            if (needed.cells > cap.cells) {
                fail(FailureKind::budget_refused, DrainageReason::cells);
            }
            if (needed.storage_bytes > cap.storage_bytes) {
                fail(FailureKind::budget_refused, DrainageReason::storage);
            }
            if (needed.minimum_work > cap.work) {
                fail(FailureKind::budget_refused, DrainageReason::work);
            }
            if (input.heights.size() != needed.cells) {
                fail(FailureKind::invalid_input, DrainageReason::height_count);
            }
            Work work(cap.work);
            charge(work, 1024);
            std::array<std::uint64_t, 1024> outlet_bits{};
            for (auto index : input.outlets) {
                charge(work);
                if (index >= needed.cells) {
                    fail(FailureKind::invalid_input, DrainageReason::outlets);
                }
                auto& word = outlet_bits[static_cast<std::size_t>(index / 64)];
                const auto bit = UINT64_C(1) << (index % 64);
                if ((word & bit) != 0) {
                    fail(FailureKind::invalid_input, DrainageReason::outlets);
                }
                word |= bit;
            }
            charge(work, multiply(4, needed.cells));
            const auto count = static_cast<std::size_t>(needed.cells);
            std::vector<Height> source(input.heights.begin(), input.heights.end());
            std::vector<DrainageCell> cells(count);
            std::vector<std::uint32_t> incoming(count, 0);
            std::vector<Index> order(count, 0);
            for (auto index : input.outlets) {
                charge(work);
                cells[static_cast<std::size_t>(index)].kind = TerminalKind::outlet;
            }
            DrainageMeasurements measured{};
            measured.cells = needed.cells;
            for (Index index = 0; index < needed.cells; ++index) {
                charge(work);
                auto& cell = cells[static_cast<std::size_t>(index)];
                if (cell.kind == TerminalKind::outlet) {
                    ++measured.outlets;
                    continue;
                }
                std::int64_t best = 0;
                for (std::size_t direction = 0; direction < 4; ++direction) {
                    charge(work); // includes outside / level / uphill attempts
                    const auto neighbor = domain.neighbor(index, direction / 2, static_cast<Direction>(direction % 2)).value();
                    if (!neighbor) {
                        continue;
                    }
                    const auto drop =
                        static_cast<std::int64_t>(source[static_cast<std::size_t>(index)]) - source[static_cast<std::size_t>(*neighbor)];
                    if (drop > 0 && (drop > best || (drop == best && *neighbor < cell.receiver))) {
                        best = drop;
                        cell.receiver = *neighbor;
                    }
                }
                if (cell.receiver == no_receiver) {
                    cell.kind = TerminalKind::sink;
                    ++measured.sinks;
                } else {
                    charge(work);
                    auto& degree = incoming[static_cast<std::size_t>(cell.receiver)];
                    if (degree >= 4) {
                        throw std::logic_error("Drainage indegree invariant failed");
                    }
                    ++degree;
                    ++measured.links;
                }
            }
            Index head = 0, tail = 0;
            const auto push = [&](Index index) {
                charge(work);
                if (tail >= needed.cells) {
                    throw std::logic_error("Drainage queue capacity invariant failed");
                }
                order[static_cast<std::size_t>(tail++)] = index;
            };
            for (Index index = 0; index < needed.cells; ++index) {
                charge(work);
                if (incoming[static_cast<std::size_t>(index)] == 0) {
                    push(index);
                }
            }
            while (head < tail) {
                charge(work);
                const auto index = order[static_cast<std::size_t>(head++)];
                const auto& cell = cells[static_cast<std::size_t>(index)];
                if (cell.receiver == no_receiver) {
                    continue;
                }
                charge(work);
                auto& receiver = cells[static_cast<std::size_t>(cell.receiver)];
                receiver.contributing = add(receiver.contributing, cell.contributing);
                auto& degree = incoming[static_cast<std::size_t>(cell.receiver)];
                if (receiver.contributing > needed.cells || degree == 0) {
                    throw std::logic_error("Drainage accumulation invariant failed");
                }
                if (--degree == 0) {
                    push(cell.receiver);
                }
            }
            if (tail != needed.cells) {
                throw std::logic_error("Strict descent unexpectedly contains a cycle");
            }
            for (Index position = tail; position != 0; --position) {
                charge(work);
                const auto index = order[static_cast<std::size_t>(position - 1)];
                auto& cell = cells[static_cast<std::size_t>(index)];
                measured.maximum_contributing = std::max(measured.maximum_contributing, cell.contributing);
                if (cell.receiver != no_receiver) {
                    cell.terminal = cells[static_cast<std::size_t>(cell.receiver)].terminal;
                    if (cell.terminal == no_receiver) {
                        throw std::logic_error("Drainage terminal order invariant failed");
                    }
                } else {
                    cell.terminal = index;
                    measured.largest_catchment = std::max(measured.largest_catchment, cell.contributing);
                    auto& sum = cell.kind == TerminalKind::outlet ? measured.outlet_contributing : measured.sink_contributing;
                    sum = add(sum, cell.contributing);
                }
            }
            if (add(measured.outlet_contributing, measured.sink_contributing) != needed.cells) {
                throw std::logic_error("Drainage terminal conservation invariant failed");
            }
            return Drainage({domain, budgets, options}, std::move(source), std::move(cells), measured, work.used());
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
}
