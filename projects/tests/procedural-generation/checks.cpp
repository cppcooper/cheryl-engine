#include "checks.h"

#include <numeric>
#include <stdexcept>

namespace ProcGenChecks {
    namespace {
        void require(bool valid, const char* message) {
            if (!valid) {
                throw std::runtime_error(message);
            }
        }

        ProcGen::Coordinate decode(ProcGen::Index index, std::span<const ProcGen::Index> extents) {
            ProcGen::Coordinate coordinate{};
            for (std::size_t axis = 0; axis < extents.size(); ++axis) {
                coordinate[axis] = index % extents[axis];
                index /= extents[axis];
            }
            require(index == 0, "independent coordinate decode overflow");
            return coordinate;
        }

        ProcGen::Index encode(const ProcGen::Coordinate& coordinate, std::span<const ProcGen::Index> extents) {
            ProcGen::Index index = 0;
            for (auto axis = extents.size(); axis-- > 0;) {
                index = index * extents[axis] + coordinate[axis];
            }
            return index;
        }
    }

    Topology verify(const ProcGen::Maze& maze) {
        const auto extents = maze.domain().extents();
        ProcGen::Index cells = 1;
        for (const auto extent : extents) {
            require(extent > 0 && extent <= 4096 && cells <= 4096 / extent, "validator cell cap exceeded");
            cells *= extent;
        }
        require(!extents.empty() && extents.size() <= ProcGen::max_dimensions, "invalid dimension count");
        require(cells == maze.domain().cells() && cells == maze.passages().size(), "cell count mismatch");
        const auto size = static_cast<std::size_t>(cells);
        std::vector<std::vector<std::size_t>> adjacency(size);
        std::vector<std::size_t> parents(size);
        std::iota(parents.begin(), parents.end(), std::size_t{0});
        const auto root = [&](std::size_t cell) {
            while (parents[cell] != cell) {
                cell = parents[cell];
            }
            return cell;
        };
        Topology report{cells, 0, 0, {}};
        for (std::size_t cell = 0; cell < size; ++cell) {
            const auto coordinate = decode(cell, extents);
            require(encode(coordinate, extents) == cell, "independent coordinate round trip failed");
            require(maze.domain().coordinates(cell).value() == coordinate, "domain coordinates mismatch");
            require(maze.domain().index({coordinate.data(), extents.size()}).value() == cell, "domain index mismatch");
            const auto bits = maze.passages()[cell];
            if (extents.size() < ProcGen::max_dimensions) {
                require((bits >> (2 * extents.size())) == 0, "passage on inactive axis");
            }
            for (std::size_t axis = 0; axis < extents.size(); ++axis) {
                for (std::uint32_t sign = 0; sign < 2; ++sign) {
                    const bool present = (bits & (std::uint32_t{1} << (2 * axis + sign))) != 0;
                    auto next_coordinate = coordinate;
                    const bool in_bounds = sign == 0 ? coordinate[axis] != 0 : coordinate[axis] < extents[axis] - 1;
                    const auto grid_neighbor = maze.domain().neighbor(cell, axis, static_cast<ProcGen::Direction>(sign)).value();
                    const auto passage_neighbor = maze.neighbor(cell, axis, static_cast<ProcGen::Direction>(sign)).value();
                    require(grid_neighbor.has_value() == in_bounds, "domain boundary mismatch");
                    require(passage_neighbor.has_value() == present, "passage query mismatch");
                    if (!in_bounds) {
                        require(!present, "passage escapes boundary");
                        continue;
                    }
                    if (sign == 0) {
                        --next_coordinate[axis];
                    } else {
                        ++next_coordinate[axis];
                    }
                    const auto next = static_cast<std::size_t>(encode(next_coordinate, extents));
                    require(next < size && grid_neighbor == next, "axis step changes incorrect coordinate");
                    if (!present) {
                        continue;
                    }
                    require(passage_neighbor == next, "passage endpoint mismatch");
                    const auto opposite = std::uint32_t{1} << (2 * axis + (sign ^ 1U));
                    require((maze.passages()[next] & opposite) != 0, "nonreciprocal passage");
                    adjacency[cell].push_back(next);
                    if (sign == 1) {
                        ++report.edges;
                        ++report.axis_edges[axis];
                        const auto left_root = root(cell);
                        const auto right_root = root(next);
                        require(left_root != right_root, "cycle detected by independent edge union");
                        parents[left_root] = right_root;
                    }
                }
            }
        }
        require(report.edges == cells - 1, "perfect-maze edge count mismatch");
        std::vector<bool> visited(size, false);
        std::vector<std::size_t> queue{0};
        visited[0] = true;
        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
            const auto cell = queue[cursor];
            for (const auto next : adjacency[cell]) {
                if (!visited[next]) {
                    visited[next] = true;
                    queue.push_back(next);
                }
            }
        }
        report.reached = queue.size();
        require(report.reached == cells, "disconnected maze");
        return report;
    }
}
