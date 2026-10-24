#include "definition-report.h"
#include "definition-codec.h"

#include <locale>
#include <sstream>

namespace GateConsole {
    namespace {
        // Keep IDs on one line, including authored quotes, newlines and escaped
        // NUL. This is display escaping, not another JSON codec.
        void identifier(std::ostream& output, std::string_view value) {
            constexpr char hex[] = "0123456789abcdef";
            output << '"';
            for (const unsigned char byte : value) {
                if (byte == '"' || byte == '\\')
                    output << '\\' << static_cast<char>(byte);
                else if (byte < 0x20 || byte == 0x7f)
                    output << "\\u00" << hex[byte >> 4] << hex[byte & 15];
                else
                    output << static_cast<char>(byte);
            }
            output << '"';
        }
    }

    std::expected<DefinitionReport, std::string> inspect_definition(const D::Definition& definition) {
        // Preserve legacy CLI cap precedence/messages for direct compiled callers.
        // This is a bounded policy preflight, not another graph traversal.
        const auto nodes = definition.nodes();
        if (nodes.size() > maximum_definition_nodes)
            return std::unexpected("Inspection exceeds 256 nodes");
        if (definition.id().value.size() > maximum_definition_id_bytes)
            return std::unexpected("Inspection definition ID exceeds 128 bytes");
        std::size_t edges = 0;
        for (const auto& node : nodes) {
            if (node.id.value.size() > maximum_definition_id_bytes)
                return std::unexpected("Inspection node ID exceeds 128 bytes");
            if (node.choices.size() > maximum_definition_choices - edges)
                return std::unexpected("Inspection exceeds 2048 choices");
            edges += node.choices.size();
        }
        auto result = D::inspect(definition);
        if (!result)
            return std::unexpected("Inspection resource limit exceeded");
        return std::move(*result);
    }

    std::string format_definition_report(const DefinitionReport& report) {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << "Definition: ";
        identifier(output, report.definition.value);
        output << "\nRevision: " << report.revision << "\nEntry: ";
        identifier(output, report.entry.value);
        output << "\nNodes: " << report.nodes << "\nChoices: " << report.choices << "\nTerminals: " << report.terminals << '\n';
        const auto list = [&](std::string_view label, const auto& ids) {
            output << label << " (" << ids.size() << "): [";
            bool first = true;
            for (const auto& id : ids) {
                if (!first)
                    output << ", ";
                identifier(output, id.value);
                first = false;
            }
            output << "]\n";
        };
        list("Entry-reachable", report.reachable);
        list("Unreachable", report.unreachable);
        list("No terminal route", report.no_terminal_route);
        output
            << "Advisory topology: conditions are ignored; no proof of gameplay reachability, effect history, termination or guaranteed completion.\n";
        return output.str();
    }
}
