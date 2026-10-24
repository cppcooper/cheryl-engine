#include "drainage-checks.h"

#include <algorithm>
#include <stdexcept>

namespace ProcGenDrainageChecks {
    void inspect(ProcGen::DrainageInput input, const ProcGen::Drainage& output) {
        using namespace ProcGen;
        const auto require = [](bool valid) {
            if (!valid) {
                throw std::runtime_error("Independent drainage chain oracle mismatch");
            }
        };
        const auto& d = input.domain;
        const auto count = d.cells(), width = d.extents()[0];
        require(d.dimensions() == 2 && count <= 1024 && input.heights.size() == count);
        require(output.descriptor().domain == d && output.descriptor().options == DrainageOptions{});
        require(output.cells().size() == count && std::ranges::equal(output.source_heights(), input.heights));
        std::vector<DrainageCell> expected(static_cast<std::size_t>(count));
        for (auto& c : expected) {
            c.contributing = 0;
        }
        for (Index cell = 0; cell < count; ++cell) {
            auto& c = expected[static_cast<std::size_t>(cell)];
            if (std::ranges::find(input.outlets, cell) != input.outlets.end()) {
                c.kind = TerminalKind::outlet;
                continue;
            }
            for (Index candidate = 0; candidate < count; ++candidate) {
                const auto x = cell % width, y = cell / width, cx = candidate % width, cy = candidate / width;
                const auto dx = x > cx ? x - cx : cx - x, dy = y > cy ? y - cy : cy - y;
                if (dx + dy != 1 || input.heights[static_cast<std::size_t>(candidate)] >= input.heights[static_cast<std::size_t>(cell)]) {
                    continue;
                }
                // Ascending candidate order resolves equal LOWEST neighbor heights.
                if (c.receiver == no_receiver ||
                    input.heights[static_cast<std::size_t>(candidate)] < input.heights[static_cast<std::size_t>(c.receiver)]) {
                    c.receiver = candidate;
                }
            }
            if (c.receiver == no_receiver) {
                c.kind = TerminalKind::sink;
            }
        }
        for (Index start = 0; start < count; ++start) {
            Index cursor = start, steps = 0;
            while (true) {
                require(cursor < count && ++steps <= count);
                auto& c = expected[static_cast<std::size_t>(cursor)];
                ++c.contributing; // each source contributes exactly once along its chain
                if (c.receiver == no_receiver) {
                    expected[static_cast<std::size_t>(start)].terminal = cursor;
                    break;
                }
                cursor = c.receiver;
            }
        }
        DrainageMeasurements m{};
        m.cells = count;
        for (Index cell = 0; cell < count; ++cell) {
            const auto& c = expected[static_cast<std::size_t>(cell)];
            require(output.cell(cell).value() == c);
            m.maximum_contributing = std::max(m.maximum_contributing, c.contributing);
            if (c.receiver != no_receiver) {
                ++m.links;
            } else {
                m.largest_catchment = std::max(m.largest_catchment, c.contributing);
                if (c.kind == TerminalKind::outlet) {
                    ++m.outlets;
                    m.outlet_contributing += c.contributing;
                } else {
                    ++m.sinks;
                    m.sink_contributing += c.contributing;
                }
            }
        }
        require(m == output.measurements() && m.outlet_contributing + m.sink_contributing == count);
    }
}
