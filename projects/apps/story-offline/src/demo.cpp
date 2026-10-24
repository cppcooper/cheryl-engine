#include "story.h"

#include <algorithm>

namespace OfflineStory {
    int run(std::ostream& out, std::string_view set_id, std::string_view action_id, std::size_t flavour, std::string_view mode) {
        const auto candidates = manual_sets();
        const auto found = std::find_if(candidates.begin(), candidates.end(), [&](const auto& row) { return row.id == set_id; });
        if (found == candidates.end() || flavour > 1 || (mode != "none" && mode != "restore" && mode != "drain"))
            return 2;
        if (std::none_of(found->actions.begin(), found->actions.end(), [&](const auto& row) { return row.id == action_id; }))
            return 2;
        const auto catalogue = ferry_catalogue();
        out << "Proposed [manually authored; no model/provider]: " << found->id << '\n' << found->proposition << '\n';
        for (const auto& action : found->actions)
            out << "  candidate " << action.id << " -> " << action.outcome << " (" << action.flavours.size()
                << " wording/response variants)\n";
        const auto accepted = accept(*found, catalogue, Context{});
        if (!accepted) {
            out << "Rejected candidates: " << accepted.error().field << '\n';
            return 3;
        }
        const auto projected = project(*accepted, flavour);
        if (!projected) {
            out << "Rejected projection: " << projected.error().field << '\n';
            return 3;
        }
        out << "Accepted: " << (*projected)->definition()->id().value << " revision=" << (*projected)->definition()->revision()
            << " mechanical-actions=3 flavour=" << flavour << '\n';
        auto started = Encounter::start(*projected, {"ferry-owner"});
        if (!started)
            return 3;
        auto& owner = *started;
        const auto view = owner.view().value();
        out << "Issued offer: " << view.node.value << " step=" << view.step << '\n';
        for (const auto& row : view.choices)
            out << "  " << row.id.value << ": " << row.text << (row.available ? " [available]" : " [unavailable]") << '\n';
        const auto selected =
            std::find_if(view.choices.begin(), view.choices.end(), [&](const auto& row) { return row.id.value == action_id; });
        if (mode == "drain") {
            if (!owner.resources(0, 0))
                return 3;
            out << "Host changes live resources after presentation: coins=0 planks=0\n";
        }
        const auto chosen = owner.choose(selected->selection); // Only a real issued selection, never reconstructed authority.
        if (!chosen) {
            out << "Choice rejected: " << (chosen.error() == D::SessionError::Unavailable ? "unavailable" : "session rejection") << '\n';
            print_state(out, owner.state(), **accepted);
            out << "Effects in this owner: " << owner.effects() << '\n';
            return 3;
        }
        out << "Chosen: " << chosen->choice.value << " -> " << chosen->target.value << '\n';
        auto response = owner.view().value();
        out << "NPC prose: " << response.text << '\n';
        if (mode == "restore") {
            const auto paired = owner.capture().value();
            const auto old = response.choices[0].selection;
            if (!owner.restore(paired))
                return 3;
            const auto rejected = owner.choose(old);
            if (rejected || rejected.error() != D::SessionError::WrongIncarnation)
                return 3;
            out << "Restored paired boundary; old selection rejected; effects=" << owner.effects() << '\n';
            response = owner.view().value();
        }
        if (!owner.choose(response.choices[0].selection))
            return 3;
        out << "End: " << owner.view().value().text << '\n';
        print_state(out, owner.state(), **accepted);
        out << "Effects in this owner: " << owner.effects() << '\n';
        return 0;
    }
}
