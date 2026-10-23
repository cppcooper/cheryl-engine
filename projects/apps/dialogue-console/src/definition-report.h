#pragma once

#include <cheryl/core/game-framework/dialogue.h>

#include <expected>
#include <string>
#include <vector>

namespace GateConsole {
    // Every value owns its storage; no definition, session or handler is retained.
    struct DefinitionReport {
        CE::Dialogue::DefinitionId definition;
        std::uint64_t revision = 0;
        CE::Dialogue::NodeId entry;
        std::size_t nodes = 0;
        std::size_t choices = 0;
        std::size_t terminals = 0;
        std::vector<CE::Dialogue::NodeId> reachable;
        std::vector<CE::Dialogue::NodeId> unreachable;
        std::vector<CE::Dialogue::NodeId> no_terminal_route;
    };

    // Borrows only during the call. Rechecks private graph/ID caps even for a
    // definition compiled directly rather than through the bounded JSON loader.
    std::expected<DefinitionReport, std::string> inspect_definition(const CE::Dialogue::Definition& definition);
    std::string format_definition_report(const DefinitionReport& report);
}
