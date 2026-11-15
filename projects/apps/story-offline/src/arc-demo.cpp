#include "arc.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#ifndef STORY_ARC_STANDALONE
namespace OfflineStory::Arc {
    namespace {
        D::Selection token(const D::View& view, std::string_view id) {
            for (const auto& row : view.choices)
                if (row.id.value == id)
                    return row.selection;
            throw std::logic_error("Missing live arc witness choice");
        }
        Component component(Role role, const Context& ctx, int ordinary, int revised) {
            const std::array names{"need", "evidence", "renegotiate", "commitment", "settlement", "response"};
            const std::array actions{Action::NoticeNeed, Action::ShareEvidence, Action::ReviseTerms,
                                     Action::PromiseOrRefuse, Action::CheckHandoff, Action::Respond};
            Component n;
            const auto index = static_cast<std::size_t>(role);
            n.id = names[index];
            n.role = role;
            n.action = actions[index];
            n.actor = role == Role::Evidence ? "witness" : ctx.recipient;
            n.recipient = ctx.recipient;
            n.location = ctx.location;
            n.material = ctx.material;
            n.society = ctx.society.id;
            n.text = "Reviewed example component: " + n.id;
            n.required_facts = role == Role::Need ? need_exists : role == Role::Evidence ? evidence_available : 0u;
            n.required_information = role == Role::Renegotiation ? evidence_known : role == Role::Response ? outcome_known : 0u;
            if (role == Role::Commitment) {
                n.ordinary_due = ordinary;
                n.revised_due = revised;
            }
            return n;
        }
        int live_host_witness(std::ostream& out, BargainHandle content) {
            auto bargain = BargainEncounter::start(content, {"arc-host-bargain"}).value();
            if (content->pick().bundle == ReliefBundle::ExplainFirst)
                bargain.choose(token(bargain.view().value(), "ask-terms")).value();
            bargain.choose(token(bargain.view().value(), "promise")).value();
            bargain.choose(token(bargain.view().value(), "close")).value();
            auto payoff = compose_payoff(content, bargain.capture().value(),
                                         {content->request().location, content->request().actor, false}, manual_payoff_library()).value();
            auto scene = PayoffEncounter::start(payoff, {"arc-host-payoff"}).value();
            if (scene.state().obligation != Obligation::Pending || scene.state().favor)
                return 4;
            auto before = scene.capture().value();
            auto absent = scene.choose(token(scene.view().value(), "deliver"));
            if (absent || scene.capture().value() != before)
                return 4;
            scene.handoff(true).value();
            const int available = scene.state().world.facts.planks;
            if (!scene.resources(0))
                return 4;
            before = scene.capture().value();
            auto insufficient = scene.choose(token(scene.view().value(), "deliver"));
            if (insufficient || scene.capture().value() != before || !scene.resources(available))
                return 4;
            scene.choose(token(scene.view().value(), "deliver")).value();
            out << "Separate existing host witness: absent/insufficient handoffs refused; obligation="
                << (scene.state().obligation == Obligation::Fulfilled ? "fulfilled" : "unexpected")
                << " delivered=" << scene.state().delivered << " remaining-planks=" << scene.state().world.facts.planks << '\n';
            return scene.state().obligation == Obligation::Fulfilled && scene.state().favor ? 0 : 4;
        }
    }
    int run(std::ostream& out) {
        // The retained existing selector supplies a real compatible plan; the arc module cannot enact it.
        const auto ferry_graph = project(OfflineStory::accept(manual_sets()[0], ferry_catalogue(), {}).value(), 0).value();
        auto ferry = Encounter::start(ferry_graph, {"arc-source-ferry"}).value();
        ferry.choose(token(ferry.view().value(), "pay")).value();
        ferry.choose(token(ferry.view().value(), "continue")).value();
        const auto watch_content = compose_watch(manual_library(), ferry_graph, ferry.capture().value(), 0).value();
        auto watch = WatchEncounter::start(watch_content, {"arc-source-watch"}).value();
        watch.choose(token(watch.view().value(), "fare-record")).value();
        watch.choose(token(watch.view().value(), "ask-again")).value();
        const auto selected = select_bargain(selection_library(), selection_catalogue(), {watch_content, watch.capture().value()}, 0, 1).value();
        const auto& source = *selected->content();
        Context ctx;
        ctx.goal = Goal::Shelter;
        ctx.recipient = source.request().actor;
        ctx.location = "quay";
        ctx.material = "planks";
        ctx.actors = {{ctx.recipient, ctx.location, ActorRole::Recipient, 0},
                      {"witness", ctx.location, ActorRole::Witness, evidence_known}};
        ctx.society = {"harbor-circle", "mixed-register", 0};
        std::vector<Component> pieces;
        for (auto r : {Role::Need, Role::Commitment, Role::Settlement, Role::Response})
            pieces.push_back(component(r, ctx, source.rule().promise_planks, source.rule().negotiated_planks));
        pieces[0].text = "Provide interim quay access for shelter relief; this is not a completed roof repair.";
        const auto direct = compose_direct(pieces, ctx).value();
        auto evidence = component(Role::Evidence, ctx, 0, 0);
        auto revision = component(Role::Renegotiation, ctx, 0, 0);
        const auto informed = introduce_evidence(*direct, evidence, revision).value();
        auto renamed_pieces = pieces;
        auto renamed_context = ctx;
        renamed_context.recipient = "other-recipient";
        renamed_context.actors[0].id = renamed_context.recipient;
        for (auto& n : renamed_pieces) {
            n.id = "copy-" + n.id;
            n.actor = renamed_context.recipient;
            n.recipient = renamed_context.recipient;
            n.text = "A paraphrase with the same supported meaning.";
        }
        std::reverse(renamed_pieces.begin(), renamed_pieces.end());
        const auto renamed = compose_direct(renamed_pieces, renamed_context).value();
        auto mechanical_pieces = pieces;
        mechanical_pieces[1].ordinary_due = 3;
        const auto mechanical = compose_direct(mechanical_pieces, ctx).value();
        std::vector<Shape> shapes;
        std::vector<std::pair<Shape, Mechanics>> variants;
        const std::array artifacts{direct, informed, renamed, mechanical};
        for (const auto& artifact : artifacts) {
            if (std::find(shapes.begin(), shapes.end(), artifact->shape()) == shapes.end())
                shapes.push_back(artifact->shape());
            const auto variant = std::pair{artifact->shape(), artifact->mechanics()};
            if (std::find(variants.begin(), variants.end(), variant) == variants.end())
                variants.push_back(variant);
        }
        out << "Proposed artifacts only; no model/provider, durable consequence or language-learning proof.\n"
            << "Accepted realizations=" << artifacts.size() << " structural-shapes=" << shapes.size()
            << " mechanical-variants=" << variants.size() << '\n'
            << "Conditional previews: direct=" << direct->traces().size() << " evidence-first=" << informed->traces().size() << '\n'
            << "Society=" << ctx.society.id << " echelon=" << ctx.society.echelon
            << " register=" << ctx.society.speaker_register << "; presentation/help cannot change membership or terms.\n";
        // This separate path executes the OLD checked host operations, not the new evidence grammar.
        return live_host_witness(out, selected->content());
    }
}

#else
int main() {
    return OfflineStory::Arc::run(std::cout);
}
#endif
