#include "settlement-sites.h"
#include "elevation-checks.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace ProcGen;
    void check(bool ok) { if (!ok) { throw std::runtime_error("generated settlement contract failed"); } }
    void report(const Elevation& field, SettlementOptions options) {
        const auto requirements = settlement_requirements(field.descriptor().domain, options).value();
        const CaveBudgets exact{requirements.cells, requirements.storage_bytes, requirements.work};
        const auto sites = select_settlement_sites(field, options, exact).value();
        const auto again = select_settlement_sites(field, options, settlement_hard_limits).value();
        check(std::equal(sites.sites().begin(), sites.sites().end(), again.sites().begin(), again.sites().end()));
        check(sites.work() == requirements.work && sites.storage_bytes() == requirements.storage_bytes);
        const auto width = field.descriptor().domain.extents()[0];
        const auto height = field.descriptor().domain.extents()[1];
        std::cout << "settlement_sites v1; actual Elevation lattice_bilinear; domain=" << width << 'x' << height
                  << "; sea=" << options.sea_level << "; maximum_slope=" << options.maximum_slope
                  << "; minimum_manhattan_spacing=" << options.minimum_spacing << "; requested=" << sites.requested()
                  << "; achieved=" << sites.achieved() << "; shortfall=" << sites.shortfall()
                  << "; cells=" << requirements.cells << "; accounted_data_bytes=" << sites.storage_bytes()
                  << "; selection_work=" << sites.work() << "; elevation_work=" << field.usage().work << '\n';
        for (const auto& site : sites.sites()) {
            check(site.x > 0 && site.x < width-1 && site.y > 0 && site.y < height-1);
            check(site.index == site.x + width*site.y && site.height > options.sea_level);
            check(site.height == field.height(site.index).value() && site.slope <= options.maximum_slope);
            // Independent signed64 slope observation, not a selector helper.
            Index slope = 0;
            for (const auto neighbor : {site.index-1, site.index+1, site.index-width, site.index+width}) {
                const auto delta = static_cast<std::int64_t>(site.height) - field.height(neighbor).value();
                slope = std::max(slope, static_cast<Index>(delta < 0 ? -delta : delta));
            }
            check(slope == site.slope);
            for (const auto& prior : sites.sites()) {
                if (prior.index == site.index) { break; }
                const auto dx = site.x > prior.x ? site.x-prior.x : prior.x-site.x;
                const auto dy = site.y > prior.y ? site.y-prior.y : prior.y-site.y;
                check(dx+dy >= options.minimum_spacing);
            }
            std::cout << "  index=" << site.index << "; x=" << site.x << "; y=" << site.y
                      << "; height=" << site.height << "; max_cardinal_slope=" << site.slope << '\n';
        }
        if (options.sea_level == -81) { check(sites.achieved() > 0); }
        if (options.sea_level == 120) { check(sites.achieved() == 0 && sites.shortfall() == sites.requested()); }
    }
}
int main() {
    try {
        const std::array<ProcGen::Index,2> extents{17,11};
        const auto field = ProcGen::generate_elevation(extents, {42,0x67656f6772617068,7}, {3,-80,120}, ProcGen::elevation_hard_limits).value();
        ProcGenElevationChecks::inspect(field);
        const auto before = std::vector<ProcGen::Height>(field.heights().begin(), field.heights().end());
        report(field, {8,-81,UINT32_MAX,4});
        report(field, {8,0,UINT32_MAX,4});
        report(field, {8,120,UINT32_MAX,4});
        check(std::equal(before.begin(),before.end(),field.heights().begin(),field.heights().end()));
        std::cout << "Settlement consumer: 3 passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Settlement consumer failure: " << error.what() << '\n';
        return 1;
    }
}
