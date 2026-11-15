#include "arc.h"

#include <algorithm>
#include <string_view>
#include <tuple>
#include <utility>

namespace OfflineStory::Arc {
    namespace {
        struct Stop {
            Code code;
            std::string_view field;
        };
        void need(bool yes, Code code, std::string_view field) {
            if (!yes)
                throw Stop{code, field};
        }
        std::size_t ordinal(Role role) { return static_cast<std::size_t>(role); }
        struct Budget {
            const Limits& limits;
            Usage usage;
            std::size_t steps = 0;
            void work(std::size_t n = 1) {
                need(n <= limits.work - usage.work, Code::WorkLimit, "work");
                usage.work += n;
            }
            void bytes(std::size_t n) {
                need(n <= limits.storage_bytes - usage.storage_bytes, Code::StorageLimit, "storage");
                usage.storage_bytes += n;
            }
            void slots(std::size_t count, std::size_t size) {
                need(count <= limits.storage_bytes / size, Code::StorageLimit, "storage.product");
                bytes(count * size);
                work(count);
            }
            void text(std::string_view s, bool id) {
                need(!s.empty() && s.size() <= (id ? 64 : limits.text_length), Code::TextLimit, "text.length");
                need(s.size() <= limits.text_bytes - usage.text_bytes, Code::TextLimit, "text.bytes");
                usage.text_bytes += s.size();
                bytes(s.size() + 1);
                work(s.size() + 1);
                for (unsigned char c : s)
                    need(id ? ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
                            : (c >= 32 || c == '\n' || c == '\t'),
                         Code::Identity, "text.characters");
            }
            bool equal(std::string_view a, std::string_view b) {
                work(a.size() + 1);
                return a == b;
            }
        };
        using Nodes = std::span<const Component* const>;
        void valid_limits(const Limits& l) {
            for (auto f : {&Limits::nodes, &Limits::edges, &Limits::actors, &Limits::traces, &Limits::steps,
                           &Limits::text_length, &Limits::text_bytes, &Limits::storage_bytes, &Limits::work})
                need(l.*f <= maximum_limits.*f, Code::InvalidLimits, "limits");
        }
        std::array<std::size_t, 6> roles(Nodes nodes) {
            std::array<std::size_t, 6> positions;
            positions.fill(nodes.size());
            for (std::size_t i = 0; i < nodes.size(); ++i) {
                const auto r = ordinal(nodes[i]->role);
                need(r < positions.size(), Code::Role, "role.unknown");
                need(positions[r] == nodes.size(), Code::Role, "role.duplicate");
                positions[r] = i;
            }
            return positions;
        }
        struct ExpectedEdge {
            Role from, to;
            Link link;
        };
        constexpr std::array direct_edges{
            ExpectedEdge{Role::Need, Role::Commitment, Link::Continue},
            ExpectedEdge{Role::Commitment, Role::Settlement, Link::Promise},
            ExpectedEdge{Role::Commitment, Role::Response, Link::Refuse},
            ExpectedEdge{Role::Settlement, Role::Response, Link::Deliver},
            ExpectedEdge{Role::Settlement, Role::Response, Link::Break},
            ExpectedEdge{Role::Settlement, Role::Response, Link::Leave}
        };
        constexpr std::array evidence_edges{
            ExpectedEdge{Role::Need, Role::Evidence, Link::Continue},
            ExpectedEdge{Role::Evidence, Role::Renegotiation, Link::Present},
            ExpectedEdge{Role::Evidence, Role::Commitment, Link::Withhold},
            ExpectedEdge{Role::Renegotiation, Role::Commitment, Link::Revised}
        };
    }
    struct Builder {
        static Handle run(Nodes nodes, std::span<const Edge> edges, const Context& ctx, const Limits& limits) {
            valid_limits(limits);
            need(nodes.size() <= limits.nodes && edges.size() <= limits.edges && ctx.actors.size() <= limits.actors,
                 Code::CountLimit, "graph.dimensions");
            need(nodes.size() == 4 || nodes.size() == 6, Code::Topology, "grammar.nodes");
            Budget b{limits, {}};
            b.work(512); // Fixed-size role/edge setup, bounded canonical sorting and result bookkeeping.
            // Logical owned payload plus bounded scratch/control-block allowance, not an allocator-byte measurement.
            b.slots(1, sizeof(Artifact) + sizeof(Context) + 512);
            b.slots(nodes.size(), sizeof(Component) + sizeof(ShapeNode) + sizeof(ComponentMechanics));
            b.slots(edges.size(), sizeof(Edge) + sizeof(ShapeEdge));
            b.slots(ctx.actors.size(), sizeof(Actor));
            b.slots(limits.traces, sizeof(Trace));
            const auto pos = roles(nodes);
            const auto has = [&](Role r) { return pos[ordinal(r)] != nodes.size(); };
            const bool evidence = has(Role::Evidence);
            need(has(Role::Need) && has(Role::Commitment) && has(Role::Settlement) && has(Role::Response) &&
                     evidence == has(Role::Renegotiation) && evidence == (nodes.size() == 6),
                 Code::Topology, "grammar.roles");
            need(ctx.goal == Goal::Access || ctx.goal == Goal::Shelter, Code::Compatibility, "context.goal");
            need((ctx.facts & ~3u) == 0 && (ctx.facts & need_exists) != 0, Code::Compatibility, "context.facts");
            need(ctx.society.echelon <= 3 && ctx.presentation.player_proficiency <= 100, Code::Scope, "context.echelon");
            need(ctx.presentation.exposure == 5 || ctx.presentation.exposure == 15 || ctx.presentation.exposure == 40 ||
                     ctx.presentation.exposure == 75 || ctx.presentation.exposure == 100,
                 Code::Scope, "presentation.exposure");
            need(ctx.allowed_themes > 0 && (ctx.allowed_themes & ~3u) == 0 && ctx.maximum_explicitness <= 2,
                 Code::Scope, "context.scope");
            for (const auto* s : {&ctx.recipient, &ctx.location, &ctx.material, &ctx.society.id,
                                  &ctx.society.speaker_register, &ctx.presentation.interface_locale})
                b.text(*s, true);
            std::array<const Actor*, 2> actors{};
            for (std::size_t i = 0; i < ctx.actors.size(); ++i) {
                const auto& a = ctx.actors[i];
                b.text(a.id, true);
                b.text(a.location, true);
                const auto r = static_cast<std::size_t>(a.role);
                need(r < actors.size() && !actors[r], Code::Reference, "actor.role");
                need((a.information & ~evidence_known) == 0, Code::Information, "actor.initial-information");
                for (std::size_t j = 0; j < i; ++j)
                    need(!b.equal(a.id, ctx.actors[j].id), Code::Identity, "actor.duplicate");
                actors[r] = &a;
            }
            need(actors[0] && b.equal(actors[0]->id, ctx.recipient) && b.equal(actors[0]->location, ctx.location),
                 Code::Reference, "context.recipient");
            if (evidence)
                need(actors[1] && b.equal(actors[1]->location, ctx.location) &&
                         (actors[1]->information & evidence_known) != 0,
                     Code::Information, "evidence.witness");
            const std::array actions{Action::NoticeNeed, Action::ShareEvidence, Action::ReviseTerms,
                                     Action::PromiseOrRefuse, Action::CheckHandoff, Action::Respond};
            for (std::size_t i = 0; i < nodes.size(); ++i) {
                const auto& n = *nodes[i];
                for (const auto* s : {&n.id, &n.actor, &n.recipient, &n.location, &n.material, &n.society})
                    b.text(*s, true);
                b.text(n.text, false);
                for (std::size_t j = 0; j < i; ++j)
                    need(!b.equal(n.id, nodes[j]->id), Code::Identity, "component.duplicate");
                need(n.action == actions[ordinal(n.role)], Code::Action, "component.action");
                const auto* actor = actors[n.role == Role::Evidence ? 1 : 0];
                need(actor && b.equal(n.actor, actor->id) && b.equal(n.recipient, ctx.recipient) &&
                         b.equal(n.location, ctx.location) && b.equal(n.material, ctx.material) && b.equal(n.society, ctx.society.id),
                     Code::Reference, "component.bindings");
                need(n.goals > 0 && n.goals <= 3 && (n.goals & (1u << static_cast<unsigned>(ctx.goal))) != 0,
                     Code::Compatibility, "component.goal");
                const auto facts = n.role == Role::Need ? need_exists : n.role == Role::Evidence ? evidence_available : 0u;
                const auto information = n.role == Role::Renegotiation ? evidence_known
                                         : n.role == Role::Response ? outcome_known : 0u;
                need(n.required_facts == facts && (ctx.facts & facts) == facts, Code::Compatibility, "component.facts");
                need(n.required_information == information, Code::Information, "component.information");
                if (n.role == Role::Commitment)
                    need(n.ordinary_due >= 1 && n.ordinary_due <= 10 && n.revised_due >= 1 &&
                             n.revised_due <= n.ordinary_due && (!evidence || n.revised_due < n.ordinary_due),
                         Code::Compatibility, "obligation.quantities");
                else
                    need(n.ordinary_due == 0 && n.revised_due == 0, Code::Compatibility, "obligation.owner");
                const auto& s = n.scope;
                need(s.themes > 0 && (s.themes & ~3u) == 0 && s.explicitness <= 2 && s.minimum_echelon <= s.maximum_echelon &&
                         s.maximum_echelon <= 3 && (s.themes & ctx.allowed_themes) == s.themes &&
                         s.explicitness <= ctx.maximum_explicitness && (!s.adult_language || ctx.allow_adult_language) &&
                         ctx.society.echelon >= s.minimum_echelon && ctx.society.echelon <= s.maximum_echelon,
                     Code::Scope, "component.scope");
            }
            for (std::size_t i = 0; i < edges.size(); ++i) {
                const auto& e = edges[i];
                b.work();
                need(e.from < nodes.size() && e.to < nodes.size(), Code::Reference, "edge.reference");
                for (std::size_t j = 0; j < i; ++j) {
                    b.work();
                    need(e != edges[j], Code::Topology, "edge.duplicate");
                }
            }
            std::array<unsigned, 6> mark{};
            const auto visit = [&](auto&& self, std::size_t node) -> void {
                b.work();
                need(mark[node] != 1, Code::Cycle, "graph.cycle");
                if (mark[node] == 2)
                    return;
                mark[node] = 1;
                for (const auto& e : edges) {
                    b.work();
                    if (e.from == node)
                        self(self, e.to);
                }
                mark[node] = 2;
            };
            for (std::size_t i = 0; i < nodes.size(); ++i)
                visit(visit, i);
            const auto contains = [&](ExpectedEdge wanted) {
                for (const auto& e : edges) {
                    b.work();
                    if (nodes[e.from]->role == wanted.from && nodes[e.to]->role == wanted.to && e.link == wanted.link)
                        return true;
                }
                return false;
            };
            need(edges.size() == (evidence ? 9u : 6u), Code::Topology, "grammar.edges");
            for (std::size_t i = evidence ? 1 : 0; i < direct_edges.size(); ++i)
                need(contains(direct_edges[i]), Code::Topology, "grammar.direct-edge");
            if (evidence)
                for (auto e : evidence_edges)
                    need(contains(e), Code::Topology, "grammar.evidence-edge");

            std::vector<Trace> traces;
            traces.reserve(limits.traces);
            std::array<Role, 6> path{};
            std::array<Link, 5> choices{};
            const auto& promise = *nodes[pos[ordinal(Role::Commitment)]];
            const auto walk = [&](auto&& self, std::size_t index, std::size_t depth, unsigned info,
                                  Obligation obligation, int quantity, int due, bool favor) -> void {
                b.work();
                need(depth < path.size(), Code::CountLimit, "trace.depth");
                const auto& n = *nodes[index];
                need((info & n.required_information) == n.required_information, Code::Information, "trace.information");
                path[depth] = n.role;
                if (n.role == Role::Renegotiation)
                    info |= revised_terms;
                if (n.role == Role::Response) {
                    need(traces.size() < limits.traces, Code::CountLimit, "trace.count");
                    need(depth + 1 <= limits.steps - b.steps, Code::CountLimit, "trace.steps");
                    b.steps += depth + 1;
                    b.slots(depth + 1, sizeof(Role));
                    b.slots(depth, sizeof(Link));
                    Trace t;
                    t.roles.assign(path.data(), path.data() + depth + 1);
                    t.choices.assign(choices.data(), choices.data() + depth);
                    t.proposed_obligation = obligation;
                    t.recorded_quantity = quantity;
                    t.remaining_due = due;
                    t.recipient_information = info;
                    t.proposed_favor = favor;
                    traces.push_back(std::move(t));
                    return;
                }
                for (const auto& e : edges) {
                    b.work();
                    if (e.from != index)
                        continue;
                    auto next_info = info;
                    auto next_obligation = obligation;
                    int next_quantity = quantity, next_due = due;
                    bool next_favor = favor;
                    if (e.link == Link::Present)
                        next_info |= evidence_known;
                    if (e.link == Link::Promise) {
                        next_obligation = Obligation::Pending;
                        next_quantity = (info & revised_terms) != 0 ? promise.revised_due : promise.ordinary_due;
                        next_due = next_quantity;
                    }
                    if (e.link == Link::Refuse)
                        next_info |= outcome_known;
                    if (n.role == Role::Settlement) {
                        need(obligation == Obligation::Pending && due > 0, Code::Compatibility, "trace.obligation");
                        next_info |= outcome_known;
                        if (e.link == Link::Deliver) {
                            next_obligation = Obligation::Fulfilled;
                            next_due = 0;
                            next_favor = true;
                        } else if (e.link == Link::Break) {
                            next_obligation = Obligation::Broken;
                            next_due = 0;
                        }
                    }
                    choices[depth] = e.link;
                    self(self, e.to, depth + 1, next_info, next_obligation, next_quantity, next_due, next_favor);
                }
            };
            walk(walk, pos[ordinal(Role::Need)], 0, actors[0]->information, Obligation::None, 0, 0, false);
            std::sort(traces.begin(), traces.end(), [](const Trace& a, const Trace& c) { return a.choices < c.choices; });
            Shape shape;
            Mechanics mechanics{ctx.goal, ctx.facts, ctx.society.echelon, {}, {}};
            for (std::size_t i = 0; i < actors.size(); ++i)
                mechanics.initial_information[i] = actors[i] ? actors[i]->information : 0;
            shape.nodes.reserve(nodes.size());
            mechanics.components.reserve(nodes.size());
            for (const auto index : pos)
                if (index != nodes.size()) {
                    const auto& n = *nodes[index];
                    shape.nodes.push_back({n.role, n.action, n.required_facts, n.required_information});
                    mechanics.components.push_back({n.role, n.goals, n.ordinary_due, n.revised_due, n.scope});
                }
            shape.edges.reserve(edges.size());
            for (const auto& e : edges)
                shape.edges.push_back({nodes[e.from]->role, nodes[e.to]->role, e.link});
            std::sort(shape.edges.begin(), shape.edges.end(), [](const ShapeEdge& a, const ShapeEdge& c) {
                return std::tie(a.from, a.to, a.link) < std::tie(c.from, c.to, c.link);
            });
            Draft owned;
            owned.nodes.reserve(nodes.size());
            for (const auto* n : nodes)
                owned.nodes.push_back(*n);
            owned.edges.assign(edges.begin(), edges.end());
            return Handle(new const Artifact(std::move(owned), ctx, std::move(shape), std::move(mechanics), std::move(traces), b.usage));
        }
    };
    Artifact::Artifact(Draft d, Context c, Shape s, Mechanics m, std::vector<Trace> t, Usage u)
    : draft_(std::move(d)), context_(std::move(c)), shape_(std::move(s)), mechanics_(std::move(m)), traces_(std::move(t)), usage_(u) {}

    std::expected<Handle, Error> accept(const Draft& draft, const Context& ctx, const Limits& limits) {
        try {
            valid_limits(limits);
            need(draft.edges.size() <= limits.edges, Code::CountLimit, "graph.edges");
            need(draft.nodes.size() <= limits.nodes, Code::CountLimit, "graph.nodes");
            std::array<const Component*, 6> pointers{};
            for (std::size_t i = 0; i < draft.nodes.size(); ++i)
                pointers[i] = &draft.nodes[i];
            return Builder::run(Nodes(pointers.data(), draft.nodes.size()), draft.edges, ctx, limits);
        } catch (const Stop& e) {
            return std::unexpected(Error{e.code, std::string(e.field)});
        }
    }
    std::expected<Handle, Error> compose_direct(const std::vector<Component>& components, const Context& ctx, const Limits& limits) {
        try {
            valid_limits(limits);
            need(components.size() == 4 && components.size() <= limits.nodes, Code::CountLimit, "direct.nodes");
            std::array<const Component*, 4> pointers{};
            for (std::size_t i = 0; i < components.size(); ++i)
                pointers[i] = &components[i];
            const auto pos = roles(pointers);
            for (auto r : {Role::Need, Role::Commitment, Role::Settlement, Role::Response})
                need(pos[ordinal(r)] < pointers.size(), Code::Role, "direct.roles");
            std::array<Edge, 6> edges{};
            for (std::size_t i = 0; i < edges.size(); ++i)
                edges[i] = {pos[ordinal(direct_edges[i].from)], pos[ordinal(direct_edges[i].to)], direct_edges[i].link};
            return Builder::run(pointers, edges, ctx, limits);
        } catch (const Stop& e) {
            return std::unexpected(Error{e.code, std::string(e.field)});
        }
    }
    std::expected<Handle, Error> introduce_evidence(
        const Artifact& source, const Component& evidence, const Component& renegotiation, const Limits& limits
    ) {
        try {
            valid_limits(limits);
            need(source.draft().nodes.size() == 4 && evidence.role == Role::Evidence && renegotiation.role == Role::Renegotiation,
                 Code::Role, "rewrite.roles");
            std::array<const Component*, 6> pointers{};
            for (std::size_t i = 0; i < 4; ++i)
                pointers[i] = &source.draft().nodes[i];
            pointers[4] = &evidence;
            pointers[5] = &renegotiation;
            const auto pos = roles(pointers);
            std::array<Edge, 9> edges{};
            std::size_t count = 0;
            // Replace only the Need -> Commitment edge. Preserve the actual obligation/payoff relationships.
            for (const auto& e : source.draft().edges)
                if (pointers[e.from]->role != Role::Need)
                    edges[count++] = e;
            for (const auto& e : evidence_edges)
                edges[count++] = {pos[ordinal(e.from)], pos[ordinal(e.to)], e.link};
            need(count == edges.size(), Code::Topology, "rewrite.edges");
            return Builder::run(pointers, edges, source.context(), limits);
        } catch (const Stop& e) {
            return std::unexpected(Error{e.code, std::string(e.field)});
        }
    }
}
