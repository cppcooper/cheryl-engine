#include "tunnels.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ProcGen {
    namespace {
        struct Stop {
            Failure failure;
        };
        [[noreturn]] void fail(FailureKind kind, Reason reason) {
            throw Stop{{kind, reason}};
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b) {
                fail(FailureKind::invalid_input, Reason::overflow);
            }
            return a + b;
        }
        Index multiply(Index a, Index b) {
            if (b != 0 && a > std::numeric_limits<Index>::max() / b) {
                fail(FailureKind::invalid_input, Reason::overflow);
            }
            return a * b;
        }
        void charge(Work& work, Index count = 1) {
            if (!work.consume(count)) {
                fail(FailureKind::budget_refused, Reason::work);
            }
        }
        struct Entry {
            Index cost, cell;
        };
        static_assert(sizeof(Entry) == 16 && sizeof(TunnelConnection) == 32);
        static_assert(2 * sizeof(ConnectedCave) + 2 * sizeof(TunnelDescriptor) + 1024 <= fixed_storage_allowance);
        bool boundary(const Domain& d, Index cell) {
            const auto w = d.extents()[0], h = d.extents()[1];
            const auto x = cell % w, y = cell / w;
            return x == 0 || y == 0 || x == w - 1 || y == h - 1;
        }
        TunnelRequirements needed_for(const Domain& domain) {
            if (domain.dimensions() != 2) {
                fail(FailureKind::invalid_input, Reason::tunnel_dimensions);
            }
            const auto cells = domain.cells(), entries = multiply(5, cells);
            if (cells > std::vector<Index>{}.max_size() || cells > std::vector<std::uint8_t>{}.max_size() ||
                cells > std::vector<TunnelConnection>{}.max_size() || entries > std::vector<Entry>{}.max_size()) {
                fail(FailureKind::invalid_input, Reason::overflow);
            }
            return {cells, add(fixed_storage_allowance, multiply(139, cells)), multiply(8, cells), entries};
        }
    }
    Result<TunnelRequirements> tunnel_requirements(const Domain& domain) {
        try {
            return needed_for(domain);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
    ConnectedCave::ConnectedCave(
        TunnelDescriptor descriptor,
        std::vector<std::uint8_t> source,
        std::vector<std::uint8_t> occupancy,
        std::vector<TunnelConnection> connections,
        TunnelMeasurements measured,
        TunnelUsage usage
    )
    : descriptor_(std::move(descriptor)),
      source_(std::move(source)),
      occupancy_(std::move(occupancy)),
      connections_(std::move(connections)),
      measured_(measured),
      usage_(usage) {}

    Result<ConnectedCave> connect_cave(OccupancyView input, CaveBudgets budget, std::uint32_t version) {
        try {
            if (version != tunnel_version) {
                fail(FailureKind::unsupported, Reason::tunnel_version);
            }
            const auto& domain = input.domain;
            const auto needed = needed_for(domain);
            if (input.occupancy.size() != needed.cells) {
                fail(FailureKind::invalid_input, Reason::occupancy);
            }
            budget = {
                std::min(budget.cells, tunnel_hard_limits.cells), std::min(budget.storage_bytes, tunnel_hard_limits.storage_bytes),
                std::min(budget.work, tunnel_hard_limits.work)
            };
            if (needed.cells > budget.cells) {
                fail(FailureKind::budget_refused, Reason::cells);
            }
            if (needed.storage_bytes > budget.storage_bytes) {
                fail(FailureKind::budget_refused, Reason::storage);
            }
            if (needed.minimum_work > budget.work) {
                fail(FailureKind::budget_refused, Reason::work);
            }
            Work work(budget.work);
            TunnelUsage usage;
            TunnelMeasurements measured;
            Index root = needed.cells;
            for (Index cell = 0; cell < needed.cells; ++cell) {
                charge(work);
                const auto value = input.occupancy[static_cast<std::size_t>(cell)];
                if (value > 1) {
                    fail(FailureKind::invalid_input, Reason::occupancy);
                }
                if (value == 0) {
                    if (boundary(domain, cell)) {
                        fail(FailureKind::contradiction, Reason::tunnel_boundary);
                    }
                    ++measured.original_floor;
                    root = std::min(root, cell);
                }
            }
            charge(work, multiply(6, needed.cells));
            const auto size = static_cast<std::size_t>(needed.cells);
            std::vector<std::uint8_t> source(input.occupancy.begin(), input.occupancy.end()), output = source, flags(size, 0);
            std::vector<Index> distance(size, 0), parent(size, 0), queue(size, 0);
            std::vector<Entry> heap;
            heap.reserve(static_cast<std::size_t>(needed.pushes_per_search));
            std::vector<TunnelConnection> connections;
            connections.reserve(size);
            const auto flood = [&](Index first, std::uint8_t mark) {
                Index head = 0, tail = 0;
                const auto visit = [&](Index cell) {
                    charge(work);
                    if (tail >= needed.cells) {
                        throw std::logic_error("Tunnel flood queue invariant");
                    }
                    flags[static_cast<std::size_t>(cell)] = mark;
                    queue[static_cast<std::size_t>(tail++)] = cell;
                };
                visit(first);
                while (head < tail) {
                    charge(work);
                    const auto cell = queue[static_cast<std::size_t>(head++)];
                    for (std::size_t direction = 0; direction < 4; ++direction) {
                        charge(work);
                        const auto next = domain.neighbor(cell, direction / 2, static_cast<Direction>(direction % 2)).value();
                        if (next && output[static_cast<std::size_t>(*next)] == 0 && flags[static_cast<std::size_t>(*next)] == 0) {
                            visit(*next);
                        }
                    }
                }
                return tail;
            };
            Index reached = 0;
            for (Index cell = 0; cell < needed.cells; ++cell) {
                charge(work);
                if (output[static_cast<std::size_t>(cell)] == 0 && flags[static_cast<std::size_t>(cell)] == 0) {
                    const auto count = flood(cell, static_cast<std::uint8_t>(cell == root ? 1 : 4));
                    ++measured.original_components;
                    if (cell == root) {
                        reached = count;
                    }
                }
            }
            measured.floor = measured.original_floor;
            const auto worse = [&](Entry a, Entry b) {
                charge(work); // ACTUAL heap comparisons, including stale entries
                return a.cost > b.cost || (a.cost == b.cost && a.cell > b.cell);
            };
            constexpr auto infinity = std::numeric_limits<Index>::max();
            while (reached < measured.floor) {
                charge(work, static_cast<Index>(heap.size()));
                heap.clear();
                charge(work, multiply(2, needed.cells));
                std::fill(distance.begin(), distance.end(), infinity);
                std::fill(parent.begin(), parent.end(), infinity);
                for (auto& flag : flags) {
                    charge(work);
                    flag &= 1U;
                }
                Index pushes = 0;
                const auto push = [&](Index cell, Index cost) {
                    charge(work);
                    if (++pushes > needed.pushes_per_search || heap.size() >= needed.pushes_per_search) {
                        throw std::logic_error("Tunnel heap capacity/push invariant");
                    }
                    heap.push_back({cost, cell});
                    std::push_heap(heap.begin(), heap.end(), worse);
                    ++usage.pushes;
                    usage.peak_queue = std::max(usage.peak_queue, static_cast<Index>(heap.size()));
                };
                for (Index cell = 0; cell < needed.cells; ++cell) {
                    charge(work);
                    if ((flags[static_cast<std::size_t>(cell)] & 1U) != 0) {
                        distance[static_cast<std::size_t>(cell)] = 0;
                        parent[static_cast<std::size_t>(cell)] = cell;
                        push(cell, 0);
                    }
                }
                Index target = infinity, target_cost = 0;
                while (!heap.empty()) {
                    charge(work);
                    std::pop_heap(heap.begin(), heap.end(), worse);
                    const auto entry = heap.back();
                    heap.pop_back();
                    ++usage.pops;
                    const auto cell = entry.cell;
                    if ((flags[static_cast<std::size_t>(cell)] & 2U) != 0 || entry.cost != distance[static_cast<std::size_t>(cell)]) {
                        ++usage.stale_pops;
                        continue;
                    }
                    flags[static_cast<std::size_t>(cell)] |= 2U;
                    if (output[static_cast<std::size_t>(cell)] == 0 && (flags[static_cast<std::size_t>(cell)] & 1U) == 0) {
                        target = cell;
                        target_cost = entry.cost;
                        break;
                    }
                    for (std::size_t direction = 0; direction < 4; ++direction) {
                        charge(work);
                        const auto next = domain.neighbor(cell, direction / 2, static_cast<Direction>(direction % 2)).value();
                        if (!next || boundary(domain, *next) || (flags[static_cast<std::size_t>(*next)] & 2U) != 0) {
                            continue;
                        }
                        const auto cost = add(entry.cost, output[static_cast<std::size_t>(*next)]);
                        if (cost < distance[static_cast<std::size_t>(*next)]) {
                            distance[static_cast<std::size_t>(*next)] = cost;
                            parent[static_cast<std::size_t>(*next)] = cell;
                            push(*next, cost);
                        }
                    }
                }
                if (target == infinity) {
                    throw std::logic_error("Carveable rectangular interior disconnected");
                }
                TunnelConnection connection{target, target, 0, 0};
                while ((flags[static_cast<std::size_t>(connection.source)] & 1U) == 0) {
                    charge(work);
                    if (connection.length >= needed.cells || parent[static_cast<std::size_t>(connection.source)] == infinity) {
                        throw std::logic_error("Tunnel reconstruction invariant");
                    }
                    auto& cell = output[static_cast<std::size_t>(connection.source)];
                    connection.carved += cell;
                    cell = 0;
                    connection.source = parent[static_cast<std::size_t>(connection.source)];
                    ++connection.length;
                }
                if (connection.carved != target_cost || connection.carved == 0 || connections.size() >= size) {
                    throw std::logic_error("Tunnel least-carve attachment invariant");
                }
                charge(work);
                connections.push_back(connection);
                measured.carved = add(measured.carved, connection.carved);
                measured.floor = add(measured.floor, connection.carved);
                measured.route_length = add(measured.route_length, connection.length);
                ++measured.connections;
                for (auto& flag : flags) {
                    charge(work);
                    flag = 0;
                }
                const auto previous = reached;
                reached = flood(root, 1);
                if (reached <= previous || measured.connections >= measured.original_components) {
                    throw std::logic_error("Tunnel component progress invariant");
                }
            }
            measured.components = measured.floor == 0 ? 0 : 1;
            measured.connected = measured.floor != 0;
            charge(work, static_cast<Index>(heap.size())); // retire remaining queued candidates
            usage.work = work.used();
            return ConnectedCave({domain, version}, std::move(source), std::move(output), std::move(connections), measured, usage);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
}
