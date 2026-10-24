#include "payoff.h"

#include <stdexcept>

namespace OfflineStory {
    namespace {
        D::Selection token(const D::View& v, std::string_view id) {
            for (const auto& c : v.choices)
                if (c.id.value == id)
                    return c.selection;
            throw std::logic_error("Missing payoff issued choice");
        }
        void show(std::ostream& out, const D::View& v) {
            out << "Payoff node: " << v.node.value << '\n' << v.text << '\n';
            for (const auto& c : v.choices)
                out << "  " << c.id.value << ": " << c.text << (c.available ? " [available]" : " [unavailable]") << '\n';
        }
    }
    void print_payoff(std::ostream& out, const PayoffState& s) {
        const auto obligation = s.obligation == Obligation::None        ? "none"
                                : s.obligation == Obligation::Pending   ? "pending"
                                : s.obligation == Obligation::Fulfilled ? "fulfilled"
                                                                        : "broken";
        const auto outcome = s.outcome == PayoffOutcome::Open           ? "open"
                             : s.outcome == PayoffOutcome::Acknowledged ? "acknowledged"
                             : s.outcome == PayoffOutcome::Fulfilled    ? "fulfilled"
                             : s.outcome == PayoffOutcome::Broken       ? "broken"
                                                                        : "left";
        const auto knowledge = s.recipient_knows == RecipientKnowledge::SawHelp           ? "saw-help"
                               : s.recipient_knows == RecipientKnowledge::SawPayment      ? "saw-payment"
                               : s.recipient_knows == RecipientKnowledge::HeardPromise    ? "heard-promise"
                               : s.recipient_knows == RecipientKnowledge::HeardRefusal    ? "heard-refusal"
                               : s.recipient_knows == RecipientKnowledge::CheckedDelivery ? "checked-delivery"
                                                                                          : "heard-breach";
        out << "Payoff actual: coins=" << s.world.facts.coins << " planks=" << s.world.facts.planks << " ready=" << s.at_handoff
            << " obligation=" << obligation << " due=" << s.due << " delivered=" << s.delivered << " favor=" << s.favor
            << " outcome=" << outcome << '\n';
        out << "Recipient: knowledge=" << knowledge << " attitude=" << s.recipient_attitude << " believes-broken=" << s.believes_broken
            << '\n';
    }
    int run_payoff(std::ostream& out, std::uint64_t seed, std::string_view prior, std::string_view route, std::string_view mode) {
        if ((prior != "immediate" && prior != "pay" && prior != "promise" && prior != "negotiate" && prior != "refuse") ||
            (route != "deliver" && route != "break" && route != "acknowledge" && route != "leave") ||
            (mode != "none" && mode != "restore" && mode != "drain" && mode != "absent" && mode != "bank"))
            return 2;
        const bool promise = prior == "promise" || prior == "negotiate";
        if ((promise && route == "acknowledge") || (!promise && (route == "deliver" || route == "break")))
            return 2;
        const auto ferry_graph = project(accept(manual_sets()[0], ferry_catalogue(), {}).value(), 0).value();
        auto ferry = Encounter::start(ferry_graph, {"payoff-source-ferry"}).value();
        ferry.choose(token(ferry.view().value(), mode == "bank" ? "decline" : "pay")).value();
        ferry.choose(token(ferry.view().value(), "continue")).value();
        const auto watch_content = compose_watch(manual_library(), ferry_graph, ferry.capture().value(), 0).value();
        auto watch = WatchEncounter::start(watch_content, {"payoff-source-watch"}).value();
        watch.choose(token(watch.view().value(), mode == "bank" ? "withhold" : "fare-record")).value();
        if (mode != "bank")
            watch.choose(token(watch.view().value(), "ask-again")).value();
        const auto chosen =
            select_bargain(selection_library(), selection_catalogue(), {watch_content, watch.capture().value()}, seed, 1).value();
        auto bargain = BargainEncounter::start(chosen->content(), {"payoff-source-bargain"}).value();
        if (chosen->descriptor().bundle == ReliefBundle::ExplainFirst || prior == "negotiate")
            bargain.choose(token(bargain.view().value(), "ask-terms")).value();
        if (prior == "negotiate")
            bargain.choose(token(bargain.view().value(), "negotiate")).value();
        bargain.choose(token(bargain.view().value(), prior == "immediate" ? "help-now" : prior == "negotiate" ? "commit" : prior)).value();
        bargain.choose(token(bargain.view().value(), "close")).value();
        const auto completed = bargain.capture().value();
        const auto content =
            compose_payoff(
                chosen->content(), completed, {chosen->content()->request().location, chosen->content()->request().actor, promise},
                manual_payoff_library()
            )
                .value();
        auto scene = PayoffEncounter::start(content, {"relief-payoff-01"}).value();
        out << "Payoff [manually authored; retained content; no model/provider]: plan=" << chosen->descriptor().plan
            << " actor=" << content->event().recipient << " prior=" << prior << '\n';
        show(out, scene.view().value());
        print_payoff(out, scene.state());
        if (mode == "restore") {
            const auto before = scene.capture().value();
            scene.handoff(true).value();
            scene.restore(before).value();
            if (scene.state().at_handoff)
                throw std::logic_error("Pre-handoff resume failed");
            out << "Restored before handoff; no prior charges or favors replayed.\n";
        }
        scene.handoff(true).value();
        auto offered = scene.view().value();
        show(out, offered);
        print_payoff(out, scene.state());
        if (mode == "restore") {
            const auto pair = scene.capture().value();
            const auto old = token(offered, route);
            scene.restore(pair).value();
            auto rejected = scene.choose(old);
            if (rejected || rejected.error() != D::SessionError::WrongIncarnation)
                throw std::logic_error("Old payoff marker accepted");
            out << "Restored pending handoff pair; old token rejected; no effect replay.\n";
            offered = scene.view().value();
        }
        if (mode == "drain")
            scene.resources(0);
        if (mode == "absent")
            scene.handoff(false).value();
        const auto selected = scene.choose(token(offered, route));
        if (!selected) {
            out << "Payoff choice rejected before mutation.\n";
            print_payoff(out, scene.state());
            out << "Payoff effects: " << scene.effects() << '\n';
            return 3;
        }
        show(out, scene.view().value());
        if (mode == "restore") {
            auto fresh = PayoffEncounter::start(content, {"fresh-payoff"}).value();
            fresh.restore(scene.capture().value()).value();
            if (fresh.effects() != 0 || fresh.state() != scene.state())
                throw std::logic_error("Resolved payoff resume failed");
            out << "Fresh owner restored resolved pair; no additional delivery or favor.\n";
        }
        print_payoff(out, scene.state());
        print_state(out, scene.state().world, content->bargain()->context().previous->ferry()->accepted());
        out << "Effects: ferry=" << ferry.effects() << " watch=" << watch.effects() << " bargain=" << bargain.effects()
            << " payoff=" << scene.effects() << '\n';
        return 0;
    }
}
