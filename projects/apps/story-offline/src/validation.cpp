#include "story.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace OfflineStory {
    namespace {
        struct Stop {}; // Private fail-fast validation only; allocation exceptions still propagate.
        bool valid_limits(const Limits& limits) {
            for (const auto field :
                 {&Limits::actors, &Limits::outcomes, &Limits::actions, &Limits::flavours, &Limits::names, &Limits::reward_refs,
                  &Limits::text_length, &Limits::text_bytes, &Limits::storage_bytes, &Limits::work})
                if (limits.*field > maximum_limits.*field)
                    return false;
            return true;
        }
        struct Budget {
            Limits limits;
            Usage usage;
            Error error{Code::Conflict, {}, {}};
            explicit Budget(const Limits& requested)
            : limits(requested) {}
            [[noreturn]] void fail(Code code, std::string_view field, std::size_t row = 0, std::string_view value = {}) {
                error = {code, std::string(field), std::string(value.substr(0, 64)), row, {}, {}};
                throw Stop{};
            }
            void need(bool ok, Code code, std::string_view field, std::size_t row = 0, std::string_view value = {}) {
                if (!ok)
                    fail(code, field, row, value);
            }
            void work(std::size_t amount = 1) {
                need(amount <= limits.work - usage.work, Code::WorkLimit, "work");
                usage.work += amount;
            }
            void storage(std::size_t amount) {
                need(amount <= limits.storage_bytes - usage.storage_bytes, Code::StorageLimit, "storage");
                usage.storage_bytes += amount;
            }
            void slots(std::size_t count, std::size_t item) {
                need(count <= limits.storage_bytes / item, Code::StorageLimit, "storage");
                storage(count * item);
                work(count);
            }
            void generated(std::size_t length) {
                need(length <= limits.text_length && length <= limits.text_bytes - usage.text_bytes, Code::TextLimit, "text");
                usage.text_bytes += length;
                storage(length + 1); // Length <= 1024, so terminator arithmetic is bounded.
                work(length + 1);
            }
            void text(std::string_view value, bool id, std::string_view field, std::size_t row = 0) {
                need(!value.empty() && (!id || value.size() <= 64), id ? Code::Identity : Code::TextLimit, field, row);
                generated(value.size());
                for (const unsigned char c : value) {
                    const bool identifier = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
                    need(id ? identifier : (c >= 32 || c == '\n' || c == '\t'), id ? Code::Identity : Code::TextLimit, field, row);
                }
            }
            bool equal(std::string_view a, std::string_view b) {
                work(a.size() + 1);
                return a == b;
            }
            template <class Rows, class Key> std::size_t find(const Rows& rows, std::string_view sought, Key key) {
                for (std::size_t i = 0; i < rows.size(); ++i)
                    if (equal(sought, key(rows[i])))
                        return i;
                return rows.size();
            }
            void names(const std::vector<std::string>& rows, std::size_t cap, std::string_view field) {
                need(rows.size() <= cap, Code::CountLimit, field);
                slots(rows.size(), sizeof(std::string));
                for (std::size_t i = 0; i < rows.size(); ++i)
                    text(rows[i], true, field, i);
            }
            template <class Rows, class Key> void unique(const Rows& rows, Key key, std::string_view field) {
                for (std::size_t i = 0; i < rows.size(); ++i)
                    for (std::size_t j = 0; j < i; ++j)
                        need(!equal(key(rows[i]), key(rows[j])), Code::Duplicate, field, i, key(rows[i]));
            }
        };
        constexpr std::string_view operation(Method method) {
            switch (method) {
                case Method::Pay:
                    return "pay_fare";
                case Method::Repair:
                    return "repair_hull";
                case Method::Decline:
                    return "decline";
            }
            return {};
        }
        constexpr std::string_view condition(Method method) {
            switch (method) {
                case Method::Pay:
                    return "can_pay";
                case Method::Repair:
                    return "can_repair";
                case Method::Decline:
                    return "can_decline";
            }
            return {};
        }
        const auto identity = [](const auto& value) -> std::string_view { return value; };
        const auto id = [](const auto& value) -> std::string_view { return value.id; };

        void preflight(Budget& b, const CandidateSet& set, const Catalogue& cat, const Context& context) {
            b.need(
                cat.actors.size() <= b.limits.actors && cat.outcomes.size() <= b.limits.outcomes && set.actions.size() <= b.limits.actions,
                Code::CountLimit, "rows"
            );
            b.storage(sizeof(Accepted));
            b.slots(cat.actors.size(), sizeof(Actor));
            b.slots(cat.outcomes.size(), sizeof(Outcome));
            b.slots(set.actions.size(), sizeof(Action));
            b.text(set.id, true, "set.id");
            b.text(set.proposition, false, "set.proposition");
            b.names(cat.operations, b.limits.names, "catalogue.operations");
            b.names(cat.conditions, b.limits.names, "catalogue.conditions");
            b.names(cat.rewards, b.limits.names, "catalogue.rewards");
            for (const auto& observer : context.observers)
                b.text(observer, true, "context.observers");
            for (std::size_t i = 0; i < cat.actors.size(); ++i) {
                b.work();
                b.text(cat.actors[i].id, true, "actor.id", i);
                b.text(cat.actors[i].name, false, "actor.name", i);
            }
            for (std::size_t i = 0; i < cat.outcomes.size(); ++i) {
                const auto& row = cat.outcomes[i];
                b.work();
                for (const auto* name : {&row.id, &row.actor, &row.operation, &row.condition})
                    b.text(*name, true, "outcome", i);
                b.names(row.rewards, b.limits.reward_refs, "outcome.rewards");
            }
            for (std::size_t i = 0; i < set.actions.size(); ++i) {
                const auto& row = set.actions[i];
                b.work();
                b.need(!row.flavours.empty() && row.flavours.size() <= b.limits.flavours, Code::CountLimit, "action.flavours", i);
                b.slots(row.flavours.size(), sizeof(Flavour));
                for (const auto* name : {&row.id, &row.actor, &row.operation, &row.condition, &row.outcome})
                    b.text(*name, true, "action", i);
                b.names(row.rewards, b.limits.reward_refs, "action.rewards");
                for (const auto& flavour : row.flavours) {
                    b.text(flavour.id, true, "flavour.id", i);
                    b.text(flavour.wording, false, "flavour.wording", i);
                    b.text(flavour.response, false, "flavour.response", i);
                }
            }
        }
    }

    Accepted::Accepted(const CandidateSet& set, const Catalogue& cat, const Context& context, Usage usage)
    : set_(set), catalogue_(cat), context_(context), usage_(usage) {}

    std::expected<AcceptedHandle, Error>
    accept(const CandidateSet& set, const Catalogue& cat, const Context& context, const Limits& limits) {
        if (!valid_limits(limits))
            return std::unexpected(Error{Code::InvalidLimits, "limits", {}});
        Budget b(limits);
        try {
            preflight(b, set, cat, context);
            b.need(set.origin == Origin::Manual && set.revision != 0 && set.actions.size() >= 2, Code::Identity, "set");
            b.unique(cat.actors, id, "actor.id");
            b.unique(cat.outcomes, id, "outcome.id");
            b.unique(set.actions, id, "action.id");
            b.unique(cat.operations, identity, "catalogue.operations");
            b.unique(cat.conditions, identity, "catalogue.conditions");
            b.unique(cat.rewards, identity, "catalogue.rewards");
            b.unique(context.observers, identity, "context.observers");
            for (const auto& key : cat.operations)
                b.need(
                    b.equal(key, "pay_fare") || b.equal(key, "repair_hull") || b.equal(key, "decline"), Code::UnknownOperation,
                    "catalogue.operations", 0, key
                );
            for (const auto& key : cat.conditions)
                b.need(
                    b.equal(key, "can_pay") || b.equal(key, "can_repair") || b.equal(key, "can_decline"), Code::UnknownCondition,
                    "catalogue.conditions", 0, key
                );
            for (const auto& key : cat.rewards)
                b.need(b.equal(key, "passage"), Code::UnknownReward, "catalogue.rewards", 0, key);
            for (const auto& observer : context.observers)
                b.need(b.find(cat.actors, observer, id) != cat.actors.size(), Code::UnknownActor, "context.observers", 0, observer);
            b.work(32);
            b.need(
                valid_state(context.state) && !context.state.facts.passage && !context.state.facts.declined, Code::Context, "context.state"
            );
            for (const auto knowledge : context.state.knowledge)
                b.need(knowledge == Knowledge::None, Code::Context, "context.initial_knowledge");
            for (std::size_t i = 0; i < cat.outcomes.size(); ++i) {
                const auto& row = cat.outcomes[i];
                b.work(32);
                b.need(b.find(cat.actors, row.actor, id) != cat.actors.size(), Code::UnknownActor, "outcome.actor", i, row.actor);
                b.need(
                    !operation(row.method).empty() && b.equal(row.operation, operation(row.method)) &&
                        b.find(cat.operations, row.operation, identity) != cat.operations.size(),
                    Code::UnknownOperation, "outcome.operation", i, row.operation
                );
                b.need(
                    b.equal(row.condition, condition(row.method)) &&
                        b.find(cat.conditions, row.condition, identity) != cat.conditions.size(),
                    Code::UnknownCondition, "outcome.condition", i, row.condition
                );
                const auto& c = row.consequence;
                const bool pay = row.method == Method::Pay, repair = row.method == Method::Repair;
                b.need(
                    c.coins >= 0 && c.coins <= 100 && c.planks >= 0 && c.planks <= 100 &&
                        (pay      ? c.coins > 0 && c.planks == 0
                         : repair ? c.planks > 0 && c.coins == 0
                                  : c.coins == 0 && c.planks == 0) &&
                        c.passage == (pay || repair) && c.paid == pay && c.repaired == repair && c.declined == (!pay && !repair) &&
                        c.attitude[0] >= 0 && c.attitude[0] <= 10 && c.attitude[1] <= 0 && c.attitude[1] >= -10 && c.attitude[2] == 0,
                    Code::Conflict, "outcome.consequence", i
                );
                b.need(
                    pay || repair ? c.attitude[0] > 0 && c.attitude[1] < 0 : c.attitude == std::array<int, 3>{}, Code::Conflict,
                    "outcome.attitude", i
                );
                b.unique(row.rewards, identity, "outcome.rewards");
                for (const auto& reward : row.rewards)
                    b.need(b.find(cat.rewards, reward, identity) != cat.rewards.size(), Code::UnknownReward, "outcome.rewards", i, reward);
                b.need(
                    pay || repair ? row.rewards.size() == 1 && b.equal(row.rewards[0], "passage") : row.rewards.empty(), Code::Conflict,
                    "outcome.rewards", i
                );
            }
            std::array<bool, 3> methods{};
            std::size_t available = 0;
            for (std::size_t i = 0; i < set.actions.size(); ++i) {
                const auto& row = set.actions[i];
                b.work(32);
                b.need(b.find(cat.actors, row.actor, id) != cat.actors.size(), Code::UnknownActor, "action.actor", i, row.actor);
                b.need(
                    b.find(cat.operations, row.operation, identity) != cat.operations.size(), Code::UnknownOperation, "action.operation", i,
                    row.operation
                );
                b.need(
                    b.find(cat.conditions, row.condition, identity) != cat.conditions.size(), Code::UnknownCondition, "action.condition", i,
                    row.condition
                );
                const auto index = b.find(cat.outcomes, row.outcome, id);
                b.need(index != cat.outcomes.size(), Code::MissingReference, "action.outcome", i, row.outcome);
                const auto& approved = cat.outcomes[index];
                b.need(
                    b.equal(row.actor, approved.actor) && b.equal(row.actor, context.observers[0]) &&
                        b.equal(row.operation, approved.operation) && b.equal(row.condition, approved.condition) &&
                        row.claimed == approved.consequence,
                    Code::Conflict, "action.consequence", i
                );
                b.unique(row.rewards, identity, "action.rewards");
                for (const auto& reward : row.rewards)
                    b.need(b.find(cat.rewards, reward, identity) != cat.rewards.size(), Code::UnknownReward, "action.rewards", i, reward);
                b.need(row.rewards.size() == approved.rewards.size(), Code::Conflict, "action.rewards", i);
                for (std::size_t reward = 0; reward < row.rewards.size(); ++reward)
                    b.need(b.equal(row.rewards[reward], approved.rewards[reward]), Code::Conflict, "action.rewards", i);
                b.unique(row.flavours, id, "flavour.id");
                const auto method = static_cast<std::size_t>(approved.method);
                b.need(!methods[method], Code::Conflict, "action.mechanical_duplicate", i);
                methods[method] = true;
                available += eligible(approved, context.state);
            }
            b.need(available >= 2, Code::Context, "context.available_actions");
            b.work(b.usage.text_bytes); // Owned copy byte work, before retained allocation.
            return AcceptedHandle(new const Accepted(set, cat, context, b.usage));
        } catch (const Stop&) {
            return std::unexpected(std::move(b.error));
        }
    }

    Projection::Projection(AcceptedHandle accepted, D::DefinitionHandle definition, std::size_t flavour, Usage usage)
    : accepted_(std::move(accepted)), definition_(std::move(definition)), flavour_(flavour), usage_(usage) {}

    std::expected<ProjectionHandle, Error> project(AcceptedHandle accepted, std::size_t flavour, const Limits& limits) {
        if (!accepted)
            return std::unexpected(Error{Code::EmptyHandle, "accepted", {}});
        if (!valid_limits(limits))
            return std::unexpected(Error{Code::InvalidLimits, "limits", {}});
        Budget b(limits);
        try {
            const auto& set = accepted->set();
            preflight(b, set, accepted->catalogue(), accepted->context());
            for (const auto& action : set.actions)
                b.need(flavour < action.flavours.size(), Code::Variant, "flavour");
            b.storage(sizeof(Projection) + sizeof(D::Draft) + sizeof(D::OperationKeys));
            b.slots(set.actions.size() + 3, sizeof(D::Node));
            b.slots(2 * set.actions.size(), sizeof(D::Choice));
            b.slots(set.actions.size(), sizeof(D::ConditionKey));
            b.slots(set.actions.size(), sizeof(D::EffectKey));
            b.generated(6 + set.id.size() + 9 + 1);
            b.generated(5);
            b.generated(5);
            b.generated(set.proposition.size());
            std::array<bool, 3> declines{};
            std::size_t action_index = 0;
            for (const auto& action : set.actions) {
                const auto outcome = b.find(accepted->catalogue().outcomes, action.outcome, id);
                declines[action_index++] = accepted->catalogue().outcomes[outcome].method == Method::Decline;
                const auto& f = action.flavours[flavour];
                for (const auto length :
                     {action.id.size(), f.wording.size(), 6 + action.id.size(), 9 + action.id.size(), 6 + action.id.size(),
                      6 + action.id.size(), f.response.size(), std::size_t{8}, std::size_t{8}, std::size_t{11}, 9 + action.id.size(),
                      6 + action.id.size(), 6 + action.id.size(), 9 + action.id.size(), 6 + action.id.size()})
                    b.generated(length);
            }
            for (const auto text : {"destination", "Destination reached.", "shore", "You remain on shore."})
                b.generated(std::string_view(text).size());
            b.work(b.usage.text_bytes);
            D::Draft draft{{"story" + std::string("-") + set.id + "-flavour-" + std::to_string(flavour)}, set.revision, {"offer"}, {}};
            D::OperationKeys operations;
            D::Node offer{{"offer"}, set.proposition, D::NodeKind::Prompt, {}};
            draft.nodes.reserve(set.actions.size() + 3);
            offer.choices.reserve(set.actions.size());
            operations.conditions.reserve(set.actions.size());
            operations.effects.reserve(set.actions.size());
            for (const auto& action : set.actions) {
                const auto& f = action.flavours[flavour];
                const auto response = "reply-" + action.id;
                const auto condition_key = "eligible-" + action.id, effect_key = "apply-" + action.id;
                offer.choices.push_back({{action.id}, f.wording, {response}, D::ConditionKey{condition_key}, D::EffectKey{effect_key}});
                operations.conditions.push_back({condition_key});
                operations.effects.push_back({effect_key});
            }
            draft.nodes.push_back(std::move(offer));
            action_index = 0;
            for (const auto& action : set.actions) {
                D::Node reply{{"reply-" + action.id}, action.flavours[flavour].response, D::NodeKind::Prompt, {}};
                reply.choices.reserve(1);
                reply.choices.push_back({{"continue"}, "Continue", {declines[action_index++] ? "shore" : "destination"}, {}, {}});
                draft.nodes.push_back(std::move(reply));
            }
            draft.nodes.push_back({{"destination"}, "Destination reached.", D::NodeKind::Terminal, {}});
            draft.nodes.push_back({{"shore"}, "You remain on shore.", D::NodeKind::Terminal, {}});
            auto compiled = D::compile(draft, operations);
            if (!compiled)
                return std::unexpected(Error{Code::Compile, "definition", {}, 0, std::move(compiled.error()), {}});
            return ProjectionHandle(new const Projection(std::move(accepted), std::move(*compiled), flavour, b.usage));
        } catch (const Stop&) {
            return std::unexpected(std::move(b.error));
        }
    }
}
