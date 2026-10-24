#include "selection.h"

#include <limits>
#include <stdexcept>

namespace OfflineStory {
    namespace {
        struct Stop {
            SelectionCode code;
            std::string_view field;
        };
        void need(bool yes, SelectionCode code, std::string_view field) {
            if (!yes)
                throw Stop{code, field};
        }
        struct Budget {
            const SelectionLimits& l;
            Usage u;
            void work(std::size_t n) {
                need(n <= l.work - u.work, SelectionCode::WorkLimit, "work");
                u.work += n;
            }
            void bytes(std::size_t n) {
                need(n <= l.storage_bytes - u.storage_bytes, SelectionCode::StorageLimit, "storage");
                u.storage_bytes += n;
            }
            void text(std::size_t n) {
                need(n <= l.text_bytes - u.text_bytes, SelectionCode::TextLimit, "text");
                u.text_bytes += n;
                bytes(n);
                work(n);
            }
            void slots(std::size_t n, std::size_t size) {
                need(n <= l.storage_bytes / size, SelectionCode::StorageLimit, "storage.product");
                bytes(n * size);
                work(n);
            }
            bool equal(std::string_view a, std::string_view b) {
                work(a.size() + 1);
                return a == b;
            }
            void id(std::string_view s) {
                need(!s.empty() && s.size() <= 64, SelectionCode::InvalidLibrary, "metadata.identity");
                text(s.size() + 1);
                for (unsigned char c : s)
                    need(
                        (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_', SelectionCode::InvalidLibrary,
                        "metadata.identity"
                    );
            }
        };
        struct Candidate {
            std::size_t request, plan, flavour;
            ReliefBundle bundle;
            ReliefGoal goal;
        };
        // SplitMix64 output permutation of the first state (seed + golden-ratio increment), modulo finite compatible count.
        std::uint64_t draw(std::uint64_t seed) noexcept {
            auto x = seed + UINT64_C(0x9e3779b97f4a7c15);
            x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
            x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
            return x ^ (x >> 31);
        }
    }
    std::expected<SelectedBargainHandle, SelectionError> select_bargain(
        const BargainLibrary& lib,
        const BargainCatalogue& cat,
        const BargainContext& ctx,
        std::uint64_t seed,
        std::uint32_t algorithm,
        const SelectionConstraints& c,
        const SelectionLimits& l
    ) {
        try {
            for (auto f :
                 {&SelectionLimits::candidates, &SelectionLimits::draws, &SelectionLimits::text_bytes, &SelectionLimits::storage_bytes,
                  &SelectionLimits::work})
                need(l.*f <= maximum_selection_limits.*f, SelectionCode::InvalidLimits, "limits");
            for (auto f :
                 {&BargainLimits::texts, &BargainLimits::actions, &BargainLimits::requests, &BargainLimits::actors, &BargainLimits::rules,
                  &BargainLimits::names, &BargainLimits::candidates, &BargainLimits::nodes, &BargainLimits::choices,
                  &BargainLimits::text_length, &BargainLimits::text_bytes, &BargainLimits::storage_bytes, &BargainLimits::work})
                need(l.composition.*f <= maximum_bargain_limits.*f, SelectionCode::InvalidLimits, "composition.limits");
            need(
                algorithm == 1 && (c.required_initial & ~127u) == 0 && c.minimum_initial >= 2 && c.minimum_initial <= 7 && c.nodes <= 8 &&
                    c.edges <= 16 && c.max_material >= 0 && c.max_material <= 10 && c.max_coins >= 0 && c.max_coins <= 10 &&
                    c.max_pending >= 0 && c.max_pending <= 10,
                SelectionCode::InvalidConstraints, "constraints/version"
            );
            need(
                !lib.sets.requests.empty() && lib.sets.requests.size() <= 8 && !cat.host.rules.empty() && cat.host.rules.size() <= 4 &&
                    !lib.goals.empty() && lib.goals.size() <= 8 && !cat.roles.empty() && cat.roles.size() <= 2 && !cat.plans.empty() &&
                    cat.plans.size() <= 4 && !lib.bundles.empty() && lib.bundles.size() <= 2 && !lib.flavours.empty() &&
                    lib.flavours.size() <= 2,
                SelectionCode::InvalidLibrary, "dimensions"
            );
            Budget b{l};
            std::size_t product = 1;
            for (auto n : {lib.sets.requests.size(), cat.host.rules.size(), lib.bundles.size(), lib.flavours.size()}) {
                need(n <= std::numeric_limits<std::size_t>::max() / product, SelectionCode::CandidateLimit, "cartesian.overflow");
                product *= n;
            }
            need(product <= l.candidates, SelectionCode::CandidateLimit, "cartesian");
            b.slots(product, sizeof(Candidate));
            b.slots(1, sizeof(SelectedBargain) + sizeof(SelectionDescriptor));
            // Reserve the entire nested allowances BEFORE entering their allocating validation/compiler boundaries.
            // Two compositions (baseline and selected), plus a conservative full allowance for the source scratch copy.
            for (int i = 0; i < 3; ++i)
                b.bytes(l.composition.storage_bytes);
            for (int i = 0; i < 3; ++i) {
                b.work(l.composition.work);
                b.text(l.composition.text_bytes);
            }
            for (std::size_t i = 0; i < lib.goals.size(); ++i) {
                const auto& g = lib.goals[i];
                b.id(g.proposition);
                need(g.goal == ReliefGoal::Access || g.goal == ReliefGoal::Shelter, SelectionCode::InvalidLibrary, "goal");
                bool found = false;
                // Bounds of the nested composer are checked here before even scanning producer vectors.
                need(lib.sets.texts.size() <= l.composition.texts && lib.sets.texts.size() <= 40, SelectionCode::InvalidLibrary, "texts");
                for (const auto& t : lib.sets.texts)
                    if (b.equal(g.proposition, t.id))
                        found = t.role == ReliefTextRole::Proposition;
                need(found, SelectionCode::InvalidLibrary, "goal.proposition");
                for (std::size_t j = 0; j < i; ++j)
                    need(!b.equal(g.proposition, lib.goals[j].proposition), SelectionCode::InvalidLibrary, "goal.duplicate");
            }
            const auto permissions = [&](const auto& rows, const auto& targets, std::string_view field) {
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    b.id(rows[i].id);
                    need(rows[i].goals > 0 && rows[i].goals <= 3, SelectionCode::InvalidLibrary, field);
                    bool found = false;
                    for (const auto& target : targets)
                        found = b.equal(rows[i].id, target.id) || found;
                    need(found, SelectionCode::InvalidLibrary, field);
                    for (std::size_t j = 0; j < i; ++j)
                        need(!b.equal(rows[i].id, rows[j].id), SelectionCode::InvalidLibrary, field);
                }
            };
            need(cat.host.actors.size() <= 2, SelectionCode::InvalidLibrary, "actors");
            permissions(cat.roles, cat.host.actors, "role");
            permissions(cat.plans, cat.host.rules, "plan");
            for (std::size_t i = 0; i < lib.bundles.size(); ++i) {
                b.work(1);
                need(
                    lib.bundles[i] == ReliefBundle::OpenOffer || lib.bundles[i] == ReliefBundle::ExplainFirst,
                    SelectionCode::InvalidLibrary, "bundle"
                );
                if (i)
                    need(lib.bundles[i] != lib.bundles[0], SelectionCode::InvalidLibrary, "bundle.duplicate");
            }
            for (std::size_t i = 0; i < lib.flavours.size(); ++i) {
                b.work(1);
                need(lib.flavours[i] < 2 && (!i || lib.flavours[i] != lib.flavours[0]), SelectionCode::InvalidLibrary, "flavour");
            }
            // Eagerly validates ALL ordinary ingredients and the complete source pair, even if constraints will filter all candidates.
            need(lib.sets.requests[0].id.size() <= 64, SelectionCode::InvalidLibrary, "request.identity");
            auto baseline = compose_bargain(lib.sets, cat.host, ctx, {lib.sets.requests[0].id, 0}, l.composition);
            if (!baseline && !(baseline.error().code == Code::Context && baseline.error().field == "request.location"))
                return std::unexpected(SelectionError{SelectionCode::Composition, "baseline", std::move(baseline.error())});
            const auto permitted = [&](const auto& rows, std::string_view id, unsigned goal) {
                for (const auto& r : rows)
                    if (b.equal(id, r.id))
                        return (r.goals & goal) != 0;
                return false;
            };
            std::vector<Candidate> compatible;
            compatible.reserve(product);
            for (std::size_t ri = 0; ri < lib.sets.requests.size(); ++ri) {
                const auto& request = lib.sets.requests[ri];
                const PropositionGoal* goal = nullptr;
                for (const auto& g : lib.goals)
                    if (b.equal(request.proposition, g.proposition))
                        goal = &g;
                need(goal, SelectionCode::InvalidLibrary, "request.goal");
                const unsigned bit = 1u << static_cast<unsigned>(goal->goal);
                for (std::size_t pi = 0; pi < cat.host.rules.size(); ++pi) {
                    const auto& plan = cat.host.rules[pi];
                    for (auto bundle : lib.bundles)
                        for (auto flavour : lib.flavours) {
                            b.work(32); // Every Cartesian row, INCLUDING location/role/goal/cost/graph refusals.
                            const bool role = permitted(cat.roles, request.actor, bit), approved = permitted(cat.plans, plan.id, bit);
                            const auto location = ctx.completed.state.world.facts.declined ? ReliefLocation::Bank : ReliefLocation::Quay;
                            unsigned initial =
                                (1u << static_cast<unsigned>(ReliefIntent::Ask)) | (1u << static_cast<unsigned>(ReliefIntent::Refuse));
                            if (bundle == ReliefBundle::OpenOffer) {
                                initial |= 1u << static_cast<unsigned>(ReliefIntent::Promise);
                                if (ctx.completed.state.world.facts.planks >= plan.now_planks)
                                    initial |= 1u << static_cast<unsigned>(ReliefIntent::Help);
                                if (ctx.completed.state.world.facts.coins >= plan.coins)
                                    initial |= 1u << static_cast<unsigned>(ReliefIntent::Pay);
                            }
                            unsigned count = 0;
                            for (unsigned k = 0; k < 7; ++k)
                                count += (initial >> k) & 1u;
                            if (role && approved && location == request.location && plan.now_planks <= c.max_material &&
                                plan.coins <= c.max_coins && plan.promise_planks <= c.max_pending &&
                                (initial & c.required_initial) == c.required_initial && count >= c.minimum_initial && c.nodes >= 8 &&
                                c.edges >= (bundle == ReliefBundle::OpenOffer ? 16u : 14u))
                                compatible.push_back({ri, pi, flavour, bundle, goal->goal});
                        }
                }
            }
            need(!compatible.empty(), SelectionCode::Empty, "compatible");
            need(l.draws >= 1, SelectionCode::DrawLimit, "draws");
            b.work(8);
            const auto chosen = compatible[draw(seed) % compatible.size()];
            const auto& request = lib.sets.requests[chosen.request];
            const auto& plan = cat.host.rules[chosen.plan];
            // Descriptor/scratch strings are bounded by the successful eager composer. Charge copies before allocating them.
            const auto descriptor_text = lib.sets.id.size() + cat.host.id.size() + request.id.size() + request.proposition.size() +
                                         request.actor.size() + plan.id.size() + 6;
            b.text(descriptor_text);
            // Baseline allowance covers the scratch deep copy, including replacement of its selected rule string.
            b.text(plan.id.size() + 1);
            SelectionDescriptor descriptor{algorithm,   seed,          lib.sets.revision,   cat.host.revision, lib.sets.id,
                                           cat.host.id, request.id,    request.proposition, request.actor,     plan.id,
                                           chosen.goal, chosen.bundle, chosen.flavour,      product,           compatible.size()};
            auto sets = lib.sets;
            sets.requests[chosen.request].rule = plan.id;
            auto projected = compose_bargain(sets, cat.host, ctx, {request.id, chosen.flavour, chosen.bundle, true}, l.composition);
            if (!projected)
                return std::unexpected(SelectionError{SelectionCode::Composition, "projection", std::move(projected.error())});
            return SelectedBargainHandle(new const SelectedBargain(std::move(*projected), std::move(descriptor), b.u));
        } catch (const Stop& s) {
            return std::unexpected(SelectionError{s.code, std::string(s.field), {}});
        }
    }
    BargainComparison compare_bargain(const BargainArtifact& c) {
        const auto& rule = c.rule();
        BargainComparison out{
            {c.request().location,
             rule.now_planks,
             rule.coins,
             rule.promise_planks,
             rule.negotiated_planks,
             {ReliefResolution::Open, ReliefResolution::Helped, ReliefResolution::Paid, ReliefResolution::Promised, ReliefResolution::Open,
              ReliefResolution::Negotiated, ReliefResolution::Refused},
             {ReliefKnowledge::Discussed, ReliefKnowledge::SawHelp, ReliefKnowledge::SawPayment, ReliefKnowledge::HeardPromise,
              ReliefKnowledge::Discussed, ReliefKnowledge::HeardPromise, ReliefKnowledge::HeardRefusal},
             {0, 3, 1, 0, 0, 0, -1},
             {}},
            {}
        };
        const auto& nodes = c.definition()->nodes();
        if (nodes.size() != 8)
            throw std::logic_error("Not a supported bargain recipe");
        std::size_t edge_count = 0, copied = 0;
        const auto check_text = [&](std::string_view text) {
            constexpr auto cap = maximum_bargain_limits.text_bytes + 49;
            if (text.size() + 1 > cap - copied)
                throw std::logic_error("Recipe report text exceeds accepted bounds");
            copied += text.size() + 1;
        };
        check_text(c.definition()->id().value);
        for (const auto& node : nodes) {
            if (node.choices.size() > 16 - edge_count)
                throw std::logic_error("Recipe report edges exceed accepted bounds");
            edge_count += node.choices.size();
            check_text(node.id.value);
            check_text(node.text);
            for (const auto& edge : node.choices) {
                check_text(edge.id.value);
                check_text(edge.text);
            }
        }
        out.mechanics.edges.reserve(edge_count);
        out.presentation.reserve(17 + 2 * edge_count);
        out.presentation.push_back(c.definition()->id().value);
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            out.presentation.push_back(nodes[i].id.value);
            out.presentation.push_back(nodes[i].text);
            for (const auto& edge : nodes[i].choices) {
                std::size_t to = 0;
                while (to < nodes.size() && nodes[to].id != edge.target)
                    ++to;
                int intent = -1;
                for (const auto& a : c.sets().actions)
                    if (a.id == edge.id.value)
                        intent = static_cast<int>(a.kind);
                if (to == nodes.size() || (intent == -1 && edge.id.value != "close"))
                    throw std::logic_error("Invalid recipe edge");
                out.mechanics.edges.push_back({i, to, intent, edge.condition.has_value(), edge.effect.has_value()});
                out.presentation.push_back(edge.id.value);
                out.presentation.push_back(edge.text);
            }
        }
        return out;
    }
    BargainLibrary selection_library() {
        return {manual_bargain_sets(), {{"ramp-request", ReliefGoal::Access}, {"shelter-request", ReliefGoal::Shelter}}};
    }
    BargainCatalogue selection_catalogue() {
        // Both stewards may pursue both goals at their own location; the smaller interim plan is approved for BOTH needs.
        return {relief_catalogue(), {{"nera", 3}, {"oren", 3}}, {{"ramp", 3}, {"shelter", 2}}};
    }
}
