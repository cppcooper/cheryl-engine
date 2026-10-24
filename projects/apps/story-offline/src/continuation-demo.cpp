#include "continuation.h"

#include <algorithm>
#include <stdexcept>

namespace OfflineStory {
    namespace {
        D::Selection issued(const D::View& view, std::string_view id) {
            for (const auto& row : view.choices)
                if (row.id.value == id)
                    return row.selection;
            throw std::logic_error("Missing issued demonstration choice");
        }
    }
    int run_watch(
        std::ostream& out,
        std::string_view set_id,
        std::string_view ferry_action,
        std::string_view action,
        std::size_t flavour,
        std::string_view mode
    ) {
        const auto sets = manual_sets();
        const auto set = std::find_if(sets.begin(), sets.end(), [&](const auto& s) { return s.id == set_id; });
        if (set == sets.end() || (ferry_action != "pay" && ferry_action != "repair" && ferry_action != "decline") ||
            (action != "evidence" && action != "withhold") || flavour > 1 || (mode != "none" && mode != "restore" && mode != "drain"))
            return 2;
        const auto accepted = accept(*set, ferry_catalogue(), {}).value();
        const auto graph = project(accepted, 0).value();
        auto ferry = Encounter::start(graph, {"ferry-owner"}).value();
        ferry.choose(issued(ferry.view().value(), ferry_action)).value();
        ferry.choose(issued(ferry.view().value(), "continue")).value();
        const auto completed = ferry.capture().value();
        const auto library = manual_library();
        const auto content = compose_watch(library, graph, completed, flavour).value();
        auto watch = WatchEncounter::start(content, {"watch-owner"}).value();
        out << "Continuation [manually authored; no model/provider]: " << content->recipe().id << " flavour=" << flavour << '\n';
        out << "Ferry completed: " << completed.dialogue.node.value << " ferry-effects=" << ferry.effects() << '\n';
        const auto offered = watch.view().value();
        out << "Issued watch: " << offered.text << '\n';
        for (const auto& row : offered.choices)
            out << "  " << row.id.value << ": " << row.text << (row.available ? " [available]" : " [unavailable]") << '\n';
        const auto selection = issued(offered, action == "withhold" ? "withhold" : content->recipe().action);
        if (mode == "drain") {
            watch.evidence(false);
            out << "Host withdraws the evidence source after presentation.\n";
        }
        const auto chosen = watch.choose(selection);
        if (!chosen) {
            out << "Watch choice rejected: unavailable\n";
            print_state(out, watch.state().world, *accepted);
            out << "Effects: ferry=" << ferry.effects() << " watch=" << watch.effects() << '\n';
            return 3;
        }
        auto response = watch.view().value();
        out << "Watch reply: " << response.text << '\n';
        if (response.kind == D::NodeKind::Prompt) {
            const auto old = issued(response, "ask-again");
            if (mode == "restore") {
                const auto saved = watch.capture().value();
                watch.restore(saved).value();
                const auto rejected = watch.choose(old);
                if (rejected || rejected.error() != D::SessionError::WrongIncarnation)
                    return 3;
                out << "Restored watch pair; old selection rejected; no repeated fare/reward.\n";
            }
            watch.choose(issued(watch.view().value(), "ask-again")).value();
            out << "Later response: " << watch.view().value().text << '\n';
        }
        print_state(out, watch.state().world, *accepted);
        out << "Effects: ferry=" << ferry.effects() << " watch=" << watch.effects() << '\n';
        auto later = completed;
        later.state = watch.state().world; // Explicit serialized world-context copy; old cursor is provenance, not new authority.
        const auto remembered = compose_watch(library, graph, later, flavour).value();
        out << "Recomposed greeting (not started): " << remembered->definition()->entry().text << '\n';
        return 0;
    }
}
