#include "clearance.h"
#include "clearance-checks.h"

#include <exception>
#include <iostream>

namespace {
    bool report(ProcGen::OccupancyView input, std::int64_t minimum, ProcGen::CaveBudgets budget) {
        const auto field = ProcGen::analyze_clearance(input, minimum, budget);
        if (!field) {
            std::cerr << "Clearance refused: " << ProcGen::reason_text(field.error().reason) << '\n';
            return false;
        }
        const auto measured = ProcGenClearanceChecks::inspect(input, *field);
        std::cout << "  analysis=" << ProcGen::clearance_algorithm << "; version=" << field->descriptor().version
                  << "; domain=" << input.domain.extents()[0] << "x" << input.domain.extents()[1]
                  << "; index=x+width*y; metric=cardinal_steps_to_solid_or_outside; minimum=" << field->descriptor().minimum
                  << "; floor=" << measured.floor << "; solid=" << measured.solid << "; max_distance=" << measured.maximum
                  << "; usable=" << measured.usable << "; usable_regions=" << measured.components
                  << "; largest_region=" << measured.largest_region << "; work=" << field->work()
                  << "; exhaustive_Manhattan_and_cardinal_regions=PASS\n";
        return true;
    }
}
int main() {
    try {
        const ProcGen::CaveBudgets budget{4096, 256 * 1024, 2'000'000};
        const std::array<ProcGen::Index, 2> extents{17, 11};
        const ProcGen::StreamKey key{42, 0x63617665, 7};
        const auto cave = ProcGen::generate_cave(extents, key, {.solid_fill = 4200, .steps = 3}, budget);
        if (!cave) {
            std::cerr << "Source cave refused: " << ProcGen::reason_text(cave.error().reason) << '\n';
            return 1;
        }
        std::cout << "Source cave_cellular_2d; version=" << cave->descriptor().version << "; seed=" << key.seed << "; stage=" << key.stage
                  << "; stream=" << key.stream << "; fill=4200; steps=3; B5678/S45678; retain_components\n";
        const bool generated = report({cave->domain(), cave->occupancy()}, 2, budget);
        const auto domain = ProcGen::Domain::checked(std::array<ProcGen::Index, 2>{3, 3}).value();
        const std::array<std::uint8_t, 9> literal{0, 0, 0, 0, 0, 0, 0, 0, 0};
        std::cout << "Source literal all-floor3x3; solid exterior; no generator seed\n";
        const bool fixture = report({domain, literal}, 2, budget);
        std::cout << "Clearance consumer: " << (generated + fixture) << " passed, " << (!generated + !fixture) << " failed, 0 skipped.\n";
        return generated && fixture ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Clearance consumer exception: " << error.what() << '\n';
        return 1;
    }
}
