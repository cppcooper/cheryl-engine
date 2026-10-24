#include "cave.h"
#include "cave-checks.h"

#include <exception>
#include <iostream>

namespace {
    bool demonstrate(
        std::span<const ProcGen::Index> extents,
        ProcGen::StreamKey key,
        ProcGen::CaveConfiguration config,
        ProcGen::CaveBudgets budgets
    ) {
        const auto cave = ProcGen::generate_cave(extents, key, config, budgets);
        if (!cave) {
            std::cerr << "Cave refused: " << ProcGen::reason_text(cave.error().reason) << '\n';
            return false;
        }
        const auto checked = ProcGenCaveChecks::inspect(*cave);
        const auto measured = cave->measurements();
        if (checked.floor != measured.floor || checked.solid != measured.solid || checked.components.size() != measured.components ||
            measured.connected != (checked.components.size() == 1)) {
            std::cerr << "Cave metric verification failed\n";
            return false;
        }
        const auto required = ProcGen::cave_requirements(cave->domain(), config).value();
        std::cout << "Cave 2D; width=" << extents[0] << "; height=" << extents[1]
                  << "; index=x+width*y; floor=0; solid=1; perimeter=solid\n"
                  << "  algorithm=cave_cellular_2d; version=" << cave->descriptor().version << "; seed=" << key.seed
                  << "; stage=" << key.stage << "; stream=" << key.stream << "; requested/effective solid_fill=" << config.solid_fill << "/"
                  << ProcGen::cave_fill_scale << "; steps=" << config.steps << "; birth_mask=" << config.birth
                  << "; survival_mask=" << config.survival << "; smoothing=simultaneous_Moore8; traversal=cardinal4\n"
                  << "  initial_interior_solid=" << measured.initial_interior_solid << "/" << required.interior_cells
                  << "; generated_floor=" << measured.generated_floor << "; generated_components=" << measured.generated_components
                  << "; floor=" << measured.floor << "; solid=" << measured.solid << "; achieved_floor_density=" << measured.floor << "/"
                  << cave->domain().cells() << "; components=" << measured.components << "; removed_floor=" << measured.removed_floor
                  << "; largest_floor=" << measured.largest_floor << '\n'
                  << "  policy=" << (measured.connectivity_required ? "largest_floor" : "retain_components")
                  << "; connectivity_required=" << (measured.connectivity_required ? "yes" : "no")
                  << "; achieved_nonempty_connected=" << (measured.connected ? "yes" : "no")
                  << "; requirement_met=" << (!measured.connectivity_required || measured.connected ? "yes" : "no")
                  << "; work=" << cave->usage().work << "; raw_draws=" << cave->usage().random_draws
                  << "; boundary/counts/independent_BFS=PASS\n";
        return true;
    }
}
int main() {
    try {
        const std::array<ProcGen::Index, 2> extents{32, 24};
        const ProcGen::StreamKey key{42, 0x63617665, 7};
        const ProcGen::CaveBudgets budgets{4096, 128 * 1024, 2'000'000};
        const ProcGen::CaveConfiguration a{
            .solid_fill = 4200, .steps = 3, .birth = 0x1e0, .survival = 0x1f0, .connectivity = ProcGen::CaveConnectivity::retain_components
        };
        const ProcGen::CaveConfiguration b{
            .solid_fill = 5200, .steps = 4, .birth = 0x1c0, .survival = 0x1f0, .connectivity = ProcGen::CaveConnectivity::largest_floor
        };
        const bool first = demonstrate(extents, key, a, budgets);
        const bool second = demonstrate(extents, key, b, budgets);
        std::cout << "Cave consumer: " << (first + second) << " passed, " << (!first + !second) << " failed, 0 skipped.\n";
        return first && second ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Cave consumer exception: " << error.what() << '\n';
        return 1;
    }
}
