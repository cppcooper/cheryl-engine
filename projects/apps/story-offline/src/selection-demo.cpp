#include "selection.h"

#include <stdexcept>

namespace OfflineStory {
    namespace {
        D::Selection token(const D::View& view, std::string_view id) {
            for (const auto& c : view.choices)
                if (c.id.value == id)
                    return c.selection;
            throw std::logic_error("Missing selected encounter choice");
        }
        void show(std::ostream& out, const D::View& v) {
            out << "Selected node: " << v.node.value << '\n' << v.text << '\n';
            for (const auto& row : v.choices)
                out << "  " << row.id.value << ": " << row.text << (row.available ? " [available]" : " [unavailable]") << '\n';
        }
        std::string_view refusal(SelectionCode c) {
            switch (c) {
                case SelectionCode::Empty:
                    return "empty-compatible-set";
                case SelectionCode::WorkLimit:
                    return "incomplete-work-budget";
                case SelectionCode::DrawLimit:
                    return "draw-budget";
                default:
                    return "invalid-or-resource-refusal";
            }
        }
    }
    int run_selected(std::ostream& out, std::uint64_t seed, std::string_view profile, std::string_view route) {
        if ((profile != "any" && profile != "open" && profile != "small" && profile != "consult" && profile != "empty" &&
             profile != "budget" && profile != "nodraw") ||
            (route != "immediate" && route != "promise" && route != "negotiate" && route != "refuse"))
            return 2;
        const auto ferry_graph = project(accept(manual_sets()[0], ferry_catalogue(), {}).value(), 0).value();
        auto ferry = Encounter::start(ferry_graph, {"seed-source-ferry"}).value();
        ferry.choose(token(ferry.view().value(), "pay")).value();
        ferry.choose(token(ferry.view().value(), "continue")).value();
        const auto watch_content = compose_watch(manual_library(), ferry_graph, ferry.capture().value(), 0).value();
        auto watch = WatchEncounter::start(watch_content, {"seed-source-watch"}).value();
        watch.choose(token(watch.view().value(), "fare-record")).value();
        watch.choose(token(watch.view().value(), "ask-again")).value();
        SelectionConstraints c;
        SelectionLimits l;
        if (profile == "open")
            c.required_initial = 1u << static_cast<unsigned>(ReliefIntent::Help);
        if (profile == "small") {
            c.max_material = 1;
            c.max_coins = 2;
            c.max_pending = 2;
        }
        if (profile == "consult")
            c.edges = 14;
        if (profile == "empty")
            c.max_pending = 0;
        if (profile == "budget")
            l.work = 0;
        if (profile == "nodraw")
            l.draws = 0;
        auto selected = select_bargain(selection_library(), selection_catalogue(), {watch_content, watch.capture().value()}, seed, 1, c, l);
        if (!selected) {
            out << "Selection refused before bargain play: " << refusal(selected.error().code) << " field=" << selected.error().field
                << '\n';
            return 3;
        }
        const auto& chosen = **selected;
        const auto& d = chosen.descriptor();
        const auto report = compare_bargain(*chosen.content());
        out << "Selected [manually authored; no model/provider]: algorithm=" << d.algorithm << " seed=" << d.seed
            << " examined=" << d.examined << " compatible=" << d.compatible << '\n';
        out << "Ingredients: request=" << d.request << " proposition=" << d.proposition << " actor=" << d.actor << " plan=" << d.plan
            << " bundle=" << (d.bundle == ReliefBundle::OpenOffer ? "open" : "explain-first") << " flavour=" << d.flavour << '\n';
        const auto& m = report.mechanics;
        out << "Mechanics: material=" << m.material_cost << " coins=" << m.coin_cost << " pending=" << m.ordinary_due
            << " revised=" << m.negotiated_due << " edges=" << m.edges.size() << '\n';
        auto scene = BargainEncounter::start(chosen.content(), {"relief-seeded-01"}).value();
        auto v = scene.view().value();
        show(out, v);
        if (route != "refuse" && (d.bundle == ReliefBundle::ExplainFirst || route == "negotiate")) {
            scene.choose(token(v, "ask-terms")).value();
            v = scene.view().value();
            show(out, v);
            const auto pair = scene.capture().value();
            const auto old = token(v, "help-now");
            scene.restore(pair).value();
            const auto rejected = scene.choose(old);
            if (rejected || rejected.error() != D::SessionError::WrongIncarnation)
                throw std::logic_error("Resume authority failure");
            out << "Restored selected information pair; old token rejected; no replay.\n";
            v = scene.view().value();
        }
        if (route == "negotiate") {
            scene.choose(token(v, "negotiate")).value();
            v = scene.view().value();
            show(out, v);
        }
        scene.choose(token(v, route == "immediate" ? "help-now" : route == "negotiate" ? "commit" : route)).value();
        show(out, scene.view().value());
        scene.choose(token(scene.view().value(), "close")).value();
        print_relief(out, scene.state());
        out << "Selected end: " << scene.capture().value().dialogue.node.value << "; effects: ferry=" << ferry.effects()
            << " watch=" << watch.effects() << " relief=" << scene.effects() << '\n';
        return 0;
    }
}
