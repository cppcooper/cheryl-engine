#include "traversal.h"
#include "traversal-internal.h"

#include <bit>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace ProcGen {
    namespace {
        using namespace TraversalDetail;

        enum class State { unseen, open, settled };
        struct Record {
            Index cost = 0;
            Index parent = 0;
            State state = State::unseen;
        };
        struct Entry {
            Index priority;
            Index cost;
            Index cell;
        };
        struct Later {
            bool operator()(const Entry& left, const Entry& right) const {
                return std::tie(left.priority, left.cost, left.cell) > std::tie(right.priority, right.cost, right.cell);
            }
        };

        static_assert(2 * sizeof(SearchResult) + 2 * sizeof(Domain) + 3 * sizeof(std::vector<Index>) + 512 <= fixed_storage_allowance);

        TraversalRequirements route_storage(const Domain& domain, Index queue_entries) {
            const auto cells = domain.cells();
            if (cells > std::vector<Record>{}.max_size() || cells > std::vector<Index>{}.max_size() ||
                queue_entries > std::vector<Entry>{}.max_size()) {
                invalid(Reason::overflow);
            }
            const auto storage =
                add(fixed_storage_allowance, add(multiply(cells, sizeof(Record) + sizeof(Index)), multiply(queue_entries, sizeof(Entry))));
            // Mandatory validation + initialized records + minimum start h/push/pop/settlement.
            return {storage, add(validation_work(domain), add(cells, 7))};
        }

        Index estimate(const Domain& domain, Index cell, Index goal, Heuristic heuristic, Work& work) {
            if (heuristic == Heuristic::zero) {
                charge(work);
                return 0;
            }
            Index total = 0;
            for (std::size_t axis = 0; axis < domain.dimensions(); ++axis) {
                charge(work);
                const auto here = (cell / domain.strides()[axis]) % domain.extents()[axis];
                const auto there = (goal / domain.strides()[axis]) % domain.extents()[axis];
                total = add(total, here > there ? here - there : there - here);
            }
            return total;
        }
    }

    MazeView maze_view(const Maze& maze) {
        return {maze.domain(), maze.passages()};
    }

    Result<TraversalRequirements> route_requirements(const Domain& domain, Index queue_entries) {
        try {
            return route_storage(domain, queue_entries);
        } catch (const TraversalDetail::Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }

    Result<SearchResult> find_route(MazeView graph, Index entrance, Index goal, Heuristic heuristic, SearchLimits limits) {
        using namespace TraversalDetail;
        try {
            header(graph, entrance, goal);
            if (heuristic != Heuristic::zero && heuristic != Heuristic::manhattan) {
                invalid(Reason::heuristic);
            }
            limits.budget = effective(limits.budget, search_hard_limits.budget);
            limits.expansions = std::min(limits.expansions, search_hard_limits.expansions);
            limits.queue_entries = std::min(limits.queue_entries, search_hard_limits.queue_entries);
            limits.queue_pushes = std::min(limits.queue_pushes, search_hard_limits.queue_pushes);
            const auto needed = route_storage(graph.domain, limits.queue_entries);
            admit(graph.domain, needed, limits.budget);
            if (limits.expansions == 0) {
                refused(Reason::expansions);
            }
            if (limits.queue_entries == 0) {
                refused(Reason::queue_entries);
            }
            if (limits.queue_pushes == 0) {
                refused(Reason::queue_pushes);
            }
            Work work(limits.budget.work);
            static_cast<void>(validate(graph, limits.budget, work));
            charge(work, graph.domain.cells());
            const auto size = static_cast<std::size_t>(graph.domain.cells());
            std::vector<Record> records(size);
            std::vector<Entry> queue;
            queue.reserve(static_cast<std::size_t>(limits.queue_entries));
            std::vector<Index> path;
            path.reserve(size);
            SearchUsage usage;
            const auto push = [&](Entry entry) {
                if (usage.pushes == limits.queue_pushes) {
                    refused(Reason::queue_pushes);
                }
                if (queue.size() == limits.queue_entries) {
                    refused(Reason::queue_entries);
                }
                charge(work, 1 + static_cast<Index>(std::bit_width(queue.size() + 1)));
                queue.push_back(entry);
                std::push_heap(queue.begin(), queue.end(), Later{});
                ++usage.pushes;
                usage.peak_queue_entries = std::max(usage.peak_queue_entries, static_cast<Index>(queue.size()));
            };
            records[static_cast<std::size_t>(entrance)] = Record{0, entrance, State::open};
            push({estimate(graph.domain, entrance, goal, heuristic, work), 0, entrance});
            while (!queue.empty()) {
                charge(work, 1 + 2 * static_cast<Index>(std::bit_width(queue.size())));
                std::pop_heap(queue.begin(), queue.end(), Later{});
                const auto current = queue.back();
                queue.pop_back();
                ++usage.pops;
                auto& record = records[static_cast<std::size_t>(current.cell)];
                if (record.state == State::settled || record.cost != current.cost) {
                    ++usage.stale_pops;
                    continue;
                }
                if (usage.expansions == limits.expansions) {
                    refused(Reason::expansions);
                }
                charge(work);
                ++usage.expansions;
                record.state = State::settled;
                if (current.cell == goal) {
                    Index cell = goal;
                    while (true) {
                        charge(work);
                        if (path.size() == size) {
                            throw std::logic_error("A* parent chain exceeds the validated cell count");
                        }
                        path.push_back(cell);
                        if (cell == entrance) {
                            break;
                        }
                        cell = records[static_cast<std::size_t>(cell)].parent;
                    }
                    charge(work, path.size() / 2);
                    std::reverse(path.begin(), path.end());
                    usage.work = work.used();
                    return SearchResult{Path{std::move(path), current.cost}, usage};
                }
                for (std::size_t direction = 0; direction < 2 * graph.domain.dimensions(); ++direction) {
                    charge(work);
                    const auto edge = arc(graph, current.cell, direction);
                    if (!edge) {
                        continue;
                    }
                    ++usage.inspected_edges;
                    auto& next = records[static_cast<std::size_t>(edge->to)];
                    // Built-in consistency makes settled costs final. No arbitrary h/cost callback is accepted.
                    if (next.state == State::settled) {
                        continue;
                    }
                    const auto cost = add(current.cost, edge->cost);
                    if (next.state == State::unseen || cost < next.cost) {
                        const auto priority = add(cost, estimate(graph.domain, edge->to, goal, heuristic, work));
                        push({priority, cost, edge->to});
                        next = Record{cost, current.cell, State::open};
                    }
                }
            }
            usage.work = work.used();
            return SearchResult{std::nullopt, usage};
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
}
