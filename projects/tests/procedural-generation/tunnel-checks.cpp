#include "tunnel-checks.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ProcGenTunnelChecks {
    namespace {
        using namespace ProcGen;
        void require(bool ok) {
            if (!ok) {
                throw std::runtime_error("Tunnel independent oracle mismatch");
            }
        }
        bool adjacent(Index a, Index b, Index w) {
            return std::max(a % w, b % w) - std::min(a % w, b % w) + std::max(a / w, b / w) - std::min(a / w, b / w) == 1;
        }
    }
    Counts count(OccupancyView input) {
        require(input.domain.dimensions() == 2 && input.domain.cells() <= 4096 && input.domain.cells() == input.occupancy.size());
        const auto w = input.domain.extents()[0], h = input.domain.extents()[1];
        Counts report;
        std::vector<bool> seen(input.occupancy.size(), false);
        for (Index cell = 0; cell < input.domain.cells(); ++cell) {
            const auto value = input.occupancy[static_cast<std::size_t>(cell)];
            require(value <= 1);
            if (cell % w == 0 || cell / w == 0 || cell % w == w - 1 || cell / w == h - 1) {
                require(value == 1);
            }
            report.floor += static_cast<Index>(value == 0);
            if (value != 0 || seen[static_cast<std::size_t>(cell)]) {
                continue;
            }
            std::vector<Index> component{cell};
            seen[static_cast<std::size_t>(cell)] = true;
            for (std::size_t head = 0; head < component.size(); ++head) {
                for (Index b = 0; b < input.domain.cells(); ++b) {
                    if (input.occupancy[static_cast<std::size_t>(b)] == 0 && !seen[static_cast<std::size_t>(b)] &&
                        adjacent(component[head], b, w)) {
                        seen[static_cast<std::size_t>(b)] = true;
                        component.push_back(b);
                    }
                }
            }
            report.components.push_back(std::move(component));
        }
        return report;
    }
    void inspect(OccupancyView input, const ConnectedCave& result) {
        require(result.descriptor().domain == input.domain && result.descriptor().version == tunnel_version);
        require(std::ranges::equal(result.source(), input.occupancy));
        const auto before = count(input), after = count({result.descriptor().domain, result.occupancy()});
        Index carved = 0;
        for (std::size_t i = 0; i < input.occupancy.size(); ++i) {
            require(input.occupancy[i] != 0 || result.occupancy()[i] == 0);
            carved += static_cast<Index>(input.occupancy[i] == 1 && result.occupancy()[i] == 0);
        }
        const auto m = result.measurements();
        require(m.original_floor == before.floor && m.original_components == before.components.size());
        require(m.floor == after.floor && m.components == after.components.size());
        require(m.carved == carved && m.floor == m.original_floor + carved && m.connected == (after.components.size() == 1));
        require(after.components.size() == (before.floor == 0 ? 0U : 1U));
        require(m.connections == result.connections().size() && (before.components.empty() || m.connections < before.components.size()));
        Index sum_cost = 0, sum_length = 0;
        for (const auto connection : result.connections()) {
            require(connection.source < input.domain.cells() && connection.target < input.domain.cells());
            require(result.occupancy()[static_cast<std::size_t>(connection.source)] == 0);
            require(input.occupancy[static_cast<std::size_t>(connection.target)] == 0);
            require(connection.carved > 0 && connection.length == connection.carved + 1);
            sum_cost += connection.carved;
            sum_length += connection.length;
        }
        require(sum_cost == carved && sum_length == m.route_length);
        require(result.usage().pushes <= 5 * input.domain.cells() * m.connections);
        require(result.usage().peak_queue <= 5 * input.domain.cells());
        require(result.usage().pops <= result.usage().pushes);
    }
    Index first_cost(OccupancyView input) {
        require(input.domain.cells() <= 256);
        const auto before = count(input);
        if (before.components.size() <= 1) {
            return 0;
        }
        const auto size = input.occupancy.size();
        constexpr auto infinity = std::numeric_limits<Index>::max();
        std::vector<Index> distance(size, infinity);
        std::vector<bool> done(size, false), source(size, false);
        for (const auto cell : before.components.front()) {
            distance[static_cast<std::size_t>(cell)] = 0;
            source[static_cast<std::size_t>(cell)] = true;
        }
        const auto w = input.domain.extents()[0], h = input.domain.extents()[1];
        for (std::size_t iteration = 0; iteration < size; ++iteration) {
            std::size_t selected = size;
            for (std::size_t i = 0; i < size; ++i) {
                if (!done[i] && distance[i] != infinity && (selected == size || distance[i] < distance[selected])) {
                    selected = i;
                }
            }
            require(selected != size);
            if (input.occupancy[selected] == 0 && !source[selected]) {
                return distance[selected];
            }
            done[selected] = true;
            for (std::size_t next = 0; next < size; ++next) {
                if (next % w == 0 || next / w == 0 || next % w == w - 1 || next / w == h - 1 || done[next]) {
                    continue;
                }
                if (adjacent(static_cast<Index>(selected), static_cast<Index>(next), w)) {
                    distance[next] = std::min(distance[next], distance[selected] + input.occupancy[next]);
                }
            }
        }
        throw std::runtime_error("Reference found no carveable interior attachment");
    }
}
