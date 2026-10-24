#include "channel-checks.h"

#include <algorithm>
#include <stdexcept>

namespace ProcGenChannelChecks {
    void inspect(ProcGen::ChannelInput input, const ProcGen::Channels& output) {
        using namespace ProcGen;
        const auto require = [](bool valid) {
            if (!valid) {
                throw std::runtime_error("Independent channel trace/coverage oracle mismatch");
            }
        };
        const auto c = input.domain.cells();
        require(input.domain.dimensions() == 2 && c <= 512 && input.routing.size() == c && input.heights.size() == c);
        require(output.descriptor().domain == input.domain && output.descriptor().meaning == input.meaning);
        require(std::ranges::equal(output.routing(), input.routing));
        const auto width = input.domain.extents()[0], threshold = output.descriptor().minimum_contributing;
        std::vector<Index> units(static_cast<std::size_t>(c), 0);
        for (Index start = 0; start < c; ++start) {
            auto current = start;
            Index steps = 0;
            while (true) {
                require(current < c && ++steps <= c);
                ++units[static_cast<std::size_t>(current)];
                const auto& record = input.routing[static_cast<std::size_t>(current)];
                require(record.original == input.heights[static_cast<std::size_t>(current)]);
                if (record.parent == no_receiver) {
                    require(
                        std::ranges::find(input.outlets, current) != input.outlets.end() &&
                        input.routing[static_cast<std::size_t>(start)].outlet == current
                    );
                    break;
                }
                require(record.parent < c);
                const auto x = current % width, y = current / width, px = record.parent % width, py = record.parent / width;
                require((x > px ? x - px : px - x) + (y > py ? y - py : py - y) == 1);
                current = record.parent;
            }
        }
        require(std::ranges::equal(output.contributing(), units));
        std::vector<std::uint8_t> flags(static_cast<std::size_t>(c), 0);
        std::vector<Index> degree(static_cast<std::size_t>(c), 0), map(static_cast<std::size_t>(c), no_receiver);
        ChannelMeasurements m{};
        m.cells = c;
        m.outlets = input.outlets.size();
        for (Index i = 0; i < c; ++i) {
            const auto& r = input.routing[static_cast<std::size_t>(i)];
            if (r.parent == no_receiver) {
                m.terminal_contributing += units[static_cast<std::size_t>(i)];
            }
            if (units[static_cast<std::size_t>(i)] < threshold) {
                continue;
            }
            flags[static_cast<std::size_t>(i)] = r.depth > 0 ? 3 : 1;
            ++m.selected_cells;
            m.selected_filled_cells += static_cast<Index>(r.depth > 0);
            if (r.parent != no_receiver) {
                ++degree[static_cast<std::size_t>(r.parent)];
                ++m.selected_links;
            }
            auto cursor = i;
            for (Index steps = 0;; ++steps) {
                require(steps < c && units[static_cast<std::size_t>(cursor)] >= threshold);
                if (input.routing[static_cast<std::size_t>(cursor)].parent == no_receiver) {
                    break;
                }
                cursor = input.routing[static_cast<std::size_t>(cursor)].parent;
            }
        }
        require(std::ranges::equal(output.mask(), flags));
        std::vector<ChannelNode> expected_nodes;
        for (Index i = 0; i < c; ++i) {
            if (flags[static_cast<std::size_t>(i)] == 0) {
                continue;
            }
            const auto incoming = degree[static_cast<std::size_t>(i)];
            const auto terminal = input.routing[static_cast<std::size_t>(i)].parent == no_receiver;
            if (incoming == 1 && !terminal) {
                continue;
            }
            const auto role = static_cast<std::uint8_t>((incoming == 0 ? 1 : 0) | (incoming >= 2 ? 2 : 0) | (terminal ? 4 : 0));
            map[static_cast<std::size_t>(i)] = expected_nodes.size();
            expected_nodes.push_back({i, role});
            m.edges += static_cast<Index>(!terminal);
        }
        m.nodes = expected_nodes.size();
        m.path_points = m.selected_links + m.edges;
        require(
            std::ranges::equal(output.nodes(), expected_nodes) && output.edges().size() == m.edges &&
            output.path_cells().size() == m.path_points
        );
        std::vector<Index> link_uses(static_cast<std::size_t>(c), 0);
        std::vector<bool> covered(static_cast<std::size_t>(c), false);
        for (const auto n : output.nodes()) {
            covered[static_cast<std::size_t>(n.cell)] = true;
        }
        Index offset = 0, previous_source = 0;
        bool first = true;
        for (const auto edge : output.edges()) {
            require(edge.from < m.nodes && edge.to < m.nodes && edge.count >= 2 && edge.offset == offset);
            require(first || edge.from > previous_source);
            first = false;
            previous_source = edge.from;
            require(edge.count <= m.path_points - offset);
            const auto path = output.path_cells().subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(edge.count));
            require(
                path.front() == output.nodes()[static_cast<std::size_t>(edge.from)].cell &&
                path.back() == output.nodes()[static_cast<std::size_t>(edge.to)].cell
            );
            Index filled = 0;
            for (std::size_t k = 0; k < path.size(); ++k) {
                const auto cell = path[k];
                require(cell < c && flags[static_cast<std::size_t>(cell)] != 0);
                covered[static_cast<std::size_t>(cell)] = true;
                filled += static_cast<Index>(input.routing[static_cast<std::size_t>(cell)].depth > 0);
                if (k > 0) {
                    require(input.routing[static_cast<std::size_t>(path[k - 1])].parent == cell);
                    ++link_uses[static_cast<std::size_t>(path[k - 1])];
                }
                if (k > 0 && k + 1 < path.size()) {
                    require(map[static_cast<std::size_t>(cell)] == no_receiver);
                }
            }
            require(filled == edge.fill_points);
            offset += edge.count;
        }
        for (Index i = 0; i < c; ++i) {
            require(covered[static_cast<std::size_t>(i)] == (flags[static_cast<std::size_t>(i)] != 0));
            require(
                link_uses[static_cast<std::size_t>(i)] ==
                static_cast<Index>(
                    flags[static_cast<std::size_t>(i)] != 0 && input.routing[static_cast<std::size_t>(i)].parent != no_receiver
                )
            );
        }
        require(offset == m.path_points && m == output.measurements() && m.terminal_contributing == c);
    }
}
