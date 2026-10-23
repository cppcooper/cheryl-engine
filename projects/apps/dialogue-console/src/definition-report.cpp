#include "definition-report.h"
#include "definition-codec.h"

#include <algorithm>
#include <array>
#include <limits>
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
        // Fixed storage and subtraction-checked counts bound all indexing and
        // queue work before copying any authored data into the owned report.
        static_assert(maximum_definition_nodes < std::numeric_limits<std::size_t>::max() / 64);
        static_assert(maximum_definition_choices < std::numeric_limits<std::size_t>::max() / 64);
        static_assert(maximum_definition_nodes <= std::numeric_limits<std::size_t>::max() / maximum_definition_choices);
        const auto nodes = definition.nodes();
        if (nodes.size() > maximum_definition_nodes)
            return std::unexpected("Inspection exceeds 256 nodes");
        if (definition.id().value.size() > maximum_definition_id_bytes)
            return std::unexpected("Inspection definition ID exceeds 128 bytes");
        std::size_t edges = 0;
        std::size_t terminals = 0;
        std::size_t entry = 0;
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            const auto& node = nodes[index];
            if (node.id.value.size() > maximum_definition_id_bytes)
                return std::unexpected("Inspection node ID exceeds 128 bytes");
            if (node.choices.size() > maximum_definition_choices - edges)
                return std::unexpected("Inspection exceeds 2048 choices");
            edges += node.choices.size();
            terminals += node.kind == D::NodeKind::Terminal;
            if (&node == &definition.entry())
                entry = index;
        }

        // Compiled definitions guarantee entry and target membership. Resolve
        // targets by bounded linear ID lookup, then traverse flat adjacency in
        // both directions. Parallel edges count separately; enqueue each node once.
        std::array<std::size_t, maximum_definition_nodes + 1> forward{}, reverse{};
        std::array<std::size_t, maximum_definition_choices> targets{}, sources{};
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            auto edge = forward[index];
            for (const auto& choice : nodes[index].choices) {
                const auto target = static_cast<std::size_t>(
                    std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) { return node.id == choice.target; }) - nodes.begin()
                );
                targets[edge++] = target;
                ++reverse[target + 1];
            }
            forward[index + 1] = edge;
        }
        for (std::size_t index = 0; index < nodes.size(); ++index)
            reverse[index + 1] += reverse[index];
        auto positions = reverse;
        for (std::size_t index = 0; index < nodes.size(); ++index)
            for (auto edge = forward[index]; edge < forward[index + 1]; ++edge)
                sources[positions[targets[edge]]++] = index;

        std::array<bool, maximum_definition_nodes> reachable{}, terminal_route{};
        std::array<std::size_t, maximum_definition_nodes> queue{};
        const auto flood = [&](const auto& offsets, const auto& adjacent, auto& visited, std::size_t tail) {
            for (std::size_t head = 0; head < tail; ++head) {
                const auto node = queue[head];
                for (auto edge = offsets[node]; edge < offsets[node + 1]; ++edge) {
                    const auto next = adjacent[edge];
                    if (!visited[next]) {
                        visited[next] = true;
                        queue[tail++] = next;
                    }
                }
            }
        };
        queue[0] = entry;
        reachable[entry] = true;
        flood(forward, targets, reachable, 1);
        std::size_t tail = 0;
        for (std::size_t index = 0; index < nodes.size(); ++index)
            if (nodes[index].kind == D::NodeKind::Terminal) {
                queue[tail++] = index;
                terminal_route[index] = true;
            }
        flood(reverse, sources, terminal_route, tail);

        DefinitionReport report{definition.id(), definition.revision(), definition.entry().id, nodes.size(), edges, terminals, {}, {}, {}};
        // Preserve authored order, independent of entry position or traversal order.
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            (reachable[index] ? report.reachable : report.unreachable).push_back(nodes[index].id);
            if (!terminal_route[index])
                report.no_terminal_route.push_back(nodes[index].id);
        }
        return report;
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
