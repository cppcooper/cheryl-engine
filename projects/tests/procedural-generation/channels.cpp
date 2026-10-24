#include "channels.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ProcGen {
    namespace {
        struct Stop {
            ChannelFailure failure;
        };
        [[noreturn]] void fail(FailureKind kind, ChannelReason reason) {
            throw Stop{{kind, reason}};
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b) {
                fail(FailureKind::invalid_input, ChannelReason::overflow);
            }
            return a + b;
        }
        Index multiply(Index a, Index b) {
            if (b != 0 && a > std::numeric_limits<Index>::max() / b) {
                fail(FailureKind::invalid_input, ChannelReason::overflow);
            }
            return a * b;
        }
        void charge(Work& work, Index count = 1) {
            if (!work.consume(count)) {
                fail(FailureKind::budget_refused, ChannelReason::work);
            }
        }
        ChannelRequirements needed_for(const Domain& d, Index outlets) {
            if (d.dimensions() != 2) {
                fail(FailureKind::invalid_input, ChannelReason::dimensions);
            }
            const auto c = d.cells();
            if (outlets == 0 || outlets > c) {
                fail(FailureKind::invalid_input, ChannelReason::outlets);
            }
            if (c > std::vector<SpillCell>{}.max_size() || c > std::vector<Index>{}.max_size() ||
                c > std::vector<ChannelNode>{}.max_size() || c > std::vector<ChannelEdge>{}.max_size()) {
                fail(FailureKind::invalid_input, ChannelReason::overflow);
            }
            const auto base = add(drainage_fixed_storage, multiply(66, c));
            const auto max_points = multiply(2, c - outlets);
            if (max_points > std::vector<Index>{}.max_size()) {
                fail(FailureKind::invalid_input, ChannelReason::overflow);
            }
            return {
                c, base, add(base, add(multiply(16, c), add(multiply(40, c - outlets), multiply(8, max_points)))),
                add(1024, multiply(14, c) - outlets), add(1024, multiply(25, c) - multiply(9, outlets))
            };
        }
        static_assert(sizeof(SpillCell) == 40 && sizeof(ChannelNode) == 16 && sizeof(ChannelEdge) == 40 && sizeof(Index) == 8);
        static_assert(2 * sizeof(Channels) + 2 * sizeof(ChannelDescriptor) + 1024 <= fixed_storage_allowance);
    }
    std::string_view channel_reason_text(ChannelReason reason) {
        switch (reason) {
            case ChannelReason::dimensions:
                return "channels require a checked positive2D domain";
            case ChannelReason::shape:
                return "channel height/routing counts must equal domain cells";
            case ChannelReason::outlets:
                return "channel outlets must be nonempty distinct in-domain roots";
            case ChannelReason::source:
                return "incompatible source heights/effective/depth/outlet meaning";
            case ChannelReason::topology:
                return "invalid cardinal acyclic ranked outlet forest";
            case ChannelReason::threshold:
                return "contributing-cell threshold must be positive";
            case ChannelReason::overflow:
                return "channel index/payload/work arithmetic or capacity overflow";
            case ChannelReason::cells:
                return "channel cell budget refused";
            case ChannelReason::nodes:
                return "channel node budget refused";
            case ChannelReason::edges:
                return "channel edge budget refused";
            case ChannelReason::paths:
                return "channel path-point budget refused";
            case ChannelReason::storage:
                return "channel accounted payload budget refused";
            case ChannelReason::work:
                return "channel work budget refused or exhausted";
            case ChannelReason::algorithm:
                return "unsupported channel algorithm or source meaning";
            case ChannelReason::version:
                return "unsupported channel version";
        }
        return "unknown channel failure";
    }
    ChannelResult<ChannelRequirements> channel_requirements(const Domain& d, Index outlet_count) {
        try {
            return needed_for(d, outlet_count);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
    Channels::Channels(
        ChannelDescriptor descriptor,
        std::vector<SpillCell> routing,
        std::vector<Index> contributing,
        std::vector<std::uint8_t> mask,
        std::vector<ChannelNode> nodes,
        std::vector<ChannelEdge> edges,
        std::vector<Index> paths,
        ChannelMeasurements measured,
        ChannelUsage usage
    )
    : descriptor_(std::move(descriptor)),
      routing_(std::move(routing)),
      contributing_(std::move(contributing)),
      mask_(std::move(mask)),
      nodes_(std::move(nodes)),
      edges_(std::move(edges)),
      paths_(std::move(paths)),
      measured_(measured),
      usage_(usage) {}
    ChannelResult<Channels> extract_channels(ChannelInput input, Index threshold, ChannelBudgets budgets, ChannelOptions options) {
        try {
            if (options.algorithm != ChannelAlgorithm::threshold_runs || input.meaning != ChannelMeaning::ranked_effective_forest) {
                fail(FailureKind::unsupported, ChannelReason::algorithm);
            }
            if (options.version != 1) {
                fail(FailureKind::unsupported, ChannelReason::version);
            }
            if (threshold == 0) {
                fail(FailureKind::invalid_input, ChannelReason::threshold);
            }
            const auto& d = input.domain;
            const auto needed = needed_for(d, input.outlets.size());
            const ChannelBudgets cap{
                std::min(budgets.cells, channel_hard_limits.cells),
                std::min(budgets.nodes, channel_hard_limits.nodes),
                std::min(budgets.edges, channel_hard_limits.edges),
                std::min(budgets.path_points, channel_hard_limits.path_points),
                std::min(budgets.storage_bytes, channel_hard_limits.storage_bytes),
                std::min(budgets.work, channel_hard_limits.work)
            };
            if (needed.cells > cap.cells) {
                fail(FailureKind::budget_refused, ChannelReason::cells);
            }
            if (needed.base_storage_bytes > cap.storage_bytes) {
                fail(FailureKind::budget_refused, ChannelReason::storage);
            }
            if (needed.minimum_work > cap.work) {
                fail(FailureKind::budget_refused, ChannelReason::work);
            }
            if (input.heights.size() != needed.cells || input.routing.size() != needed.cells) {
                fail(FailureKind::invalid_input, ChannelReason::shape);
            }
            Work work(cap.work);
            charge(work, 1024);
            std::array<std::uint64_t, 1024> bits{};
            for (auto outlet : input.outlets) {
                charge(work);
                if (outlet >= needed.cells) {
                    fail(FailureKind::invalid_input, ChannelReason::outlets);
                }
                auto& word = bits[static_cast<std::size_t>(outlet / 64)];
                const auto bit = UINT64_C(1) << (outlet % 64);
                if ((word & bit) != 0) {
                    fail(FailureKind::invalid_input, ChannelReason::outlets);
                }
                word |= bit;
            }
            const auto declared = [&](Index i) { return (bits[static_cast<std::size_t>(i / 64)] & (UINT64_C(1) << (i % 64))) != 0; };
            charge(work, multiply(6, needed.cells));
            const auto size = static_cast<std::size_t>(needed.cells);
            std::vector<SpillCell> routing(input.routing.begin(), input.routing.end());
            std::vector<Index> contributing(size, 1), order(size, no_receiver), node_map(size, no_receiver);
            std::vector<std::uint8_t> mask(size, 0), incoming(size, 0);
            const auto width = d.extents()[0];
            for (Index i = 0; i < needed.cells; ++i) {
                charge(work);
                const auto& c = routing[static_cast<std::size_t>(i)];
                if (c.original != input.heights[static_cast<std::size_t>(i)] || c.effective < c.original ||
                    c.depth != static_cast<Index>(static_cast<std::int64_t>(c.effective) - c.original) || c.outlet >= needed.cells ||
                    !declared(c.outlet)) {
                    fail(FailureKind::invalid_input, ChannelReason::source);
                }
                if (c.rank >= needed.cells || order[static_cast<std::size_t>(c.rank)] != no_receiver) {
                    fail(FailureKind::invalid_input, ChannelReason::topology);
                }
                order[static_cast<std::size_t>(c.rank)] = i;
                if ((c.parent == no_receiver) != declared(i)) {
                    fail(FailureKind::invalid_input, ChannelReason::outlets);
                }
                if (c.parent == no_receiver) {
                    if (c.outlet != i || c.depth != 0) {
                        fail(FailureKind::invalid_input, ChannelReason::source);
                    }
                } else {
                    charge(work);
                    if (c.parent >= needed.cells) {
                        fail(FailureKind::invalid_input, ChannelReason::topology);
                    }
                    const auto& p = routing[static_cast<std::size_t>(c.parent)];
                    const auto x = i % width, y = i / width, px = c.parent % width, py = c.parent / width;
                    if ((x > px ? x - px : px - x) + (y > py ? y - py : py - y) != 1 || p.rank >= c.rank || p.effective > c.effective ||
                        p.outlet != c.outlet) {
                        fail(FailureKind::invalid_input, ChannelReason::topology);
                    }
                }
            }
            ChannelMeasurements m{};
            m.cells = needed.cells;
            m.outlets = input.outlets.size();
            for (Index position = needed.cells; position != 0; --position) {
                charge(work);
                const auto i = order[static_cast<std::size_t>(position - 1)], parent = routing[static_cast<std::size_t>(i)].parent;
                if (parent == no_receiver) {
                    m.terminal_contributing = add(m.terminal_contributing, contributing[static_cast<std::size_t>(i)]);
                } else {
                    charge(work);
                    auto& sum = contributing[static_cast<std::size_t>(parent)];
                    sum = add(sum, contributing[static_cast<std::size_t>(i)]);
                    if (sum > needed.cells) {
                        throw std::logic_error("Channel accumulation exceeds supplied units");
                    }
                }
            }
            if (m.terminal_contributing != needed.cells) {
                throw std::logic_error("Channel terminal conservation invariant failed");
            }
            for (Index i = 0; i < needed.cells; ++i) {
                charge(work);
                if (contributing[static_cast<std::size_t>(i)] >= threshold) {
                    mask[static_cast<std::size_t>(i)] = channel_selected;
                    ++m.selected_cells;
                    if (routing[static_cast<std::size_t>(i)].depth > 0) {
                        mask[static_cast<std::size_t>(i)] |= channel_through_fill;
                        ++m.selected_filled_cells;
                    }
                }
                incoming[static_cast<std::size_t>(i)] = 0;
            }
            for (Index i = 0; i < needed.cells; ++i) {
                charge(work);
                const auto parent = routing[static_cast<std::size_t>(i)].parent;
                if (mask[static_cast<std::size_t>(i)] != 0 && parent != no_receiver) {
                    charge(work);
                    if (mask[static_cast<std::size_t>(parent)] == 0) {
                        throw std::logic_error("Channel threshold lost downstream continuity");
                    }
                    ++m.selected_links;
                    ++incoming[static_cast<std::size_t>(parent)];
                }
            }
            for (Index i = 0; i < needed.cells; ++i) {
                charge(work);
                const auto parent = routing[static_cast<std::size_t>(i)].parent;
                if (mask[static_cast<std::size_t>(i)] != 0 && (incoming[static_cast<std::size_t>(i)] != 1 || parent == no_receiver)) {
                    node_map[static_cast<std::size_t>(i)] = m.nodes++;
                    m.edges += static_cast<Index>(parent != no_receiver);
                }
            }
            m.path_points = add(m.selected_links, m.edges);
            if (m.nodes > cap.nodes) {
                fail(FailureKind::budget_refused, ChannelReason::nodes);
            }
            if (m.edges > cap.edges) {
                fail(FailureKind::budget_refused, ChannelReason::edges);
            }
            if (m.path_points > cap.path_points) {
                fail(FailureKind::budget_refused, ChannelReason::paths);
            }
            const auto storage =
                add(needed.base_storage_bytes, add(multiply(16, m.nodes), add(multiply(40, m.edges), multiply(8, m.path_points))));
            if (storage > cap.storage_bytes) {
                fail(FailureKind::budget_refused, ChannelReason::storage);
            }
            charge(work, add(m.nodes, add(m.edges, m.path_points)));
            std::vector<ChannelNode> nodes(static_cast<std::size_t>(m.nodes));
            std::vector<ChannelEdge> edges(static_cast<std::size_t>(m.edges));
            std::vector<Index> paths(static_cast<std::size_t>(m.path_points), 0);
            for (Index i = 0; i < needed.cells; ++i) {
                charge(work);
                const auto node = node_map[static_cast<std::size_t>(i)];
                if (node == no_receiver) {
                    continue;
                }
                charge(work);
                const auto degree = incoming[static_cast<std::size_t>(i)];
                const auto roles = static_cast<std::uint8_t>(
                    (degree == 0 ? channel_source : 0) | (degree >= 2 ? channel_junction : 0) |
                    (routing[static_cast<std::size_t>(i)].parent == no_receiver ? channel_outlet : 0)
                );
                nodes[static_cast<std::size_t>(node)] = {i, roles};
            }
            Index edge_position = 0, path_position = 0;
            for (Index n = 0; n < m.nodes; ++n) {
                charge(work);
                auto current = nodes[static_cast<std::size_t>(n)].cell;
                if (routing[static_cast<std::size_t>(current)].parent == no_receiver) {
                    continue;
                }
                const auto offset = path_position;
                Index fills = 0;
                const auto append = [&](Index i) {
                    charge(work);
                    if (path_position >= m.path_points) {
                        throw std::logic_error("Channel path accounting invariant failed");
                    }
                    paths[static_cast<std::size_t>(path_position++)] = i;
                    fills += static_cast<Index>(routing[static_cast<std::size_t>(i)].depth > 0);
                };
                append(current);
                do {
                    charge(work);
                    current = routing[static_cast<std::size_t>(current)].parent;
                    append(current);
                } while (node_map[static_cast<std::size_t>(current)] == no_receiver);
                charge(work);
                if (edge_position >= m.edges) {
                    throw std::logic_error("Channel edge accounting invariant failed");
                }
                edges[static_cast<std::size_t>(edge_position++)] = {
                    n, node_map[static_cast<std::size_t>(current)], offset, path_position - offset, fills
                };
            }
            if (edge_position != m.edges || path_position != m.path_points) {
                throw std::logic_error("Channel coverage accounting invariant failed");
            }
            return Channels(
                {d, threshold, input.meaning, budgets, options}, std::move(routing), std::move(contributing), std::move(mask),
                std::move(nodes), std::move(edges), std::move(paths), m, {work.used(), storage}
            );
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
}
