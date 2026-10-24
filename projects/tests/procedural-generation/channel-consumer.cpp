#include "channel-checks.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
    void report(ProcGen::ChannelInput input, const ProcGen::Channels& result) {
        ProcGenChannelChecks::inspect(input, result);
        const auto m = result.measurements();
        std::cout << "threshold_runs v" << result.descriptor().options.version
                  << "; source=ranked_effective_forest; domain=" << input.domain.extents()[0] << 'x' << input.domain.extents()[1]
                  << "; index=x+width*y; threshold=" << result.descriptor().minimum_contributing << "; selected=" << m.selected_cells
                  << "; links=" << m.selected_links << "; through_fill=" << m.selected_filled_cells << "; nodes=" << m.nodes
                  << "; edges=" << m.edges << "; path_points=" << m.path_points << "; terminal_units=" << m.terminal_contributing
                  << "; work=" << result.usage().work << "; payload=" << result.usage().storage_bytes << '\n';
        for (auto outlet : input.outlets) {
            std::cout << "  explicit outlet=" << outlet << "; recomputed_units=" << result.contributing()[static_cast<std::size_t>(outlet)]
                      << "; selected=" << static_cast<unsigned>(result.mask()[static_cast<std::size_t>(outlet)]) << '\n';
        }
        const auto shown = std::min<std::size_t>(3, result.edges().size());
        for (std::size_t i = 0; i < shown; ++i) {
            const auto e = result.edges()[i];
            std::cout << "  edge=" << i << "; nodes=" << e.from << "->" << e.to << "; fill_points_inclusive=" << e.fill_points
                      << "; actual receiver path=";
            const auto path = result.path_cells().subspan(static_cast<std::size_t>(e.offset), static_cast<std::size_t>(e.count));
            for (std::size_t k = 0; k < path.size(); ++k) {
                if (k > 0) {
                    std::cout << " -> ";
                }
                const auto cell = path[k];
                std::cout << cell;
                if ((result.mask()[static_cast<std::size_t>(cell)] & ProcGen::channel_through_fill) != 0) {
                    std::cout << "[fill]";
                }
            }
            std::cout << '\n';
        }
        std::cout << "  complete trace/count/threshold/network-link coverage=PASS; abstract cells, not discharge/width/navigation\n";
    }
}
int main() {
    unsigned passed = 0;
    try {
        const auto budget = ProcGen::channel_hard_limits;
        const auto d = ProcGen::Domain::checked(std::array<ProcGen::Index, 2>{5, 1}).value();
        const std::array<ProcGen::Height, 5> heights{5, 0, 0, 0, 0};
        const std::array<ProcGen::Index, 1> outlets{0};
        const auto lake = ProcGen::analyze_spill({d, heights, outlets}, ProcGen::spill_hard_limits).value();
        const ProcGen::ChannelInput literal{d, heights, lake.cells(), outlets};
        const auto crossing = ProcGen::extract_channels(literal, 2, budget).value();
        std::cout << "Literal fill crossing: original{5,0,0,0,0}, effective{5,5,5,5,5}, priority_flood v1, explicit outlet0\n";
        report(literal, crossing);
        ++passed;
        const auto terrain =
            ProcGen::generate_elevation(
                std::array<ProcGen::Index, 2>{17, 11}, {42, 0x67656f6772617068, 7}, {3, -80, 120}, ProcGen::elevation_hard_limits
            )
                .value();
        const std::array<ProcGen::Index, 2> chosen{0, 186};
        const auto routing =
            ProcGen::analyze_spill({terrain.descriptor().domain, terrain.heights(), chosen}, ProcGen::spill_hard_limits).value();
        const ProcGen::ChannelInput generated{terrain.descriptor().domain, terrain.heights(), routing.cells(), chosen};
        std::cout << "U8 lattice_bilinear v1 key{42,0x67656f6772617068,7},17x11,pitch3,requested-80..120 -> U10 priority_flood v"
                  << routing.descriptor().options.version << " -> fresh accumulation and channels, explicit outlets{0,186}\n";
        ProcGen::Index previous = 188;
        for (ProcGen::Index threshold : {8U, 24U, 188U}) {
            const auto rivers = ProcGen::extract_channels(generated, threshold, budget).value();
            report(generated, rivers);
            if (rivers.measurements().selected_cells > previous) {
                throw std::logic_error("Fixed-forest threshold count increased");
            }
            previous = rivers.measurements().selected_cells;
            ++passed;
        }
        std::cout << "Channel consumer: " << passed << " passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Channel consumer failure: " << error.what() << "\nChannel consumer: " << passed << " passed, 1 failed, 0 skipped.\n";
        return 1;
    }
}
