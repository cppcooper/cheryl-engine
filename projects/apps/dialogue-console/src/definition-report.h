#pragma once

#include <cheryl/core/game-framework/dialogue-inspection.h>

#include <expected>
#include <string>

namespace GateConsole {
    // Engine owns the topology report; this private adapter owns legacy CLI policy.
    using DefinitionReport = CE::Dialogue::InspectionReport;

    // Borrows only during the call. Rechecks private graph/ID caps even for a
    // definition compiled directly rather than through the bounded JSON loader.
    std::expected<DefinitionReport, std::string> inspect_definition(const CE::Dialogue::Definition& definition);
    std::string format_definition_report(const DefinitionReport& report);
}
