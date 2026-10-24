#include "cave-checks.h"

#include <algorithm>
#include <stdexcept>

namespace ProcGenCaveChecks {
    Report inspect(const ProcGen::Cave& cave) {
        const auto& domain = cave.domain();
        if (domain.dimensions() != 2 || domain.cells() > 4096 || cave.occupancy().size() != domain.cells()) {
            throw std::runtime_error("Cave oracle domain/capacity mismatch");
        }
        const auto width = domain.extents()[0], height = domain.extents()[1];
        Report report;
        const auto occupancy = cave.occupancy();
        for (ProcGen::Index cell = 0; cell < domain.cells(); ++cell) {
            const auto x = cell % width, y = cell / width;
            if (occupancy[static_cast<std::size_t>(cell)] > 1 ||
                ((x == 0 || y == 0 || x + 1 == width || y + 1 == height) && occupancy[static_cast<std::size_t>(cell)] != 1)) {
                throw std::runtime_error("Cave occupancy/perimeter invariant failed");
            }
            const std::array<ProcGen::Index, 2> coordinate{x, y};
            const auto decoded = domain.coordinates(cell).value();
            if (decoded[0] != x || decoded[1] != y || domain.index(coordinate).value() != cell) {
                throw std::runtime_error("Cave row/coordinate convention mismatch");
            }
            if (occupancy[static_cast<std::size_t>(cell)] == 0) {
                ++report.floor;
            } else {
                ++report.solid;
            }
        }
        std::vector<bool> seen(occupancy.size(), false);
        for (ProcGen::Index first = 0; first < domain.cells(); ++first) {
            if (occupancy[static_cast<std::size_t>(first)] != 0 || seen[static_cast<std::size_t>(first)]) {
                continue;
            }
            std::vector<ProcGen::Index> component{first};
            seen[static_cast<std::size_t>(first)] = true;
            for (std::size_t head = 0; head < component.size(); ++head) {
                const auto a = component[head];
                for (ProcGen::Index b = 0; b < domain.cells(); ++b) {
                    if (occupancy[static_cast<std::size_t>(b)] != 0 || seen[static_cast<std::size_t>(b)]) {
                        continue;
                    }
                    const auto dx = std::max(a % width, b % width) - std::min(a % width, b % width);
                    const auto dy = std::max(a / width, b / width) - std::min(a / width, b / width);
                    if (dx + dy == 1) {
                        seen[static_cast<std::size_t>(b)] = true;
                        component.push_back(b);
                    }
                }
            }
            report.components.push_back(std::move(component));
        }
        return report;
    }
}
