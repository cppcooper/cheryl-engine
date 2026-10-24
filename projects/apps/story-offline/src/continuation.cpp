#include "continuation.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace OfflineStory {
    namespace {
        struct Stop {};
        struct Budget {
            WatchLimits limits;
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
            void storage(std::size_t n) {
                need(n <= limits.storage_bytes - usage.storage_bytes, Code::StorageLimit, "storage");
                usage.storage_bytes += n;
            }
            void slots(std::size_t n, std::size_t size) {
                need(n <= limits.storage_bytes / size, Code::StorageLimit, "storage");
                storage(n * size);
                work(n);
            }
            void generated(std::size_t n) {
                need(n <= limits.text_length && n <= limits.text_bytes - usage.text_bytes, Code::TextLimit, "text");
                usage.text_bytes += n;
                storage(n + 1); // n <= 1024 before arithmetic.
                work(n + 1);
            }
            void text(std::string_view s, bool id, std::string_view field, std::size_t row = 0) {
                need(!s.empty() && (!id || s.size() <= 64), Code::Identity, field, {}, row);
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
        };
        bool valid_limits(const WatchLimits& limits) {
            for (const auto field :
                 {&WatchLimits::fragments, &WatchLimits::outcomes, &WatchLimits::recipes, &WatchLimits::candidates, &WatchLimits::nodes,
                  &WatchLimits::choices, &WatchLimits::text_length, &WatchLimits::text_bytes, &WatchLimits::storage_bytes,
                  &WatchLimits::work})
                if (limits.*field > maximum_watch_limits.*field)
                    return false;
            return true;
        }
        struct Approved {
            Method prior;
            std::string_view id, operation, condition;
            Knowledge learned;
            int attitude;
        };
        constexpr Approved approved[] = {
            {Method::Pay, "fare-confirmed", "verify_record", "has_record", Knowledge::VerifiedFare, 1},
            {Method::Repair, "repair-confirmed", "hear_testimony", "mara_available", Knowledge::HeardTestimony, 1},
            {Method::Decline, "shore-explained", "explain_shore", "still_ashore", Knowledge::SawDecline, 0}
        };
        const Approved* policy(Method prior) noexcept {
            for (const auto& row : approved)
                if (row.prior == prior)
                    return &row;
            return nullptr;
        }
        std::optional<Method> prior_method(const State& s) noexcept {
            if (!valid_state(s))
                return {};
            if (s.facts.paid)
                return Method::Pay;
            if (s.facts.repaired)
                return Method::Repair;
            if (s.facts.declined)
                return Method::Decline;
            return {};
        }
        bool knows(const State& s, const Approved& rule) noexcept {
            return s.knowledge[1] == rule.learned && !s.believes_unlicensed[1];
        }
        bool context_ok(const State& s, const Approved& rule) noexcept {
            if (prior_method(s) != rule.prior || s.knowledge[2] != Knowledge::None || s.believes_unlicensed[2] || s.attitude[2] != 0)
                return false;
            const auto witness = rule.prior == Method::Pay      ? Knowledge::SawFare
                                 : rule.prior == Method::Repair ? Knowledge::SawRepair
                                                                : Knowledge::SawDecline;
            if (s.knowledge[0] != witness || s.believes_unlicensed[0])
                return false;
            if (knows(s, rule))
                return s.attitude[1] >= 0;
            return rule.prior == Method::Decline
                       ? s.knowledge[1] == Knowledge::None
                       : s.knowledge[1] == Knowledge::HeardPassage && s.believes_unlicensed[1] && s.attitude[1] < 0;
        }
    }

    Continuation::Continuation(
        const FragmentLibrary& library,
        ProjectionHandle ferry,
        const Checkpoint& context,
        D::DefinitionHandle definition,
        std::size_t recipe,
        std::size_t outcome,
        std::size_t flavour,
        Usage usage
    )
    : library_(library),
      ferry_(std::move(ferry)),
      context_(context),
      definition_(std::move(definition)),
      recipe_(recipe),
      outcome_(outcome),
      flavour_(flavour),
      usage_(usage) {}

    std::expected<ContinuationHandle, Error> compose_watch(
        const FragmentLibrary& library,
        ProjectionHandle ferry,
        const Checkpoint& context,
        std::size_t flavour,
        const WatchLimits& limits
    ) {
        if (!ferry)
            return std::unexpected(Error{Code::EmptyHandle, "ferry", {}});
        if (!valid_limits(limits))
            return std::unexpected(Error{Code::InvalidLimits, "limits", {}});
        Budget b{limits};
        try {
            b.need(
                library.fragments.size() <= limits.fragments && library.outcomes.size() <= limits.outcomes &&
                    library.recipes.size() <= limits.recipes && library.recipes.size() <= limits.candidates,
                Code::CountLimit, "library.rows"
            );
            b.need(limits.nodes >= 5 && limits.choices >= 4, Code::CountLimit, "graph");
            b.need(library.origin == Origin::Manual && library.revision != 0, Code::Identity, "library");
            b.need(flavour < 2, Code::Variant, "flavour");
            b.storage(sizeof(Continuation) + sizeof(D::Draft) + sizeof(D::OperationKeys));
            b.slots(library.fragments.size(), sizeof(Fragment));
            b.slots(library.outcomes.size(), sizeof(WatchOutcome));
            b.slots(library.recipes.size(), sizeof(WatchRecipe));
            b.slots(5, sizeof(D::Node));
            b.slots(4, sizeof(D::Choice));
            b.slots(2, sizeof(D::ConditionKey));
            b.slots(1, sizeof(D::EffectKey));
            b.text(library.id, true, "library.id");
            for (std::size_t i = 0; i < library.fragments.size(); ++i) {
                const auto& f = library.fragments[i];
                b.text(f.id, true, "fragment.id", i);
                b.need(
                    f.kind == FragmentKind::Greeting || f.kind == FragmentKind::Wording || f.kind == FragmentKind::Reply, Code::Conflict,
                    "fragment.kind", f.id, i
                );
                for (const auto& text : f.variants)
                    b.text(text, false, "fragment.text", i);
            }
            for (std::size_t i = 0; i < library.outcomes.size(); ++i) {
                const auto& row = library.outcomes[i];
                for (const auto* key : {&row.id, &row.operation, &row.condition})
                    b.text(*key, true, "outcome", i);
            }
            for (std::size_t i = 0; i < library.recipes.size(); ++i) {
                const auto& row = library.recipes[i];
                for (const auto* key : {&row.id, &row.actor, &row.action, &row.outcome, &row.location, &row.wording, &row.reply})
                    b.text(*key, true, "recipe", i);
            }
            for (const auto* key :
                 {&library.doubt, &library.known, &library.ashore, &library.shore_known, &library.withhold_word, &library.withhold_reply,
                  &library.ask_word, &library.known_reply, &library.finish_word, &library.finish_reply})
                b.text(*key, true, "library.reference");
            for (const auto* key : {&context.dialogue.definition.value, &context.dialogue.session.value, &context.dialogue.node.value}) {
                b.need(!key->empty() && key->size() <= 128, Code::Context, "context.identity");
                b.generated(key->size()); // Opaque existing core identity bytes; no coercion.
            }
            b.unique(library.fragments, "fragment.id");
            b.unique(library.outcomes, "outcome.id");
            b.unique(library.recipes, "recipe.id");
            const auto piece = [&](const std::string& key, FragmentKind kind) -> const Fragment& {
                const auto i = b.find(library.fragments, key);
                b.need(i != library.fragments.size(), Code::MissingReference, "fragment.reference", key);
                b.need(library.fragments[i].kind == kind, Code::Conflict, "fragment.role", key);
                return library.fragments[i];
            };
            for (const auto* key : {&library.doubt, &library.known, &library.ashore, &library.shore_known})
                static_cast<void>(piece(*key, FragmentKind::Greeting));
            for (const auto* key : {&library.withhold_word, &library.ask_word, &library.finish_word})
                static_cast<void>(piece(*key, FragmentKind::Wording));
            for (const auto* key : {&library.withhold_reply, &library.known_reply, &library.finish_reply})
                static_cast<void>(piece(*key, FragmentKind::Reply));
            for (std::size_t i = 0; i < library.outcomes.size(); ++i) {
                const auto& row = library.outcomes[i];
                const auto* rule = policy(row.prior);
                b.work(16);
                b.need(rule && b.equal(row.operation, rule->operation), Code::UnknownOperation, "outcome.operation", row.operation, i);
                b.need(b.equal(row.condition, rule->condition), Code::UnknownCondition, "outcome.condition", row.condition, i);
                b.need(
                    b.equal(row.id, rule->id) && row.learned == rule->learned && row.attitude == rule->attitude, Code::Conflict,
                    "outcome.consequence", row.id, i
                );
            }
            b.work(32);
            const auto prior = prior_method(context.state);
            b.need(prior.has_value(), Code::Context, "context.facts");
            const auto* rule = policy(*prior);
            b.need(context_ok(context.state, *rule), Code::Context, "context.observers");
            b.need(
                ferry->accepted().context().observers == std::array<std::string, 3>{"mara", "ivo", "lena"}, Code::Context, "context.roles"
            );
            b.work(3 * 129 * 6); // Bound the core factory's small source-node lookup/identity work.
            auto validated = D::Session::resume(ferry->definition(), context.dialogue);
            if (!validated)
                return std::unexpected(Error{Code::Resume, "context.dialogue", {}, 0, {}, validated.error()});
            const auto* node = ferry->definition()->find_node(context.dialogue.node);
            b.need(
                node && node->kind == D::NodeKind::Terminal && b.equal(node->id.value, *prior == Method::Decline ? "shore" : "destination"),
                Code::Context, "context.completed"
            );
            std::size_t selected = library.recipes.size();
            for (std::size_t i = 0; i < library.recipes.size(); ++i) {
                const auto& row = library.recipes[i];
                b.work(16);
                b.need(policy(row.prior), Code::Conflict, "recipe.prior", row.id, i);
                for (std::size_t j = 0; j < i; ++j) {
                    b.work(1);
                    b.need(row.prior != library.recipes[j].prior, Code::Duplicate, "recipe.prior", row.id, i);
                }
                b.need(b.equal(row.actor, "ivo"), Code::UnknownActor, "recipe.actor", row.actor, i);
                b.need(!b.equal(row.action, "withhold"), Code::Duplicate, "recipe.action", row.action, i);
                const auto o = b.find(library.outcomes, row.outcome);
                b.need(o != library.outcomes.size(), Code::MissingReference, "recipe.outcome", row.outcome, i);
                b.need(library.outcomes[o].prior == row.prior, Code::Conflict, "recipe.consequence", row.outcome, i);
                static_cast<void>(piece(row.location, FragmentKind::Greeting));
                static_cast<void>(piece(row.wording, FragmentKind::Wording));
                static_cast<void>(piece(row.reply, FragmentKind::Reply));
                if (row.prior == *prior)
                    selected = i;
            }
            b.need(selected != library.recipes.size(), Code::MissingReference, "recipe.context");
            const auto& recipe = library.recipes[selected];
            const auto outcome = b.find(library.outcomes, recipe.outcome);
            const bool known = knows(context.state, *rule);
            const auto& location = piece(recipe.location, FragmentKind::Greeting).variants[flavour];
            const auto& greeting = piece(
                                       known                       ? (*prior == Method::Decline ? library.shore_known : library.known)
                                       : *prior == Method::Decline ? library.ashore
                                                                   : library.doubt,
                                       FragmentKind::Greeting
            )
                                       .variants[flavour];
            const auto& wording = piece(recipe.wording, FragmentKind::Wording).variants[flavour];
            const auto& reply = piece(recipe.reply, FragmentKind::Reply).variants[flavour];
            const auto& withhold = piece(library.withhold_word, FragmentKind::Wording).variants[flavour];
            const auto& reserved = piece(library.withhold_reply, FragmentKind::Reply).variants[flavour];
            const auto& ask = piece(library.ask_word, FragmentKind::Wording).variants[flavour];
            const auto& remembered = piece(library.known_reply, FragmentKind::Reply).variants[flavour];
            const auto& finish = piece(library.finish_word, FragmentKind::Wording).variants[flavour];
            const auto& done = piece(library.finish_reply, FragmentKind::Reply).variants[flavour];
            b.generated(6 + library.id.size() + 1 + ferry->definition()->id().value.size() + 1 + recipe.id.size() + 4 + 1 + 1);
            b.generated(location.size() + 1 + greeting.size());
            for (const auto text : std::initializer_list<std::string_view>{
                     "question",
                     "question",
                     "understood",
                     std::string_view(recipe.action),
                     std::string_view(wording),
                     "understood",
                     std::string_view(rule->condition),
                     std::string_view(rule->operation),
                     "withhold",
                     std::string_view(withhold),
                     "reserved",
                     "understood",
                     std::string_view(reply),
                     "ask-again",
                     std::string_view(ask),
                     "remembered",
                     "is-informed",
                     "finish",
                     std::string_view(finish),
                     "done",
                     "reserved",
                     std::string_view(reserved),
                     "remembered",
                     std::string_view(remembered),
                     "done",
                     std::string_view(done),
                     std::string_view(rule->condition),
                     "is-informed",
                     std::string_view(rule->operation)
                 })
                b.generated(text.size());
            b.work(b.usage.text_bytes); // Retention and generated payload copy allowance, before construction.
            D::Draft draft{
                {"watch-" + library.id + "-" + ferry->definition()->id().value + "-" + recipe.id + "-f" + std::to_string(flavour) +
                 (known ? "-k" : "-u")},
                library.revision,
                {"question"},
                {}
            };
            draft.nodes.reserve(5);
            D::Node question{{"question"}, location + "\n" + greeting, D::NodeKind::Prompt, {}};
            question.choices.reserve(2);
            question.choices.push_back(
                {{recipe.action},
                 wording,
                 {"understood"},
                 D::ConditionKey{std::string(rule->condition)},
                 D::EffectKey{std::string(rule->operation)}}
            );
            question.choices.push_back({{"withhold"}, withhold, {"reserved"}, {}, {}});
            D::Node understood{{"understood"}, reply, D::NodeKind::Prompt, {}};
            understood.choices.reserve(2);
            understood.choices.push_back({{"ask-again"}, ask, {"remembered"}, D::ConditionKey{"is-informed"}, {}});
            understood.choices.push_back({{"finish"}, finish, {"done"}, {}, {}});
            draft.nodes.push_back(std::move(question));
            draft.nodes.push_back(std::move(understood));
            draft.nodes.push_back({{"reserved"}, reserved, D::NodeKind::Terminal, {}});
            draft.nodes.push_back({{"remembered"}, remembered, D::NodeKind::Terminal, {}});
            draft.nodes.push_back({{"done"}, done, D::NodeKind::Terminal, {}});
            const D::OperationKeys operations{{{std::string(rule->condition)}, {"is-informed"}}, {{std::string(rule->operation)}}};
            auto compiled = D::compile(draft, operations);
            if (!compiled)
                return std::unexpected(Error{Code::Compile, "continuation", {}, 0, std::move(compiled.error()), {}});
            return ContinuationHandle(
                new const Continuation(library, std::move(ferry), context, std::move(*compiled), selected, outcome, flavour, b.usage)
            );
        } catch (const Stop&) {
            return std::unexpected(std::move(b.error));
        }
    }

    WatchEncounter::WatchEncounter(ContinuationHandle content, D::Session session)
    : content_(std::move(content)), session_(std::move(session)), state_{content_->context().state, true} {}
    std::expected<WatchEncounter, Error> WatchEncounter::start(ContinuationHandle content, D::SessionId id) {
        if (!content)
            return std::unexpected(Error{Code::EmptyHandle, "continuation", {}});
        auto started = D::Session::start(content->definition(), std::move(id));
        if (!started)
            return std::unexpected(Error{Code::Resume, "session", {}, 0, {}, started.error()});
        return WatchEncounter(std::move(content), std::move(*started));
    }
    bool WatchEncounter::coherent(const WatchState& s) const noexcept {
        if (!content_ || !context_ok(s.world, *policy(content_->recipe().prior)) || s.world.facts != content_->context().state.facts)
            return false;
        const auto& base = content_->context().state;
        for (const auto i : {0, 2})
            if (s.world.knowledge[i] != base.knowledge[i] || s.world.believes_unlicensed[i] != base.believes_unlicensed[i] ||
                s.world.attitude[i] != base.attitude[i])
                return false;
        return true;
    }
    bool WatchEncounter::informed() const noexcept {
        return content_ && knows(state_.world, *policy(content_->recipe().prior));
    }
    bool WatchEncounter::test(const D::ConditionKey& key) const {
        if (key.value == "is-informed")
            return coherent(state_) && informed();
        if (key.value != content_->outcome().condition)
            throw std::logic_error("Unknown continuation condition");
        return coherent(state_) && state_.evidence_available;
    }
    std::expected<D::View, D::SessionError> WatchEncounter::view() {
        return session_.view([this](const auto& key) { return test(key); });
    }
    std::expected<D::Transition, D::SessionError> WatchEncounter::choose(const D::Selection& selection) {
        static_assert(std::is_nothrow_copy_assignable_v<WatchState>);
        return session_.choose_with_transition(
            selection, [this](const auto& key) { return test(key); },
            [this](const auto& key) {
                if (key.value != content_->outcome().operation || !coherent(state_) || !state_.evidence_available)
                    throw std::logic_error("Invalid continuation effect");
                auto prepared = state_;
                prepared.world.knowledge[1] = content_->outcome().learned;
                prepared.world.believes_unlicensed[1] = false;
                prepared.world.attitude[1] = content_->outcome().attitude;
                if (!coherent(prepared))
                    throw std::logic_error("Invalid staged continuation");
                state_ = prepared;
                if (effects_ != std::numeric_limits<std::uint64_t>::max())
                    ++effects_;
            }
        );
    }
    std::expected<WatchCheckpoint, D::SessionError> WatchEncounter::capture() {
        auto saved = session_.snapshot();
        if (!saved)
            return std::unexpected(saved.error());
        return WatchCheckpoint{std::move(*saved), state_};
    }
    std::expected<void, Error> WatchEncounter::restore(const WatchCheckpoint& saved) {
        if (!content_)
            return std::unexpected(Error{Code::EmptyHandle, "continuation", {}});
        if (!coherent(saved.state))
            return std::unexpected(Error{Code::Context, "checkpoint.state", {}});
        auto loaded = D::Session::resume(content_->definition(), saved.dialogue);
        if (!loaded)
            return std::unexpected(Error{Code::Resume, "checkpoint.dialogue", {}, 0, {}, loaded.error()});
        static_assert(std::is_nothrow_move_assignable_v<D::Session>);
        std::expected<void, Error> success;
        session_ = std::move(*loaded);
        state_ = saved.state;
        return success;
    }
    bool WatchEncounter::evidence(bool available) noexcept {
        if (!content_)
            return false;
        state_.evidence_available = available;
        return true;
    }
}
