#include "bargain.h"

#include <limits>
#include <stdexcept>
#include <type_traits>

namespace OfflineStory {
    namespace {
        struct Policy {
            ReliefIntent kind;
            std::string_view id, operation, condition, target;
            ReliefResolution result;
        };
        constexpr Policy policies[] = {
            {ReliefIntent::Ask, "ask-terms", "learn_terms", "can_ask", "terms", ReliefResolution::Open},
            {ReliefIntent::Help, "help-now", "give_material", "can_help", "helped", ReliefResolution::Helped},
            {ReliefIntent::Pay, "pay", "pay_relief", "can_pay", "paid", ReliefResolution::Paid},
            {ReliefIntent::Promise, "promise", "record_promise", "can_promise", "promised", ReliefResolution::Promised},
            {ReliefIntent::Negotiate, "negotiate", "", "can_negotiate", "proposal", ReliefResolution::Open},
            {ReliefIntent::Commit, "commit", "record_negotiated", "can_negotiate", "promised", ReliefResolution::Negotiated},
            {ReliefIntent::Refuse, "refuse", "decline_relief", "can_refuse", "refused", ReliefResolution::Refused}
        };
        const Policy* policy(ReliefIntent kind) noexcept {
            for (const auto& p : policies)
                if (p.kind == kind)
                    return &p;
            return nullptr;
        }
        struct Stop {};
        struct Budget {
            BargainLimits limits;
            Usage usage;
            Error error{Code::Conflict, {}, {}};
            void need(bool ok, Code code, std::string_view field, std::string_view value = {}, std::size_t row = 0) {
                if (!ok) {
                    error = {code, std::string(field), std::string(value.substr(0, 64)), row, {}, {}};
                    throw Stop{};
                }
            }
            void work(std::size_t n) {
                need(n <= limits.work - usage.work, Code::WorkLimit, "work");
                usage.work += n;
            }
            void bytes(std::size_t n) {
                need(n <= limits.storage_bytes - usage.storage_bytes, Code::StorageLimit, "storage");
                usage.storage_bytes += n;
            }
            void slots(std::size_t n, std::size_t size) {
                need(n <= limits.storage_bytes / size, Code::StorageLimit, "storage");
                bytes(n * size);
                work(n);
            }
            void generated(std::size_t n) {
                need(n <= limits.text_length && n <= limits.text_bytes - usage.text_bytes, Code::TextLimit, "text");
                usage.text_bytes += n;
                bytes(n + 1);
                work(n + 1);
            }
            void text(std::string_view s, bool id, std::string_view field, std::size_t row = 0) {
                need(!s.empty() && (!id || s.size() <= 64), id ? Code::Identity : Code::TextLimit, field, {}, row);
                generated(s.size());
                for (unsigned char c : s)
                    need(
                        id ? (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' : c >= 32 || c == '\n' || c == '\t',
                        id ? Code::Identity : Code::TextLimit, field, {}, row
                    );
            }
            bool equal(std::string_view a, std::string_view c) {
                work(a.size() + 1);
                return a == c;
            }
            template <class Rows> std::size_t find(const Rows& rows, std::string_view key) {
                for (std::size_t i = 0; i < rows.size(); ++i)
                    if (equal(key, rows[i].id))
                        return i;
                return rows.size();
            }
            template <class Rows> void unique(const Rows& rows, std::string_view field) {
                for (std::size_t i = 0; i < rows.size(); ++i)
                    for (std::size_t j = 0; j < i; ++j)
                        need(!equal(rows[i].id, rows[j].id), Code::Duplicate, field, rows[i].id, i);
            }
            void names(const std::vector<std::string>& rows, std::string_view field, bool operation) {
                need(rows.size() <= limits.names, Code::CountLimit, field);
                slots(rows.size(), sizeof(std::string));
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    text(rows[i], true, field, i);
                    for (std::size_t j = 0; j < i; ++j)
                        need(!equal(rows[i], rows[j]), Code::Duplicate, field, rows[i], i);
                    bool supported = false;
                    for (const auto& p : policies)
                        supported = equal(rows[i], operation ? p.operation : p.condition) || supported;
                    need(supported, operation ? Code::UnknownOperation : Code::UnknownCondition, field, rows[i], i);
                }
            }
            bool contains(const std::vector<std::string>& rows, std::string_view key) {
                for (const auto& row : rows)
                    if (equal(key, row))
                        return true;
                return false;
            }
        };
        bool valid_limits(const BargainLimits& l) {
            for (const auto f :
                 {&BargainLimits::texts, &BargainLimits::actions, &BargainLimits::requests, &BargainLimits::actors, &BargainLimits::rules,
                  &BargainLimits::names, &BargainLimits::candidates, &BargainLimits::nodes, &BargainLimits::choices,
                  &BargainLimits::text_length, &BargainLimits::text_bytes, &BargainLimits::storage_bytes, &BargainLimits::work})
                if (l.*f > maximum_bargain_limits.*f)
                    return false;
            return true;
        }
        std::size_t digits(std::uint64_t n) noexcept {
            std::size_t count = 1;
            while (n >= 10) {
                n /= 10;
                ++count;
            }
            return count;
        }
    }
    BargainArtifact::BargainArtifact(
        const BargainSets& sets,
        const ReliefCatalogue& cat,
        const BargainContext& context,
        const BargainPick& pick,
        D::DefinitionHandle definition,
        std::size_t request,
        std::size_t rule,
        Usage usage
    )
    : sets_(sets),
      catalogue_(cat),
      context_(context),
      pick_(pick),
      definition_(std::move(definition)),
      request_(request),
      rule_(rule),
      usage_(usage) {}

    std::expected<BargainHandle, Error> compose_bargain(
        const BargainSets& sets,
        const ReliefCatalogue& cat,
        const BargainContext& context,
        const BargainPick& pick,
        const BargainLimits& limits
    ) {
        if (!context.previous)
            return std::unexpected(Error{Code::EmptyHandle, "context.previous", {}});
        if (!valid_limits(limits))
            return std::unexpected(Error{Code::InvalidLimits, "limits", {}});
        Budget b{limits};
        try {
            b.need(
                sets.texts.size() <= limits.texts && sets.actions.size() <= limits.actions && sets.requests.size() <= limits.requests &&
                    sets.requests.size() <= limits.candidates && cat.actors.size() <= limits.actors && cat.rules.size() <= limits.rules,
                Code::CountLimit, "rows"
            );
            b.need(pick.bundle == ReliefBundle::OpenOffer || pick.bundle == ReliefBundle::ExplainFirst, Code::Variant, "bundle");
            const bool explain = pick.bundle == ReliefBundle::ExplainFirst;
            const std::size_t edges = explain ? 14 : 16;
            b.need(limits.nodes >= 8 && limits.choices >= edges, Code::CountLimit, "graph");
            b.need(sets.origin == Origin::Manual && sets.revision != 0 && cat.revision != 0, Code::Identity, "origin/revision");
            b.need(pick.flavour < 2, Code::Variant, "flavour");
            b.bytes(sizeof(BargainArtifact) + sizeof(D::Draft) + sizeof(D::OperationKeys));
            b.slots(sets.texts.size(), sizeof(ReliefText));
            b.slots(sets.actions.size(), sizeof(ReliefAction));
            b.slots(sets.requests.size(), sizeof(ReliefRequest));
            b.slots(cat.actors.size(), sizeof(ReliefActor));
            b.slots(cat.rules.size(), sizeof(ReliefRule));
            b.slots(8, sizeof(D::Node));
            b.slots(edges, sizeof(D::Choice));
            b.slots(6, sizeof(D::ConditionKey));
            b.slots(6, sizeof(D::EffectKey));
            b.text(sets.id, true, "sets.id");
            b.text(cat.id, true, "catalogue.id");
            b.text(pick.request, true, "pick.request");
            b.names(cat.operations, "operations", true);
            b.names(cat.conditions, "conditions", false);
            for (std::size_t i = 0; i < sets.texts.size(); ++i) {
                const auto& t = sets.texts[i];
                b.text(t.id, true, "text.id", i);
                b.need(t.role >= ReliefTextRole::Proposition && t.role <= ReliefTextRole::Closing, Code::Conflict, "text.role", t.id, i);
                for (const auto& value : t.flavours)
                    b.text(value, false, "text.value", i);
            }
            for (std::size_t i = 0; i < sets.actions.size(); ++i) {
                const auto& a = sets.actions[i];
                for (const auto* key : {&a.id, &a.wording, &a.reply, &a.condition})
                    b.text(*key, true, "action", i);
                if (!a.operation.empty())
                    b.text(a.operation, true, "action.operation", i);
            }
            for (std::size_t i = 0; i < sets.requests.size(); ++i) {
                const auto& r = sets.requests[i];
                for (const auto* key : {&r.id, &r.actor, &r.rule, &r.proposition, &r.information, &r.proposal, &r.closing})
                    b.text(*key, true, "request", i);
            }
            for (std::size_t i = 0; i < cat.actors.size(); ++i) {
                b.text(cat.actors[i].id, true, "actor.id", i);
                b.text(cat.actors[i].name, false, "actor.name", i);
                const auto& a = cat.actors[i];
                b.need(
                    (b.equal(a.id, "nera") && a.location == ReliefLocation::Quay) ||
                        (b.equal(a.id, "oren") && a.location == ReliefLocation::Bank),
                    Code::UnknownActor, "actor.location", a.id, i
                );
            }
            for (std::size_t i = 0; i < cat.rules.size(); ++i) {
                const auto& r = cat.rules[i];
                b.text(r.id, true, "rule.id", i);
                b.work(8);
                b.need(
                    r.now_planks >= 1 && r.now_planks <= 10 && r.coins >= 1 && r.coins <= 10 && r.promise_planks >= 2 &&
                        r.promise_planks <= 10 && r.negotiated_planks >= 1 && r.negotiated_planks < r.promise_planks,
                    Code::Conflict, "rule.costs", r.id, i
                );
            }
            b.unique(sets.texts, "text.id");
            b.unique(sets.actions, "action.id");
            b.unique(sets.requests, "request.id");
            b.unique(cat.actors, "actor.id");
            b.unique(cat.rules, "rule.id");
            const auto text = [&](const std::string& key, ReliefTextRole role) -> const std::string& {
                const auto i = b.find(sets.texts, key);
                b.need(i != sets.texts.size(), Code::MissingReference, "text.reference", key);
                b.need(sets.texts[i].role == role, Code::Conflict, "text.role", key);
                return sets.texts[i].flavours[pick.flavour];
            };
            std::array<std::size_t, 7> ord;
            ord.fill(sets.actions.size());
            for (std::size_t i = 0; i < sets.actions.size(); ++i) {
                const auto& a = sets.actions[i];
                const auto* p = policy(a.kind);
                b.work(16);
                b.need(p, Code::Conflict, "action.kind", a.id, i);
                const auto k = static_cast<std::size_t>(a.kind);
                b.need(ord[k] == sets.actions.size(), Code::Duplicate, "action.kind", a.id, i);
                ord[k] = i;
                b.need(b.equal(a.id, p->id) && a.claimed == p->result, Code::Conflict, "action.meaning", a.id, i);
                b.need(
                    b.equal(a.operation, p->operation) && (a.operation.empty() || b.contains(cat.operations, a.operation)),
                    Code::UnknownOperation, "action.operation", a.operation, i
                );
                b.need(
                    b.equal(a.condition, p->condition) && b.contains(cat.conditions, a.condition), Code::UnknownCondition,
                    "action.condition", a.condition, i
                );
                static_cast<void>(text(a.wording, ReliefTextRole::Wording));
                static_cast<void>(text(a.reply, ReliefTextRole::Reply));
            }
            for (const auto i : ord)
                b.need(i != sets.actions.size(), Code::MissingReference, "required.intent");
            const auto action = [&](ReliefIntent kind) -> const ReliefAction& { return sets.actions[ord[static_cast<std::size_t>(kind)]]; };
            b.need(b.equal(action(ReliefIntent::Promise).reply, action(ReliefIntent::Commit).reply), Code::Conflict, "promise.reply");
            std::size_t selected = sets.requests.size();
            for (std::size_t i = 0; i < sets.requests.size(); ++i) {
                const auto& r = sets.requests[i];
                b.work(8);
                const auto actor = b.find(cat.actors, r.actor);
                b.need(actor != cat.actors.size(), Code::UnknownActor, "request.actor", r.actor, i);
                b.need(r.location == cat.actors[actor].location, Code::Conflict, "request.location", r.id, i);
                b.need(b.find(cat.rules, r.rule) != cat.rules.size(), Code::MissingReference, "request.rule", r.rule, i);
                static_cast<void>(text(r.proposition, ReliefTextRole::Proposition));
                static_cast<void>(text(r.information, ReliefTextRole::Information));
                static_cast<void>(text(r.proposal, ReliefTextRole::Information));
                static_cast<void>(text(r.closing, ReliefTextRole::Closing));
                if (b.equal(r.id, pick.request))
                    selected = i;
            }
            b.need(selected != sets.requests.size(), Code::MissingReference, "pick.request", pick.request);
            for (const auto* s :
                 {&context.completed.dialogue.definition.value, &context.completed.dialogue.session.value,
                  &context.completed.dialogue.node.value}) {
                b.need(!s->empty() && s->size() <= 512, Code::Context, "context.identity");
                b.generated(s->size());
            }
            b.work(3 * 513 * 5 + 64); // Conservative reservation for bounded source factory/lookup validation.
            auto validator = WatchEncounter::start(context.previous, {"bargain-validation"}).value();
            const auto restored = validator.restore(context.completed);
            if (!restored)
                return std::unexpected(restored.error());
            const auto* node = context.previous->definition()->find_node(context.completed.dialogue.node);
            b.need(node && node->kind == D::NodeKind::Terminal, Code::Context, "context.completed");
            const auto& request = sets.requests[selected];
            b.need(
                request.location == (context.completed.state.world.facts.declined ? ReliefLocation::Bank : ReliefLocation::Quay),
                Code::Context, "request.location"
            );
            const auto rule = b.find(cat.rules, request.rule);
            const auto& proposition = text(request.proposition, ReliefTextRole::Proposition);
            const auto& info = text(request.information, ReliefTextRole::Information);
            const auto& proposal = text(request.proposal, ReliefTextRole::Information);
            const auto& closing = text(request.closing, ReliefTextRole::Closing);
            const std::string_view location = request.location == ReliefLocation::Bank ? "On the original bank. " : "On the harbor quay. ";
            b.generated(
                7 + sets.id.size() + 1 + cat.id.size() + 1 + digits(cat.revision) + 1 + request.id.size() + 1 +
                context.completed.dialogue.definition.value.size() + 3 + (explain ? 8 : 0) +
                (pick.include_plan_identity ? 3 + request.rule.size() : 0)
            );
            b.generated(location.size() + proposition.size());
            b.generated(text(action(ReliefIntent::Ask).reply, ReliefTextRole::Reply).size() + 1 + info.size());
            b.generated(text(action(ReliefIntent::Negotiate).reply, ReliefTextRole::Reply).size() + 1 + proposal.size());
            for (const auto& p : policies) {
                b.generated(text(action(p.kind).reply, ReliefTextRole::Reply).size());
                b.generated(p.condition.size());
                if (!p.operation.empty())
                    b.generated(p.operation.size());
            }
            b.generated(closing.size());
            for (const auto s : std::initializer_list<std::string_view>{
                     "request", "request", "terms", "proposal", "helped", "paid", "promised", "refused", "closing"
                 })
                b.generated(s.size());
            const auto charge_choice = [&](ReliefIntent k) {
                const auto& p = *policy(k);
                b.generated(p.id.size());
                b.generated(text(action(k).wording, ReliefTextRole::Wording).size());
                b.generated(p.target.size());
                b.generated(p.condition.size());
                if (!p.operation.empty())
                    b.generated(p.operation.size());
            };
            constexpr ReliefIntent open[] = {ReliefIntent::Ask,     ReliefIntent::Help,      ReliefIntent::Pay,
                                             ReliefIntent::Promise, ReliefIntent::Negotiate, ReliefIntent::Refuse};
            constexpr ReliefIntent explain_open[] = {ReliefIntent::Ask, ReliefIntent::Refuse};
            constexpr ReliefIntent open_terms[] = {ReliefIntent::Help, ReliefIntent::Negotiate, ReliefIntent::Refuse};
            constexpr ReliefIntent explain_terms[] = {
                ReliefIntent::Help, ReliefIntent::Pay, ReliefIntent::Promise, ReliefIntent::Negotiate, ReliefIntent::Refuse
            };
            const auto opening_kinds = explain ? std::span<const ReliefIntent>(explain_open) : std::span<const ReliefIntent>(open);
            const auto terms_kinds = explain ? std::span<const ReliefIntent>(explain_terms) : std::span<const ReliefIntent>(open_terms);
            for (const auto k : opening_kinds)
                charge_choice(k);
            for (const auto k : terms_kinds)
                charge_choice(k);
            for (const auto k : {ReliefIntent::Commit, ReliefIntent::Help, ReliefIntent::Refuse})
                charge_choice(k);
            for (int i = 0; i < 4; ++i)
                for (const auto s : {"close", "Close this conversation", "closing"})
                    b.generated(std::string_view(s).size());
            b.work(b.usage.text_bytes);
            D::Draft draft{
                {"relief-" + sets.id + "-" + cat.id + "-" + std::to_string(cat.revision) + "-" + request.id + "-" +
                 context.completed.dialogue.definition.value + "-f" + std::to_string(pick.flavour) + (explain ? "-explain" : "") +
                 (pick.include_plan_identity ? "-p-" + request.rule : "")},
                sets.revision,
                {"request"},
                {}
            };
            draft.nodes.reserve(8);
            const auto choice = [&](ReliefIntent k, bool effect = true) {
                const auto& p = *policy(k);
                return D::Choice{
                    {std::string(p.id)},
                    text(action(k).wording, ReliefTextRole::Wording),
                    {std::string(p.target)},
                    D::ConditionKey{std::string(p.condition)},
                    effect && !p.operation.empty() ? std::optional<D::EffectKey>{D::EffectKey{std::string(p.operation)}} : std::nullopt
                };
            };
            D::Node opening{{"request"}, std::string(location) + proposition, D::NodeKind::Prompt, {}};
            opening.choices.reserve(opening_kinds.size());
            for (const auto k : opening_kinds)
                opening.choices.push_back(choice(k));
            draft.nodes.push_back(std::move(opening));
            D::Node terms{{"terms"}, text(action(ReliefIntent::Ask).reply, ReliefTextRole::Reply) + " " + info, D::NodeKind::Prompt, {}};
            terms.choices.reserve(terms_kinds.size());
            for (const auto k : terms_kinds)
                terms.choices.push_back(choice(k));
            draft.nodes.push_back(std::move(terms));
            draft.nodes.push_back(
                {{"proposal"},
                 text(action(ReliefIntent::Negotiate).reply, ReliefTextRole::Reply) + " " + proposal,
                 D::NodeKind::Prompt,
                 {choice(ReliefIntent::Commit), choice(ReliefIntent::Help), choice(ReliefIntent::Refuse)}}
            );
            for (const auto k : {ReliefIntent::Help, ReliefIntent::Pay, ReliefIntent::Promise, ReliefIntent::Refuse})
                draft.nodes.push_back(
                    {{std::string(policy(k)->target)},
                     text(action(k).reply, ReliefTextRole::Reply),
                     D::NodeKind::Prompt,
                     {{{"close"}, "Close this conversation", {"closing"}, {}, {}}}}
                );
            draft.nodes.push_back({{"closing"}, closing, D::NodeKind::Terminal, {}});
            D::OperationKeys operations;
            operations.conditions.reserve(6);
            operations.effects.reserve(6);
            for (const auto& c : cat.conditions)
                operations.conditions.push_back({c});
            for (const auto& op : cat.operations)
                operations.effects.push_back({op});
            auto compiled = D::compile(draft, operations);
            if (!compiled)
                return std::unexpected(Error{Code::Compile, "bargain", {}, 0, std::move(compiled.error()), {}});
            return BargainHandle(new const BargainArtifact(sets, cat, context, pick, std::move(*compiled), selected, rule, b.usage));
        } catch (const Stop&) {
            return std::unexpected(std::move(b.error));
        }
    }

    BargainEncounter::BargainEncounter(BargainHandle content, D::Session session)
    : content_(std::move(content)), session_(std::move(session)), state_{content_->context().completed.state.world} {}
    std::expected<BargainEncounter, Error> BargainEncounter::start(BargainHandle content, D::SessionId id) {
        if (!content)
            return std::unexpected(Error{Code::EmptyHandle, "bargain", {}});
        auto session = D::Session::start(content->definition(), std::move(id));
        if (!session)
            return std::unexpected(Error{Code::Resume, "session", {}, 0, {}, session.error()});
        return BargainEncounter(std::move(content), std::move(*session));
    }
    bool BargainEncounter::coherent(const ReliefState& s) const noexcept {
        if (!content_ || !valid_state(s.world))
            return false;
        auto facts = s.world.facts;
        const auto& base = content_->context().completed.state.world;
        facts.coins = base.facts.coins;
        facts.planks = base.facts.planks;
        if (facts != base.facts || s.world.knowledge != base.knowledge || s.world.attitude != base.attitude ||
            s.world.believes_unlicensed != base.believes_unlicensed)
            return false;
        const auto& r = content_->rule();
        switch (s.resolution) {
            case ReliefResolution::Open:
                return s.planks_due == 0 && s.requester_attitude == 0 &&
                       s.requester_knows == (s.terms_known ? ReliefKnowledge::Discussed : ReliefKnowledge::None);
            case ReliefResolution::Helped:
                return s.planks_due == 0 && s.requester_attitude == 3 && s.requester_knows == ReliefKnowledge::SawHelp;
            case ReliefResolution::Paid:
                return s.planks_due == 0 && s.requester_attitude == 1 && s.requester_knows == ReliefKnowledge::SawPayment;
            case ReliefResolution::Promised:
                return s.planks_due == r.promise_planks && s.requester_attitude == 0 && s.requester_knows == ReliefKnowledge::HeardPromise;
            case ReliefResolution::Negotiated:
                return s.terms_known && s.planks_due == r.negotiated_planks && s.requester_attitude == 0 &&
                       s.requester_knows == ReliefKnowledge::HeardPromise;
            case ReliefResolution::Refused:
                return s.planks_due == 0 && s.requester_attitude == -1 && s.requester_knows == ReliefKnowledge::HeardRefusal;
        }
        return false;
    }
    bool BargainEncounter::eligible(ReliefIntent kind) const noexcept {
        if (!coherent(state_) || state_.resolution != ReliefResolution::Open)
            return false;
        switch (kind) {
            case ReliefIntent::Ask:
                return !state_.terms_known;
            case ReliefIntent::Help:
                return state_.world.facts.planks >= content_->rule().now_planks;
            case ReliefIntent::Pay:
                return state_.world.facts.coins >= content_->rule().coins;
            case ReliefIntent::Negotiate:
            case ReliefIntent::Commit:
                return state_.terms_known;
            case ReliefIntent::Promise:
            case ReliefIntent::Refuse:
                return true;
        }
        return false;
    }
    bool BargainEncounter::test(const D::ConditionKey& key) const {
        for (const auto& p : policies)
            if (key.value == p.condition)
                return eligible(p.kind);
        throw std::logic_error("Unknown relief condition");
    }
    std::expected<D::View, D::SessionError> BargainEncounter::view() {
        return session_.view([this](const auto& k) { return test(k); });
    }
    std::expected<D::Transition, D::SessionError> BargainEncounter::choose(const D::Selection& selected) {
        static_assert(std::is_nothrow_copy_assignable_v<ReliefState>);
        return session_.choose_with_transition(
            selected, [this](const auto& k) { return test(k); },
            [this](const auto& key) {
                const Policy* chosen = nullptr;
                for (const auto& p : policies)
                    if (!p.operation.empty() && key.value == p.operation)
                        chosen = &p;
                if (!chosen || !eligible(chosen->kind))
                    throw std::logic_error("Invalid relief effect");
                auto prepared = state_;
                const auto& r = content_->rule();
                prepared.resolution = chosen->result;
                switch (chosen->kind) {
                    case ReliefIntent::Ask:
                        prepared.terms_known = true;
                        prepared.requester_knows = ReliefKnowledge::Discussed;
                        break;
                    case ReliefIntent::Help:
                        prepared.world.facts.planks -= r.now_planks;
                        prepared.requester_knows = ReliefKnowledge::SawHelp;
                        prepared.requester_attitude = 3;
                        break;
                    case ReliefIntent::Pay:
                        prepared.world.facts.coins -= r.coins;
                        prepared.requester_knows = ReliefKnowledge::SawPayment;
                        prepared.requester_attitude = 1;
                        break;
                    case ReliefIntent::Promise:
                    case ReliefIntent::Commit:
                        prepared.planks_due = chosen->kind == ReliefIntent::Promise ? r.promise_planks : r.negotiated_planks;
                        prepared.requester_knows = ReliefKnowledge::HeardPromise;
                        break;
                    case ReliefIntent::Refuse:
                        prepared.requester_knows = ReliefKnowledge::HeardRefusal;
                        prepared.requester_attitude = -1;
                        break;
                    case ReliefIntent::Negotiate:
                        throw std::logic_error("Negotiation has no effect");
                }
                if (!coherent(prepared))
                    throw std::logic_error("Invalid staged relief state");
                state_ = prepared;
                if (effects_ != std::numeric_limits<std::uint64_t>::max())
                    ++effects_;
            }
        );
    }
    std::expected<BargainCheckpoint, D::SessionError> BargainEncounter::capture() {
        auto saved = session_.snapshot();
        if (!saved)
            return std::unexpected(saved.error());
        return BargainCheckpoint{std::move(*saved), state_};
    }
    std::expected<void, Error> BargainEncounter::restore(const BargainCheckpoint& saved) {
        if (!content_)
            return std::unexpected(Error{Code::EmptyHandle, "bargain", {}});
        if (!coherent(saved.state))
            return std::unexpected(Error{Code::Context, "checkpoint.state", {}});
        auto restored = D::Session::resume(content_->definition(), saved.dialogue);
        if (!restored)
            return std::unexpected(Error{Code::Resume, "checkpoint.dialogue", {}, 0, {}, restored.error()});
        static_assert(std::is_nothrow_move_assignable_v<D::Session>);
        std::expected<void, Error> success;
        session_ = std::move(*restored);
        state_ = saved.state;
        return success;
    }
    bool BargainEncounter::resources(int coins, int planks) noexcept {
        if (!content_ || state_.resolution != ReliefResolution::Open)
            return false;
        auto prepared = state_;
        prepared.world.facts.coins = coins;
        prepared.world.facts.planks = planks;
        if (!coherent(prepared))
            return false;
        state_ = prepared;
        return true;
    }
}
