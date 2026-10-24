#pragma once

#include "traversal.h"

#include <algorithm>
#include <limits>

namespace ProcGen::TraversalDetail {
    // Only this private control exception becomes a typed expected error.
    // Allocation, standard-library and programming exceptions are never caught.
    struct Stop {
        Failure failure;
    };

    [[noreturn]] inline void invalid(Reason reason) {
        throw Stop{{FailureKind::invalid_input, reason}};
    }
    [[noreturn]] inline void refused(Reason reason) {
        throw Stop{{FailureKind::budget_refused, reason}};
    }

    inline void charge(Work& work, Index units = 1) {
        if (!work.consume(units)) {
            refused(Reason::work);
        }
    }

    inline Index add(Index left, Index right) {
        if (right > std::numeric_limits<Index>::max() - left) {
            invalid(Reason::overflow);
        }
        return left + right;
    }

    inline Index multiply(Index left, Index right) {
        if (right != 0 && left > std::numeric_limits<Index>::max() / right) {
            invalid(Reason::overflow);
        }
        return left * right;
    }

    inline Budgets effective(Budgets budget, Budgets hard) {
        return {
            std::min(budget.cells, hard.cells), std::min(budget.edges, hard.edges), std::min(budget.storage_bytes, hard.storage_bytes),
            std::min(budget.work, hard.work)
        };
    }

    inline void admit(const Domain& domain, TraversalRequirements needed, Budgets budget) {
        if (domain.cells() > budget.cells) {
            refused(Reason::cells);
        }
        if (needed.storage_bytes > budget.storage_bytes) {
            refused(Reason::storage);
        }
        if (needed.minimum_work > budget.work) {
            refused(Reason::work);
        }
    }

    inline void header(MazeView graph, Index entrance, Index goal) {
        if (entrance >= graph.domain.cells() || goal >= graph.domain.cells()) {
            invalid(Reason::endpoint);
        }
        if (graph.passages.size() != graph.domain.cells()) {
            invalid(Reason::topology);
        }
        const auto slots = multiply(graph.domain.cells(), graph.domain.dimensions());
        if (!graph.costs.empty() && graph.costs.size() != slots) {
            invalid(Reason::cost);
        }
    }

    inline Index validation_work(const Domain& domain) {
        return multiply(domain.cells(), 1 + 2 * domain.dimensions());
    }

    inline Index validate(MazeView graph, Budgets budget, Work& work) {
        const auto dimensions = graph.domain.dimensions();
        Index edges = 0;
        for (Index cell = 0; cell < graph.domain.cells(); ++cell) {
            charge(work);
            const auto bits = graph.passages[static_cast<std::size_t>(cell)];
            if ((static_cast<std::uint64_t>(bits) >> (2 * dimensions)) != 0) {
                invalid(Reason::topology);
            }
            for (std::size_t direction = 0; direction < 2 * dimensions; ++direction) {
                charge(work);
                const bool open = (bits & (std::uint32_t{1} << direction)) != 0;
                if (direction % 2 == 1 && !graph.costs.empty()) {
                    const auto cost = graph.costs[static_cast<std::size_t>(cell) * dimensions + direction / 2];
                    if ((cost != 0) != open) {
                        invalid(Reason::cost);
                    }
                }
                if (!open) {
                    continue;
                }
                const auto next = graph.domain.neighbor(cell, direction / 2, static_cast<Direction>(direction % 2)).value();
                if (!next || (graph.passages[static_cast<std::size_t>(*next)] & (std::uint32_t{1} << (direction ^ 1U))) == 0) {
                    invalid(Reason::topology);
                }
                if (direction % 2 == 1 && ++edges > budget.edges) {
                    refused(Reason::edges);
                }
            }
        }
        return edges;
    }

    struct Arc {
        Index to;
        Index cost;
    };

    // Caller must have fully validated this immutable view first.
    inline std::optional<Arc> arc(MazeView graph, Index cell, std::size_t direction) {
        if ((graph.passages[static_cast<std::size_t>(cell)] & (std::uint32_t{1} << direction)) == 0) {
            return std::nullopt;
        }
        const auto next = graph.domain.neighbor(cell, direction / 2, static_cast<Direction>(direction % 2)).value().value();
        const auto owner = std::min(cell, next);
        const auto cost =
            graph.costs.empty() ? Index{1} : graph.costs[static_cast<std::size_t>(owner) * graph.domain.dimensions() + direction / 2];
        return Arc{next, cost};
    }
}
