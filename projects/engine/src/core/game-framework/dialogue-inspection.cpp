#include <cheryl/core/game-framework/dialogue-inspection.h>

#include <algorithm>
#include <limits>
#include <span>
#include <vector>

namespace CE::Dialogue {
    namespace {
        struct Work {
            std::size_t remaining;
            std::size_t used = 0;

            bool take(std::size_t amount = 1) noexcept {
                if (amount > remaining)
                    return false;
                remaining -= amount;
                used += amount;
                return true;
            }
        };
    }

    std::expected<InspectionReport, InspectionError> inspect(const Definition& definition, const InspectionLimits& limits) {
        const auto& hard = maximum_inspection_limits;
        if (limits.nodes > hard.nodes || limits.choices > hard.choices || limits.copied_id_bytes > hard.copied_id_bytes ||
            limits.storage_bytes > hard.storage_bytes || limits.work > hard.work)
            return std::unexpected(InspectionError::InvalidLimits);
        // These bounds cover every subsequent count/product, even on 32-bit size_t.
        static_assert(
            16 * (2 * maximum_inspection_limits.choices + 6 * maximum_inspection_limits.nodes + 3) < std::numeric_limits<std::size_t>::max()
        );
        static_assert(
            4 * maximum_inspection_limits.nodes * sizeof(NodeId) + 2 * maximum_inspection_limits.copied_id_bytes <
            std::numeric_limits<std::size_t>::max()
        );
        const auto nodes = definition.nodes();
        const auto count = nodes.size();
        if (count > limits.nodes)
            return std::unexpected(InspectionError::NodeLimit);
        Work work{limits.work};
        std::size_t edges = 0;
        std::size_t terminals = 0;
        std::size_t entry = 0;
        std::size_t copied = 0;
        const auto id_bytes = [&](std::size_t bytes) {
            if (bytes > limits.copied_id_bytes - copied)
                return false;
            copied += bytes;
            return true;
        };
        if (!id_bytes(definition.id().value.size()) || !id_bytes(definition.entry().id.value.size()))
            return std::unexpected(InspectionError::CopiedIdLimit);
        for (std::size_t index = 0; index < count; ++index) {
            if (!work.take())
                return std::unexpected(InspectionError::WorkLimit);
            const auto& node = nodes[index];
            if (node.choices.size() > limits.choices - edges)
                return std::unexpected(InspectionError::ChoiceLimit);
            edges += node.choices.size();
            if (!id_bytes(node.id.value.size()))
                return std::unexpected(InspectionError::CopiedIdLimit);
            terminals += node.kind == NodeKind::Terminal;
            if (&node == &definition.entry())
                entry = index;
        }

        // One contiguous scratch allocation: CSR in both directions, write
        // cursors, queue and two visited sets. Each slot is a size_t.
        const auto words = 2 * edges + 6 * count + 3;
        const auto scratch_bytes = words * sizeof(std::size_t);
        const auto storage = [&](std::size_t slots, std::size_t bytes) {
            return scratch_bytes + sizeof(InspectionReport) + slots * sizeof(NodeId) + bytes + slots + 2;
        };
        if (storage(count, copied) > limits.storage_bytes)
            return std::unexpected(InspectionError::StorageLimit);
        if (!work.take(words))
            return std::unexpected(InspectionError::WorkLimit);
        std::vector<std::size_t> scratch(words, 0);
        auto rest = std::span(scratch);
        const auto portion = [&](std::size_t size) {
            const auto result = rest.first(size);
            rest = rest.subspan(size);
            return result;
        };
        auto forward = portion(count + 1), reverse = portion(count + 1), positions = portion(count + 1);
        auto targets = portion(edges), sources = portion(edges), queue = portion(count);
        auto reachable = portion(count), terminal_route = portion(count);
        for (std::size_t index = 0; index < count; ++index) {
            if (!work.take())
                return std::unexpected(InspectionError::WorkLimit);
            auto edge = forward[index];
            for (const auto& choice : nodes[index].choices) {
                if (!work.take())
                    return std::unexpected(InspectionError::WorkLimit);
                std::size_t target = 0;
                for (; target < count; ++target) {
                    if (!work.take())
                        return std::unexpected(InspectionError::WorkLimit);
                    const auto& candidate = nodes[target].id.value;
                    const auto& sought = choice.target.value;
                    if (candidate.size() != sought.size())
                        continue;
                    std::size_t byte = 0;
                    for (; byte < candidate.size(); ++byte) {
                        if (!work.take())
                            return std::unexpected(InspectionError::WorkLimit);
                        if (candidate[byte] != sought[byte])
                            break;
                    }
                    if (byte == candidate.size())
                        break;
                }
                // Successful immutable compilation guarantees target membership.
                targets[edge++] = target;
                ++reverse[target + 1];
            }
            forward[index + 1] = edge;
        }
        if (!work.take(2 * count + 1))
            return std::unexpected(InspectionError::WorkLimit);
        for (std::size_t index = 0; index < count; ++index)
            reverse[index + 1] += reverse[index];
        std::copy(reverse.begin(), reverse.end(), positions.begin());
        for (std::size_t index = 0; index < count; ++index) {
            if (!work.take())
                return std::unexpected(InspectionError::WorkLimit);
            for (auto edge = forward[index]; edge < forward[index + 1]; ++edge) {
                if (!work.take())
                    return std::unexpected(InspectionError::WorkLimit);
                sources[positions[targets[edge]]++] = index;
            }
        }
        const auto flood = [&](auto offsets, auto adjacent, auto visited, std::size_t tail) {
            for (std::size_t head = 0; head < tail; ++head) {
                if (!work.take())
                    return false;
                const auto node = queue[head];
                for (auto edge = offsets[node]; edge < offsets[node + 1]; ++edge) {
                    if (!work.take())
                        return false;
                    const auto next = adjacent[edge];
                    if (!visited[next]) {
                        visited[next] = 1;
                        queue[tail++] = next;
                    }
                }
            }
            return true;
        };
        queue[0] = entry;
        reachable[entry] = 1;
        if (!flood(forward, targets, reachable, 1))
            return std::unexpected(InspectionError::WorkLimit);
        std::size_t tail = 0;
        for (std::size_t index = 0; index < count; ++index) {
            if (!work.take())
                return std::unexpected(InspectionError::WorkLimit);
            if (nodes[index].kind == NodeKind::Terminal) {
                queue[tail++] = index;
                terminal_route[index] = 1;
            }
        }
        if (!flood(reverse, sources, terminal_route, tail))
            return std::unexpected(InspectionError::WorkLimit);
        std::size_t reachable_count = 0;
        std::size_t no_route_count = 0;
        for (std::size_t index = 0; index < count; ++index) {
            if (!work.take())
                return std::unexpected(InspectionError::WorkLimit);
            reachable_count += reachable[index] != 0;
            if (!terminal_route[index]) {
                ++no_route_count;
                if (!id_bytes(nodes[index].id.value.size()))
                    return std::unexpected(InspectionError::CopiedIdLimit);
            }
        }
        const auto slots = count + no_route_count;
        const auto peak = storage(slots, copied);
        if (peak > limits.storage_bytes)
            return std::unexpected(InspectionError::StorageLimit);
        // Exact list sizes and all copy work are prepared before allocating a
        // report. Definitions and any previously returned reports stay untouched.
        if (!work.take(count + slots + copied + slots + 2))
            return std::unexpected(InspectionError::WorkLimit);
        InspectionReport report;
        report.reachable.reserve(reachable_count);
        report.unreachable.reserve(count - reachable_count);
        report.no_terminal_route.reserve(no_route_count);
        report.definition = definition.id();
        report.revision = definition.revision();
        report.entry = definition.entry().id;
        report.nodes = count;
        report.choices = edges;
        report.terminals = terminals;
        for (std::size_t index = 0; index < count; ++index) {
            (reachable[index] ? report.reachable : report.unreachable).push_back(nodes[index].id);
            if (!terminal_route[index])
                report.no_terminal_route.push_back(nodes[index].id);
        }
        report.usage = {copied, peak, work.used};
        return report;
    }
}
