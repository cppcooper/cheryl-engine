#include "elevation.h"
#include "elevation-checks.h"

#include <iostream>
#include <stdexcept>

namespace {
    void report(const ProcGen::Elevation& field, ProcGen::Height sea) {
        const auto m = field.measurements();
        const auto c = field.effective();
        const auto& r = field.descriptor();
        const auto needed = ProcGen::elevation_requirements(r.domain, c).value();
        const auto water = ProcGen::measure_sea(field, sea, r.domain.cells()).value();
        std::cout << "elevation_lattice_bilinear v" << r.version << "; domain=17x11; index=x+width*y; units=height_ticks; datum=0"
                  << "; seed=" << r.random.seed << "; stage=" << r.random.stage << "; stream=" << r.random.stream
                  << "; spacing=" << c.spacing << "; requested=" << c.low << ".." << c.high << "; observed=" << m.minimum << ".."
                  << m.maximum << "; range=" << m.range << "; anchors=" << needed.lattice_samples << "; sea_level=" << sea
                  << "; land=" << water.land << "; water=" << water.water << "; generation_work=" << field.usage().work
                  << "; raw_draws=" << field.usage().random_draws << "; analysis_work=" << water.work
                  << "; independent_integer_field=PASS\n";
    }
}
int main() {
    unsigned passed = 0;
    try {
        const std::array<ProcGen::Index, 2> extents{17, 11};
        const ProcGen::StreamKey key{42, 0x67656f6772617068, 7};
        const ProcGen::CaveBudgets budget{4096, 1024 * 1024, 2'000'000};
        const auto a = ProcGen::generate_elevation(extents, key, {3, -80, 120}, budget).value();
        ProcGenElevationChecks::inspect(a);
        report(a, 0);
        ++passed;
        report(a, 40);
        ++passed; // same owned heights, only analysis threshold changes
        const auto b = ProcGen::generate_elevation(extents, key, {6, -240, 360}, budget).value();
        ProcGenElevationChecks::inspect(b);
        report(b, 0);
        ++passed;
        std::cout << "Elevation consumer: " << passed << " passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Elevation consumer failure: " << error.what() << "\nElevation consumer: " << passed
                  << " passed, 1 failed, 0 skipped.\n";
        return 1;
    }
}
