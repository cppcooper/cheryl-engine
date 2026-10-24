#include "drainage-checks.h"

#include <iostream>
#include <stdexcept>

namespace {
    std::string_view kind(ProcGen::TerminalKind value) {
        switch (value) {
            case ProcGen::TerminalKind::none:
                return "flow";
            case ProcGen::TerminalKind::outlet:
                return "outlet";
            case ProcGen::TerminalKind::sink:
                return "sink";
        }
        throw std::logic_error("Unknown terminal kind");
    }
    void report(std::string_view label, ProcGen::DrainageInput input, const ProcGen::Drainage& result) {
        ProcGenDrainageChecks::inspect(input, result);
        const auto m = result.measurements();
        const auto& d = result.descriptor();
        std::cout << label << ": steepest_cardinal v" << d.options.version << "; domain=" << d.domain.extents()[0] << 'x'
                  << d.domain.extents()[1] << "; index=x+width*y; original_height_ticks/datum; exact_owned_input_witness; cells=" << m.cells
                  << "; links=" << m.links << "; outlets=" << m.outlets << "; sinks=" << m.sinks
                  << "; outlet_units=" << m.outlet_contributing << "; sink_units=" << m.sink_contributing
                  << "; largest_catchment=" << m.largest_catchment << "; work=" << result.work() << '\n';
        for (ProcGen::Index i = 0; i < m.cells; ++i) {
            if (m.cells > 6 && i >= 4 && i + 2 < m.cells) {
                continue;
            } // report sample; oracle checks complete field
            const auto c = result.cell(i).value();
            std::cout << "  cell=" << i << "; height=" << result.source_heights()[static_cast<std::size_t>(i)] << "; receiver=";
            if (c.receiver == ProcGen::no_receiver) {
                std::cout << "none";
            } else {
                std::cout << c.receiver;
            }
            std::cout << "; kind=" << kind(c.kind) << "; catchment/terminal=" << c.terminal << " (" << kind(result.cell(c.terminal)->kind)
                      << "); contributing_units=" << c.contributing << '\n';
        }
        std::cout << "  complete independent chains/counts/conservation: PASS; contributions are cells, not rainfall/discharge\n";
    }
}
int main() {
    unsigned passed = 0;
    try {
        const auto budget = ProcGen::drainage_hard_limits;
        const auto d = ProcGen::Domain::checked(std::array<ProcGen::Index, 2>{3, 2}).value();
        const std::array<ProcGen::Height, 6> heights{9, 6, 3, 8, 5, 0};
        const std::array<ProcGen::Index, 1> outlets{5};
        const ProcGen::DrainageInput input{d, heights, outlets};
        const auto literal = ProcGen::analyze_drainage(input, budget).value();
        report("literal", input, literal);
        ++passed;
        const auto field =
            ProcGen::generate_elevation(std::array<ProcGen::Index, 2>{17, 11}, {42, 0x67656f6772617068, 7}, {3, -80, 120}, budget).value();
        std::cout << "U8 source: lattice_bilinear v" << field.descriptor().version
                  << "; seed=42; stage=0x67656f6772617068; stream=7; spacing=3; range=-80..120; explicit outlets={0,186}\n";
        const std::array<ProcGen::Index, 2> chosen{0, 186};
        const ProcGen::DrainageInput generated{field.descriptor().domain, field.heights(), chosen};
        const auto drainage = ProcGen::analyze_drainage(generated, budget).value();
        report("generated (six sample cells)", generated, drainage);
        ++passed;
        std::cout << "Drainage consumer: " << passed << " passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Drainage consumer failure: " << error.what() << "\nDrainage consumer: " << passed
                  << " passed, 1 failed, 0 skipped.\n";
        return 1;
    }
}
