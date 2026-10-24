#include "bargain.h"

#include <stdexcept>

namespace OfflineStory {
    namespace {
        D::Selection issued(const D::View& view, std::string_view id) {
            for (const auto& row : view.choices)
                if (row.id.value == id)
                    return row.selection;
            throw std::logic_error("Missing issued bargain choice");
        }
        void show(std::ostream& out, const D::View& view) {
            out << "Bargain node: " << view.node.value << '\n' << view.text << '\n';
            for (const auto& row : view.choices)
                out << "  " << row.id.value << ": " << row.text << (row.available ? " [available]" : " [unavailable]") << '\n';
        }
        std::string_view resolution(ReliefResolution r) {
            switch (r) {
                case ReliefResolution::Open:
                    return "open";
                case ReliefResolution::Helped:
                    return "helped";
                case ReliefResolution::Paid:
                    return "paid";
                case ReliefResolution::Promised:
                    return "promised";
                case ReliefResolution::Negotiated:
                    return "negotiated";
                case ReliefResolution::Refused:
                    return "refused";
            }
            return "invalid";
        }
    }
    void print_relief(std::ostream& out, const ReliefState& s) {
        out << "Relief actual: coins=" << s.world.facts.coins << " planks=" << s.world.facts.planks << " terms=" << s.terms_known
            << " resolution=" << resolution(s.resolution) << " due_planks=" << s.planks_due << " requester=";
        switch (s.requester_knows) {
            case ReliefKnowledge::None:
                out << "unaware";
                break;
            case ReliefKnowledge::Discussed:
                out << "discussed";
                break;
            case ReliefKnowledge::SawHelp:
                out << "saw-help";
                break;
            case ReliefKnowledge::SawPayment:
                out << "saw-payment";
                break;
            case ReliefKnowledge::HeardPromise:
                out << "heard-promise";
                break;
            case ReliefKnowledge::HeardRefusal:
                out << "heard-refusal";
                break;
        }
        out << " attitude=" << s.requester_attitude << '\n';
    }
    int run_bargain(
        std::ostream& out,
        std::string_view configuration,
        std::string_view prior,
        std::string_view route,
        std::size_t flavour,
        std::string_view mode
    ) {
        if ((configuration != "ramp" && configuration != "shelter") || (prior != "pay" && prior != "repair" && prior != "decline") ||
            (route != "immediate" && route != "pay" && route != "promise" && route != "negotiate" && route != "refuse") || flavour > 1 ||
            (mode != "none" && mode != "restore" && mode != "drain"))
            return 2;
        const auto ferry_graph = project(accept(manual_sets()[0], ferry_catalogue(), {}).value(), 0).value();
        auto ferry = Encounter::start(ferry_graph, {"ferry-owner"}).value();
        ferry.choose(issued(ferry.view().value(), prior)).value();
        ferry.choose(issued(ferry.view().value(), "continue")).value();
        const auto watch_content = compose_watch(manual_library(), ferry_graph, ferry.capture().value(), 0).value();
        auto watch = WatchEncounter::start(watch_content, {"watch-owner"}).value();
        auto watch_view = watch.view().value();
        watch.choose(issued(watch_view, prior == "decline" ? "withhold" : watch_content->recipe().action)).value();
        if (prior != "decline")
            watch.choose(issued(watch.view().value(), "ask-again")).value();
        const BargainContext context{watch_content, watch.capture().value()};
        const auto content = compose_bargain(
                                 manual_bargain_sets(), relief_catalogue(), context,
                                 {std::string(configuration) + (prior == "decline" ? "-bank" : "-quay"), flavour}
        )
                                 .value();
        auto scene = BargainEncounter::start(content, {"relief-01"}).value();
        out << "Bargain [manually authored; no model/provider]: " << content->request().id << " flavour=" << flavour << '\n';
        out << "Requester: " << content->request().actor << "; source=" << (prior == "decline" ? "original bank" : "harbor quay") << '\n';
        const auto& rule = content->rule();
        out << "Approved terms: now_planks=" << rule.now_planks << " coins=" << rule.coins << " ordinary_due=" << rule.promise_planks
            << '\n';
        auto offered = scene.view().value();
        show(out, offered);
        if (mode == "drain") {
            scene.resources(0, 0);
            out << "Host changes live resources to zero after presentation.\n";
        }
        if (route == "negotiate") {
            scene.choose(issued(offered, "ask-terms")).value();
            offered = scene.view().value();
            show(out, offered);
            out << "Learned smaller_due=" << rule.negotiated_planks << '\n';
            if (mode == "restore") {
                const auto saved = scene.capture().value();
                const auto old = issued(offered, "negotiate");
                scene.restore(saved).value();
                const auto rejected = scene.choose(old);
                if (rejected || rejected.error() != D::SessionError::WrongIncarnation)
                    return 3;
                out << "Restored information boundary; old token rejected; no repeated source or information effect.\n";
            }
            scene.choose(issued(scene.view().value(), "negotiate")).value();
            offered = scene.view().value();
            show(out, offered);
        }
        const auto choice = route == "immediate" ? "help-now" : route == "negotiate" ? "commit" : route;
        const auto chosen = scene.choose(issued(offered, choice));
        if (!chosen) {
            out << "Bargain choice rejected: unavailable\n";
            print_relief(out, scene.state());
            out << "Effects: ferry=" << ferry.effects() << " watch=" << watch.effects() << " relief=" << scene.effects() << '\n';
            return 3;
        }
        offered = scene.view().value();
        show(out, offered);
        if (mode == "restore" && route != "negotiate") {
            const auto saved = scene.capture().value();
            const auto old = issued(offered, "close");
            scene.restore(saved).value();
            const auto rejected = scene.choose(old);
            if (rejected || rejected.error() != D::SessionError::WrongIncarnation)
                return 3;
            out << "Restored outcome boundary; old token rejected; no repeated source or cost effect.\n";
        }
        scene.choose(issued(scene.view().value(), "close")).value();
        out << "End: " << scene.capture().value().dialogue.node.value << '/' << resolution(scene.state().resolution) << '\n';
        print_relief(out, scene.state());
        print_state(out, scene.state().world, content->context().previous->ferry()->accepted());
        out << "Effects: ferry=" << ferry.effects() << " watch=" << watch.effects() << " relief=" << scene.effects() << '\n';
        return 0;
    }
}
