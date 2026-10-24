#include "dungeon-checks.h"

#include <algorithm>
#include <stdexcept>

namespace ProcGenDungeonChecks {
    namespace {
        using namespace ProcGen;
        void require(bool ok) {
            if (!ok) {
                throw std::runtime_error("Independent dungeon rectangle/corridor/occupancy oracle mismatch");
            }
        }
        Index difference(Index a, Index b) {
            return std::max(a, b) - std::min(a, b);
        }
        std::array<Index, 2> center(const DungeonRoom& r) {
            return {(r.upper[0] - r.lower[0] - 1) / 2 + r.lower[0], (r.upper[1] - r.lower[1] - 1) / 2 + r.lower[1]};
        }
        bool segment(std::array<Index, 2> cell, std::array<Index, 2> a, std::array<Index, 2> b) {
            return cell[0] >= std::min(a[0], b[0]) && cell[0] <= std::max(a[0], b[0]) && cell[1] >= std::min(a[1], b[1]) &&
                   cell[1] <= std::max(a[1], b[1]);
        }
    }
    Report inspect(const Dungeon& dungeon) {
        const auto& d = dungeon.descriptor().domain;
        require(d.dimensions() == 2 && d.cells() <= 4096 && dungeon.occupancy().size() == d.cells());
        const auto& c = dungeon.descriptor().requested;
        const auto w = d.extents()[0], h = d.extents()[1];
        const auto rooms = dungeon.rooms();
        const auto connections = dungeon.connections();
        const auto m = dungeon.measurements();
        require(c == dungeon.effective() && m.requested == c.rooms && m.achieved == rooms.size());
        require(m.attempts <= c.attempts && m.achieved + m.fit_rejections + m.overlap_rejections == m.attempts);
        require((m.achievement == DungeonAchievement::complete) == (rooms.size() == c.rooms));
        if (m.achievement == DungeonAchievement::attempt_limit) {
            require(m.attempts == c.attempts);
        }
        std::vector<std::uint8_t> expected(dungeon.occupancy().size(), 1);
        Index room_floor = 0, corridor_floor = 0;
        for (std::size_t i = 0; i < rooms.size(); ++i) {
            const auto& r = rooms[i];
            require(r.id == i && c.margin <= w && c.margin <= h);
            for (std::size_t axis = 0; axis < 2; ++axis) {
                require(r.lower[axis] < r.upper[axis] && r.lower[axis] >= c.margin && r.upper[axis] <= d.extents()[axis] - c.margin);
                require(r.upper[axis] - r.lower[axis] >= c.minimum[axis] && r.upper[axis] - r.lower[axis] <= c.maximum[axis]);
            }
            for (std::size_t j = 0; j < i; ++j) {
                Index cell_distance = 0;
                for (std::size_t axis = 0; axis < 2; ++axis) {
                    const auto lo = std::max(r.lower[axis], rooms[j].lower[axis]);
                    const auto hi = std::min(r.upper[axis] - 1, rooms[j].upper[axis] - 1);
                    if (lo > hi) {
                        cell_distance = std::max(cell_distance, lo - hi);
                    }
                }
                require(cell_distance > c.spacing); // min Chebyshev cell distance
            }
            for (Index cell = 0; cell < d.cells(); ++cell) {
                const auto x = cell % w, y = cell / w;
                if (x >= r.lower[0] && x < r.upper[0] && y >= r.lower[1] && y < r.upper[1]) {
                    require(expected[static_cast<std::size_t>(cell)] == 1);
                    expected[static_cast<std::size_t>(cell)] = 0;
                    ++room_floor;
                }
            }
        }
        require(connections.size() == (rooms.empty() ? 0 : rooms.size() - 1));
        for (std::size_t i = 0; i < connections.size(); ++i) {
            const auto& edge = connections[i];
            require(edge.from == i && edge.to == i + 1 && edge.first_axis <= 1);
            const auto a = center(rooms[i]), b = center(rooms[i + 1]);
            const std::array<Index, 2> bend = edge.first_axis == 0 ? std::array<Index, 2>{b[0], a[1]} : std::array<Index, 2>{a[0], b[1]};
            require(edge.length == difference(a[0], b[0]) + difference(a[1], b[1]));
            Index carved = 0;
            for (Index cell = 0; cell < d.cells(); ++cell) {
                if (segment({cell % w, cell / w}, a, bend) || segment({cell % w, cell / w}, bend, b)) {
                    carved += expected[static_cast<std::size_t>(cell)];
                    expected[static_cast<std::size_t>(cell)] = 0;
                    require(dungeon.occupancy()[static_cast<std::size_t>(cell)] == 0);
                }
            }
            require(carved == edge.carved);
            corridor_floor += carved;
        }
        require(std::ranges::equal(expected, dungeon.occupancy()));
        Report report;
        std::vector<bool> seen(expected.size(), false);
        for (Index cell = 0; cell < d.cells(); ++cell) {
            const auto x = cell % w, y = cell / w;
            if (x == 0 || y == 0 || x == w - 1 || y == h - 1) {
                require(expected[static_cast<std::size_t>(cell)] == 1);
            }
            if (expected[static_cast<std::size_t>(cell)] != 0) {
                continue;
            }
            require(c.margin <= w && c.margin <= h && x >= c.margin && x < w - c.margin && y >= c.margin && y < h - c.margin);
            ++report.floor;
            if (seen[static_cast<std::size_t>(cell)]) {
                continue;
            }
            ++report.components;
            std::vector<Index> pending{cell};
            seen[static_cast<std::size_t>(cell)] = true;
            for (std::size_t head = 0; head < pending.size(); ++head) {
                for (Index next = 0; next < d.cells(); ++next) {
                    if (!seen[static_cast<std::size_t>(next)] && expected[static_cast<std::size_t>(next)] == 0 &&
                        difference(next % w, pending[head] % w) + difference(next / w, pending[head] / w) == 1) {
                        seen[static_cast<std::size_t>(next)] = true;
                        pending.push_back(next);
                    }
                }
            }
        }
        require(report.components == (rooms.empty() ? 0U : 1U));
        require(
            m.room_floor == room_floor && m.corridor_floor == corridor_floor && m.floor == report.floor &&
            m.solid == d.cells() - report.floor
        );
        require(m.connected == (report.components == 1));
        return report;
    }
}
