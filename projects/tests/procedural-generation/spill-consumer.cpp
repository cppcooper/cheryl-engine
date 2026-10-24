#include "spill-checks.h"

#include <iostream>
#include <stdexcept>

namespace {
    void report(std::string_view label, ProcGen::DrainageInput input, const ProcGen::Spill& output, ProcGen::Index start) {
        ProcGenSpillChecks::inspect(input, output);
        const auto& d = output.descriptor();
        const auto m = output.measurements();
        const auto u = output.usage();
        std::cout << label << ": priority_flood v" << d.options.version << "; domain=" << d.domain.extents()[0] << 'x'
                  << d.domain.extents()[1] << "; index=x+width*y; original height ticks/datum; outlets=" << m.outlets
                  << "; cells=" << m.cells << "; filled_cells=" << m.filled_cells << "; max_depth=" << m.maximum_depth
                  << "; volume_cell_height_ticks=" << m.volume << "; effective=" << m.minimum_effective << ".." << m.maximum_effective
                  << "; pushes/pops=" << u.queue_pushes << '/' << u.queue_pops << "; peak_frontier=" << u.peak_queue << "; work=" << u.work
                  << '\n';
        for (ProcGen::Index i = 0; i < m.cells; ++i) {
            if (m.cells > 21 && i >= 4 && i + 2 < m.cells) {
                continue;
            } // sample only; complete field checked
            const auto c = output.cell(i).value();
            std::cout << "  cell=" << i << "; original=" << c.original << "; effective=" << c.effective << "; depth=" << c.depth
                      << "; parent=";
            if (c.parent == ProcGen::no_receiver) {
                std::cout << "none";
            } else {
                std::cout << c.parent;
            }
            std::cout << "; rank=" << c.rank << "; outlet=" << c.outlet << '\n';
        }
        std::cout << "  concrete outlet path: ";
        auto cursor = start;
        bool finished = false;
        for (ProcGen::Index steps = 0; steps < m.cells; ++steps) {
            const auto c = output.cell(cursor).value();
            std::cout << cursor << "[h=" << c.effective << ",rank=" << c.rank << ']';
            if (c.parent == ProcGen::no_receiver) {
                finished = true;
                break;
            }
            std::cout << " -> ";
            cursor = c.parent;
        }
        if (!finished) {
            throw std::logic_error("Spill consumer path failed to terminate");
        }
        std::cout << "; complete threshold/parent witness=PASS\n";
    }
}
int main() {
    unsigned passed = 0;
    try {
        const auto budget = ProcGen::spill_hard_limits;
        const auto nested_domain = ProcGen::Domain::checked(std::array<ProcGen::Index, 2>{7, 3}).value();
        const std::array<ProcGen::Height, 21> nested{9, 9, 9, 9, 9, 9, 9, 0, 6, -2, 3, -4, 8, 1, 9, 9, 9, 9, 9, 9, 9};
        const std::array<ProcGen::Index, 2> outlets{7, 13};
        const ProcGen::DrainageInput input{nested_domain, nested, outlets};
        const auto bowls = ProcGen::analyze_spill(input, budget).value();
        report("nested bowls: escape saddle6 competes with saddle8", input, bowls, 11);
        ++passed;
        const auto saddle_domain = ProcGen::Domain::checked(std::array<ProcGen::Index, 2>{5, 1}).value();
        const std::array<ProcGen::Height, 5> saddle{0, 5, -3, 5, 0};
        const std::array<ProcGen::Index, 2> ends{0, 4};
        const ProcGen::DrainageInput tied{saddle_domain, saddle, ends};
        const auto competing = ProcGen::analyze_spill(tied, budget).value();
        report("equal saddles: stable tie forest", tied, competing, 2);
        ++passed;
        const auto field =
            ProcGen::generate_elevation(
                std::array<ProcGen::Index, 2>{17, 11}, {42, 0x67656f6772617068, 7}, {3, -80, 120}, ProcGen::elevation_hard_limits
            )
                .value();
        std::cout << "U8 source: lattice_bilinear v1, key{42,0x67656f6772617068,7}, pitch3, requested-80..120; explicit outlets{0,186}\n";
        const std::array<ProcGen::Index, 2> chosen{0, 186};
        const ProcGen::DrainageInput generated{field.descriptor().domain, field.heights(), chosen};
        const auto terrain = ProcGen::analyze_spill(generated, budget).value();
        ProcGen::Index deepest = 0;
        for (ProcGen::Index i = 1; i < terrain.measurements().cells; ++i) {
            if (terrain.cell(i)->depth > terrain.cell(deepest)->depth) {
                deepest = i;
            }
        }
        report("generated: six sample cells, deepest-cell path", generated, terrain, deepest);
        ++passed;
        std::cout << "Spill consumer: " << passed << " passed, 0 failed, 0 skipped; heights unchanged, no physical water simulation.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Spill consumer failure: " << error.what() << "\nSpill consumer: " << passed << " passed, 1 failed, 0 skipped.\n";
        return 1;
    }
}
