#include "clearance-checks.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace ProcGenClearanceChecks {
    ProcGen::ClearanceMeasurements inspect(ProcGen::OccupancyView input, const ProcGen::Clearance& field) {
        const auto& domain = input.domain;
        const auto cells = domain.cells();
        if (domain.dimensions() != 2 || cells > 4096 || input.occupancy.size() != cells || field.descriptor().domain != domain ||
            field.distances().size() != cells || field.usable().size() != cells || field.descriptor().version != 1) {
            throw std::runtime_error("Clearance oracle shape/version mismatch");
        }
        const auto width = domain.extents()[0], height = domain.extents()[1];
        const auto separation = [&](ProcGen::Index a, ProcGen::Index b) {
            return std::max(a % width, b % width) - std::min(a % width, b % width) + std::max(a / width, b / width) -
                   std::min(a / width, b / width);
        };
        std::vector<std::uint8_t> selected(static_cast<std::size_t>(cells), 0);
        ProcGen::ClearanceMeasurements measured;
        for (ProcGen::Index cell = 0; cell < cells; ++cell) {
            if (input.occupancy[static_cast<std::size_t>(cell)] > 1) {
                throw std::runtime_error("Clearance oracle nonbinary input");
            }
            ProcGen::Index distance = 0;
            if (input.occupancy[static_cast<std::size_t>(cell)] == 0) {
                ++measured.floor;
                const auto x = cell % width, y = cell / width;
                distance = std::min({x + 1, width - x, y + 1, height - y});
                for (ProcGen::Index solid = 0; solid < cells; ++solid) {
                    if (input.occupancy[static_cast<std::size_t>(solid)] == 1) {
                        distance = std::min(distance, separation(cell, solid));
                    }
                }
            } else {
                ++measured.solid;
            }
            const auto usable =
                static_cast<std::uint8_t>(input.occupancy[static_cast<std::size_t>(cell)] == 0 && distance >= field.descriptor().minimum);
            if (field.distances()[static_cast<std::size_t>(cell)] != distance || field.usable()[static_cast<std::size_t>(cell)] != usable) {
                throw std::runtime_error("Clearance distance/mask mismatch at cell " + std::to_string(cell));
            }
            selected[static_cast<std::size_t>(cell)] = usable;
            measured.usable += usable;
            measured.maximum = std::max(measured.maximum, distance);
        }
        std::vector<bool> seen(static_cast<std::size_t>(cells), false);
        for (ProcGen::Index first = 0; first < cells; ++first) {
            if (selected[static_cast<std::size_t>(first)] == 0 || seen[static_cast<std::size_t>(first)]) {
                continue;
            }
            std::vector<ProcGen::Index> queue{first};
            seen[static_cast<std::size_t>(first)] = true;
            for (std::size_t head = 0; head < queue.size(); ++head) {
                const auto here = queue[head];
                for (ProcGen::Index there = 0; there < cells; ++there) {
                    if (selected[static_cast<std::size_t>(there)] != 0 && !seen[static_cast<std::size_t>(there)] &&
                        separation(here, there) == 1) {
                        seen[static_cast<std::size_t>(there)] = true;
                        queue.push_back(there);
                    }
                }
            }
            ++measured.components;
            measured.largest_region = std::max(measured.largest_region, static_cast<ProcGen::Index>(queue.size()));
        }
        if (measured != field.measurements()) {
            throw std::runtime_error("Clearance summary/region mismatch");
        }
        return measured;
    }
}
