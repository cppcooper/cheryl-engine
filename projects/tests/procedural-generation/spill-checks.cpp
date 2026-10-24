#include "spill-checks.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ProcGenSpillChecks {
    void inspect(ProcGen::DrainageInput input, const ProcGen::Spill& output) {
        using namespace ProcGen;
        const auto require = [](bool valid) {
            if (!valid) {
                throw std::runtime_error("Independent threshold/forest spill oracle mismatch");
            }
        };
        const auto& d = input.domain;
        require(d.dimensions() == 2 && d.cells() <= 512 && input.heights.size() == d.cells() && !input.outlets.empty());
        const auto count = d.cells(), width = d.extents()[0], height = d.extents()[1];
        require(output.descriptor().domain == d && output.descriptor().options == SpillOptions{} && output.cells().size() == count);
        std::vector<Height> levels(input.heights.begin(), input.heights.end());
        std::ranges::sort(levels);
        levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
        std::vector<Height> expected(static_cast<std::size_t>(count));
        std::vector<bool> assigned(static_cast<std::size_t>(count), false);
        for (auto level : levels) {
            std::vector<bool> seen(static_cast<std::size_t>(count), false);
            std::vector<Index> queue;
            for (auto outlet : input.outlets) {
                if (input.heights[static_cast<std::size_t>(outlet)] <= level) {
                    seen[static_cast<std::size_t>(outlet)] = true;
                    queue.push_back(outlet);
                }
            }
            for (std::size_t head = 0; head < queue.size(); ++head) {
                const auto cell = queue[head], x = cell % width, y = cell / width;
                const auto visit = [&](Index candidate) {
                    if (!seen[static_cast<std::size_t>(candidate)] && input.heights[static_cast<std::size_t>(candidate)] <= level) {
                        seen[static_cast<std::size_t>(candidate)] = true;
                        queue.push_back(candidate);
                    }
                };
                if (x > 0) {
                    visit(cell - 1);
                }
                if (x + 1 < width) {
                    visit(cell + 1);
                }
                if (y > 0) {
                    visit(cell - width);
                }
                if (y + 1 < height) {
                    visit(cell + width);
                }
            }
            for (Index i = 0; i < count; ++i) {
                if (seen[static_cast<std::size_t>(i)] && !assigned[static_cast<std::size_t>(i)]) {
                    expected[static_cast<std::size_t>(i)] = level;
                    assigned[static_cast<std::size_t>(i)] = true;
                }
            }
        }
        SpillMeasurements measured{};
        measured.cells = count;
        measured.outlets = input.outlets.size();
        measured.minimum_effective = std::numeric_limits<Height>::max();
        measured.maximum_effective = std::numeric_limits<Height>::min();
        std::vector<bool> ranks(static_cast<std::size_t>(count), false);
        for (Index i = 0; i < count; ++i) {
            const auto c = output.cell(i).value();
            require(assigned[static_cast<std::size_t>(i)] && c.original == input.heights[static_cast<std::size_t>(i)]);
            require(c.effective == expected[static_cast<std::size_t>(i)] && c.effective >= c.original);
            require(c.depth == static_cast<Index>(static_cast<std::int64_t>(c.effective) - c.original));
            require(c.rank < count && !ranks[static_cast<std::size_t>(c.rank)]);
            ranks[static_cast<std::size_t>(c.rank)] = true;
            const auto is_outlet = std::ranges::find(input.outlets, i) != input.outlets.end();
            require((c.parent == no_receiver) == is_outlet);
            if (is_outlet) {
                require(c.outlet == i && c.depth == 0);
            } else {
                require(c.parent < count);
                const auto p = output.cell(c.parent).value();
                const auto x = i % width, y = i / width, px = c.parent % width, py = c.parent / width;
                require((x > px ? x - px : px - x) + (y > py ? y - py : py - y) == 1);
                require(p.rank < c.rank && p.effective <= c.effective && p.outlet == c.outlet);
            }
            Index cursor = i, steps = 0;
            Height highest = c.original;
            while (true) {
                require(cursor < count && ++steps <= count);
                const auto next = output.cell(cursor).value();
                highest = std::max(highest, next.original);
                if (next.parent == no_receiver) {
                    require(std::ranges::find(input.outlets, cursor) != input.outlets.end() && c.outlet == cursor);
                    break;
                }
                cursor = next.parent;
            }
            require(highest == c.effective); // concrete path witnesses that escape level
            measured.filled_cells += static_cast<Index>(c.depth > 0);
            measured.maximum_depth = std::max(measured.maximum_depth, c.depth);
            measured.volume += c.depth;
            measured.minimum_effective = std::min(measured.minimum_effective, c.effective);
            measured.maximum_effective = std::max(measured.maximum_effective, c.effective);
        }
        require(measured == output.measurements());
        require(output.usage().queue_pushes == count && output.usage().queue_pops == count);
    }
}
