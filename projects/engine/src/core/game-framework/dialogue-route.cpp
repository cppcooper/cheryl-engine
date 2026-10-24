#include <cheryl/core/game-framework/dialogue-route.h>

#include <bit>
#include <limits>
#include <stdexcept>

namespace CE::Dialogue {
    namespace {
        bool add(std::size_t& total, std::size_t amount, std::size_t limit) noexcept {
            if (amount > limit - total)
                return false;
            total += amount;
            return true;
        }

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
            bool product(std::size_t count, std::size_t amount) noexcept {
                return (!count || amount <= remaining / count) && take(count * amount);
            }
        };
    }

    std::expected<Route, RouteError>
    find_route(const Definition& definition, const NodeId& from, const NodeId& to, const RouteLimits& limits) {
        const auto& hard = maximum_route_limits;
        if (limits.nodes > hard.nodes || limits.choices > hard.choices || limits.route_edges > hard.route_edges ||
            limits.copied_id_bytes > hard.copied_id_bytes || limits.storage_bytes > hard.storage_bytes || limits.work > hard.work)
            return std::unexpected(RouteError::InvalidLimits);
        const auto nodes = definition.nodes();
        const auto count = nodes.size();
        if (count > limits.nodes)
            return std::unexpected(RouteError::NodeLimit);
        Work work{limits.work};
        std::size_t edges = 0;
        for (const auto& node : nodes) {
            if (!work.take())
                return std::unexpected(RouteError::WorkLimit);
            if (!add(edges, node.choices.size(), limits.choices))
                return std::unexpected(RouteError::ChoiceLimit);
        }
        // U7's lower_bound uses at most bit_width(N) comparisons plus final equality.
        // Reserve a query-length bound before each lookup without copying query keys.
        const auto comparisons = static_cast<std::size_t>(std::bit_width(count)) + 1;
        const auto lookup_work = [&](const NodeId& id) { return work.take(comparisons) && work.product(comparisons, id.value.size()); };
        if (!lookup_work(from))
            return std::unexpected(RouteError::WorkLimit);
        const auto source = definition.find_node(from);
        if (!source)
            return std::unexpected(RouteError::UnknownFrom);
        if (!lookup_work(to))
            return std::unexpected(RouteError::WorkLimit);
        const auto target = definition.find_node(to);
        if (!target)
            return std::unexpected(RouteError::UnknownTo);
        const auto start = static_cast<std::size_t>(source - nodes.data());
        const auto finish = static_cast<std::size_t>(target - nodes.data());

        std::size_t words = 0;
        if (count > std::numeric_limits<std::size_t>::max() / 3 ||
            (words = 3 * count) > std::numeric_limits<std::size_t>::max() / sizeof(std::size_t))
            return std::unexpected(RouteError::StorageLimit);
        const auto scratch_bytes = words * sizeof(std::size_t);
        const auto storage = [&](std::size_t node_slots, std::size_t choice_slots, std::size_t bytes) -> std::optional<std::size_t> {
            std::size_t total = 0;
            const auto slots = [&](std::size_t size, std::size_t item) {
                return size <= limits.storage_bytes / item && add(total, size * item, limits.storage_bytes);
            };
            if (!add(total, scratch_bytes, limits.storage_bytes) || !add(total, sizeof(Route), limits.storage_bytes) ||
                !slots(node_slots, sizeof(NodeId)) || !slots(choice_slots, sizeof(ChoiceId)) || !add(total, bytes, limits.storage_bytes) ||
                !add(total, node_slots, limits.storage_bytes) || !add(total, choice_slots, limits.storage_bytes) ||
                !add(total, 1, limits.storage_bytes))
                return {};
            return total;
        };
        std::size_t minimum_ids = 0;
        if (!add(minimum_ids, definition.id().value.size(), limits.copied_id_bytes) ||
            !add(minimum_ids, source->id.value.size(), limits.copied_id_bytes))
            return std::unexpected(RouteError::CopiedIdLimit);
        if (!storage(1, 0, minimum_ids))
            return std::unexpected(RouteError::StorageLimit);
        if (!work.take(words))
            return std::unexpected(RouteError::WorkLimit);
        // Parent sentinel N means undiscovered. Queue entries are unique, so N slots suffice.
        std::vector<std::size_t> scratch(words, count);
        auto rest = std::span(scratch);
        auto parents = rest.first(count);
        auto via = rest.subspan(count, count);
        auto queue = rest.subspan(2 * count, count);
        parents[start] = start;
        queue[0] = start;
        std::size_t tail = 1;
        for (std::size_t head = 0; head < tail && parents[finish] == count; ++head) {
            if (!work.take())
                return std::unexpected(RouteError::WorkLimit);
            const auto current = queue[head];
            const auto& choices = nodes[current].choices;
            for (std::size_t index = 0; index < choices.size(); ++index) {
                if (!work.take() || !lookup_work(choices[index].target))
                    return std::unexpected(RouteError::WorkLimit);
                const auto next = definition.find_node(choices[index].target);
                if (!next)
                    throw std::logic_error("Compiled dialogue route target is absent");
                const auto ordinal = static_cast<std::size_t>(next - nodes.data());
                if (parents[ordinal] != count)
                    continue;
                parents[ordinal] = current;
                via[ordinal] = index;
                queue[tail++] = ordinal;
                if (ordinal == finish)
                    break;
            }
        }
        if (parents[finish] == count)
            return std::unexpected(RouteError::Unreachable);
        std::size_t length = 0;
        for (auto current = finish;; current = parents[current]) {
            if (!work.take())
                return std::unexpected(RouteError::WorkLimit);
            if (current == start)
                break;
            if (!add(length, 1, limits.route_edges))
                return std::unexpected(RouteError::OutputLimit);
        }
        // Count all output bytes and reserve copy work before any result allocation.
        std::size_t copied = 0;
        if (!add(copied, definition.id().value.size(), limits.copied_id_bytes))
            return std::unexpected(RouteError::CopiedIdLimit);
        for (auto current = finish;; current = parents[current]) {
            if (!work.take())
                return std::unexpected(RouteError::WorkLimit);
            if (!add(copied, nodes[current].id.value.size(), limits.copied_id_bytes))
                return std::unexpected(RouteError::CopiedIdLimit);
            if (current == start)
                break;
            const auto& choice = nodes[parents[current]].choices[via[current]];
            if (!add(copied, choice.id.value.size(), limits.copied_id_bytes))
                return std::unexpected(RouteError::CopiedIdLimit);
        }
        const auto node_slots = length + 1; // Length <= N-1 <= hard ceiling, already checked.
        const auto peak = storage(node_slots, length, copied);
        if (!peak)
            return std::unexpected(RouteError::StorageLimit);
        if (!work.take(node_slots) || !work.take(length) || !work.take(node_slots) || !work.take(length) || !work.take(copied) ||
            !work.take(node_slots) || !work.take(length) || !work.take(1))
            return std::unexpected(RouteError::WorkLimit);
        Route route;
        route.nodes.resize(node_slots);
        route.choices.resize(length);
        route.definition = definition.id();
        route.revision = definition.revision();
        auto current = finish;
        for (auto position = length;; --position) {
            route.nodes[position] = nodes[current].id;
            if (!position)
                break;
            route.choices[position - 1] = nodes[parents[current]].choices[via[current]].id;
            current = parents[current];
        }
        route.usage = {copied, *peak, work.used};
        return route;
    }
}
