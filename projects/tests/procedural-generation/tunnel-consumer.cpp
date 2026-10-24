#include "tunnels.h"
#include "tunnel-checks.h"

#include <iostream>
#include <stdexcept>

namespace {
    void report(ProcGen::OccupancyView input, ProcGen::CaveBudgets budget) {
        const auto joined = ProcGen::connect_cave(input, budget).value();
        ProcGenTunnelChecks::inspect(input, joined);
        const auto m = joined.measurements();
        const auto clearance = ProcGen::analyze_clearance({joined.descriptor().domain, joined.occupancy()}, 1, budget).value();
        std::cout << "  transform=" << ProcGen::tunnel_algorithm << "; version=" << joined.descriptor().version
                  << "; domain=" << input.domain.extents()[0] << "x" << input.domain.extents()[1]
                  << "; index=x+width*y; exact_source_bytes=" << joined.source().size() << "; original_floor=" << m.original_floor
                  << "; original_components=" << m.original_components << "; carved=" << m.carved << "; attachment_routes=" << m.connections
                  << "; route_edges=" << m.route_length << "; final_floor=" << m.floor << "; final_components=" << m.components
                  << "; nonempty_connected=" << m.connected << "; work=" << joined.usage().work
                  << "; peak_heap=" << joined.usage().peak_queue
                  << "; fresh_clearance_minimum=1; usable=" << clearance.measurements().usable
                  << "; fresh_usable_regions=" << clearance.measurements().components << "; independent_cardinal_checks=PASS\n";
    }
}
int main() {
    unsigned passed = 0;
    try {
        const ProcGen::CaveBudgets budget{4096, 1024 * 1024, 8'000'000};
        const auto domain = ProcGen::Domain::checked(std::array<ProcGen::Index, 2>{7, 3}).value();
        const std::array<std::uint8_t, 21> source{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1};
        std::cout << "Source literal two-chamber7x3; solid perimeter; no RNG\n";
        report({domain, source}, budget);
        ++passed;
        const auto cave =
            ProcGen::generate_cave(std::array<ProcGen::Index, 2>{17, 11}, {42, 0x63617665, 7}, {.solid_fill = 6500, .steps = 0}, budget)
                .value();
        std::cout << "Source cave_cellular_2d v1; seed=42; stage=0x63617665; stream=7; fill=6500; steps=0; retain_components\n";
        report({cave.domain(), cave.occupancy()}, budget);
        ++passed;
        std::cout << "Tunnel consumer: 2 passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Tunnel consumer failure: " << error.what() << "\nTunnel consumer: " << passed << " passed, 1 failed, 0 skipped.\n";
        return 1;
    }
}
