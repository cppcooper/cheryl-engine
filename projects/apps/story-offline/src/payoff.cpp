#include "payoff.h"

#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace OfflineStory {
    namespace {
        constexpr std::string_view names[] = {"pending", "help", "paid", "refused", "delivered", "broken", "ack", "left"};
        bool pending(ReliefResolution r) noexcept {
            return r == ReliefResolution::Promised || r == ReliefResolution::Negotiated;
        }
        RecipientKnowledge initial_knowledge(ReliefResolution r) noexcept {
            if (r == ReliefResolution::Helped)
                return RecipientKnowledge::SawHelp;
            if (r == ReliefResolution::Paid)
                return RecipientKnowledge::SawPayment;
            if (pending(r))
                return RecipientKnowledge::HeardPromise;
            return RecipientKnowledge::HeardRefusal;
        }
        struct Stop {};
        struct Budget {
            PayoffLimits limits;
            PayoffUsage usage;
            Error error{Code::Conflict, {}, {}};
            void need(bool yes, Code code, std::string_view field, std::string_view value = {}, std::size_t row = 0) {
                if (!yes) {
                    error = {code, std::string(field), std::string(value.substr(0, 64)), row};
                    throw Stop{};
                }
            }
            void work(std::size_t n) {
                need(n <= limits.work - usage.owned.work, Code::WorkLimit, "payoff.work");
                usage.owned.work += n;
            }
            void bytes(std::size_t n) {
                need(n <= limits.storage_bytes - usage.owned.storage_bytes, Code::StorageLimit, "payoff.storage");
                usage.owned.storage_bytes += n;
            }
            void slots(std::size_t n, std::size_t size) {
                need(n <= limits.storage_bytes / size, Code::StorageLimit, "payoff.product");
                bytes(n * size);
                work(n);
            }
            void text(std::string_view s, bool id = false) {
                need(!s.empty() && s.size() <= limits.text_length && (!id || s.size() <= 64), Code::TextLimit, "payoff.text");
                need(s.size() <= limits.text_bytes - usage.owned.text_bytes, Code::TextLimit, "payoff.text");
                usage.owned.text_bytes += s.size();
                bytes(s.size() + 1);
                work(s.size() + 1);
                for (unsigned char c : s)
                    need(
                        id ? (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' : c >= 32 || c == '\n' || c == '\t',
                        Code::TextLimit, "payoff.text"
                    );
            }
            bool equal(std::string_view a, std::string_view b) {
                work(a.size() + 1);
                return a == b;
            }
            void retain(std::size_t n) {
                need(n <= limits.retained_bytes - usage.retained_bytes, Code::StorageLimit, "payoff.retained");
                usage.retained_bytes += n;
            }
        };
        std::size_t decimal(std::uint64_t n) noexcept {
            std::size_t size = 1;
            while (n >= 10) {
                n /= 10;
                ++size;
            }
            return size;
        }
        PayoffState initial(const PayoffContent& c) {
            const auto& source = c.source().state;
            PayoffState s;
            s.world = source.world;
            s.obligation = pending(source.resolution) ? Obligation::Pending : Obligation::None;
            s.due = source.planks_due;
            s.recipient_knows = initial_knowledge(source.resolution);
            s.recipient_attitude = source.requester_attitude;
            return s;
        }
    }
    PayoffContent::PayoffContent(
        BargainHandle b,
        const BargainCheckpoint& s,
        const HandoffEvent& e,
        const PayoffLibrary& l,
        D::DefinitionHandle d,
        PayoffUsage u
    )
    : bargain_(std::move(b)), source_(s), event_(e), library_(l), definition_(std::move(d)), usage_(u) {}

    std::expected<PayoffHandle, Error> compose_payoff(
        BargainHandle source,
        const BargainCheckpoint& completed,
        const HandoffEvent& event,
        const PayoffLibrary& library,
        const PayoffLimits& limits
    ) {
        if (!source)
            return std::unexpected(Error{Code::EmptyHandle, "payoff.bargain", {}});
        for (auto f :
             {&PayoffLimits::ingredients, &PayoffLimits::candidates, &PayoffLimits::nodes, &PayoffLimits::choices,
              &PayoffLimits::text_length, &PayoffLimits::text_bytes, &PayoffLimits::storage_bytes, &PayoffLimits::retained_bytes,
              &PayoffLimits::work})
            if (limits.*f > maximum_payoff_limits.*f)
                return std::unexpected(Error{Code::InvalidLimits, "payoff.limits", {}});
        Budget b{limits};
        try {
            const bool promise = pending(completed.state.resolution);
            b.need(
                library.fragments.size() == 8 && library.fragments.size() <= limits.ingredients && limits.candidates >= 4, Code::CountLimit,
                "payoff.rows"
            );
            b.need(limits.nodes >= (promise ? 4u : 3u) && limits.choices >= (promise ? 3u : 2u), Code::CountLimit, "payoff.graph");
            b.need(library.origin == Origin::Manual && library.revision > 0, Code::Identity, "payoff.origin/revision");
            b.retain(source->usage().storage_bytes);
            const auto& watch = *source->context().previous;
            b.retain(watch.usage().storage_bytes);
            b.retain(watch.ferry()->usage().storage_bytes);
            b.retain(watch.ferry()->accepted().usage().storage_bytes);
            b.bytes(sizeof(PayoffContent) + sizeof(D::Draft) + sizeof(D::OperationKeys));
            b.slots(8, sizeof(PayoffFragment));
            b.slots(promise ? 4 : 3, sizeof(D::Node));
            b.slots(promise ? 3 : 2, sizeof(D::Choice));
            b.slots(4, sizeof(D::ConditionKey));
            b.slots(4, sizeof(D::EffectKey));
            b.text(library.id, true);
            b.text(event.recipient, true);
            std::array<std::size_t, 8> ord;
            ord.fill(8);
            for (std::size_t i = 0; i < 8; ++i) {
                const auto& f = library.fragments[i];
                b.text(f.id, true);
                std::size_t at = 8;
                for (std::size_t j = 0; j < 8; ++j)
                    if (b.equal(f.id, names[j]))
                        at = j;
                b.need(at != 8, Code::MissingReference, "payoff.fragment", f.id, i);
                b.need(ord[at] == 8, Code::Duplicate, "payoff.fragment", f.id, i);
                ord[at] = i;
                for (const auto& text : f.text)
                    b.text(text);
            }
            for (auto i : ord)
                b.need(i != 8, Code::MissingReference, "payoff.fragment");
            for (const auto* s : {&completed.dialogue.definition.value, &completed.dialogue.session.value, &completed.dialogue.node.value})
                b.text(*s);
            // Existing bounded source restore has no handlers. Reserve key comparison work before entering its allocator boundary.
            b.work(3 * 2049 * 8 + 128);
            b.bytes(2 * sizeof(D::Session) + 3 * 2049); // Conservative temporary source-validation session/identity reservation.
            auto validator = BargainEncounter::start(source, {"payoff-validation"}).value();
            auto checked = validator.restore(completed);
            if (!checked)
                return std::unexpected(std::move(checked.error()));
            const auto* terminal = source->definition()->find_node(completed.dialogue.node);
            b.need(
                terminal && terminal->kind == D::NodeKind::Terminal && completed.state.resolution != ReliefResolution::Open, Code::Context,
                "payoff.completed"
            );
            b.need(
                event.location == source->request().location && b.equal(event.recipient, source->request().actor) &&
                    (!event.suspected_breach || promise),
                Code::Context, "payoff.handoff"
            );
            const auto flavour = source->pick().flavour;
            const auto fragment = [&](std::size_t i) -> const std::string& { return library.fragments[ord[i]].text[flavour]; };
            // Four finite context alternatives; every comparison is charged, including rejected rows.
            const ReliefResolution variants[] = {
                ReliefResolution::Helped, ReliefResolution::Paid, ReliefResolution::Refused, ReliefResolution::Promised
            };
            std::size_t greeting = 0;
            for (std::size_t i = 0; i < 4; ++i) {
                b.work(4);
                if (completed.state.resolution == variants[i] || (i == 3 && promise))
                    greeting = i == 0 ? 1 : i == 1 ? 2 : i == 2 ? 3 : 0;
            }
            const ReliefText* proposition = nullptr;
            for (const auto& t : source->sets().texts)
                if (b.equal(t.id, source->request().proposition))
                    proposition = &t;
            b.need(proposition, Code::MissingReference, "payoff.proposition");
            // Preflight every generated text/copy before constructing the Draft.
            const std::string_view prefix = event.location == ReliefLocation::Bank ? "Original bank handoff. " : "Harbor quay handoff. ";
            const auto opening_size = prefix.size() + proposition->flavours[flavour].size() + 1 + fragment(greeting).size() + 1 +
                                      std::string_view("Recorded due: ").size() + decimal(completed.state.planks_due) +
                                      std::string_view(" planks.").size();
            const auto generated = [&](std::size_t n) {
                b.need(n <= limits.text_length && n <= limits.text_bytes - b.usage.owned.text_bytes, Code::TextLimit, "payoff.generated");
                b.usage.owned.text_bytes += n;
                b.bytes(n + 1);
                b.work(n + 1);
            };
            generated(opening_size);
            const auto id_size = 7 + library.id.size() + 1 + source->definition()->id().value.size() + 2 +
                                 decimal(source->definition()->revision()) + 2 + decimal(library.revision) + 2 +
                                 decimal(static_cast<unsigned>(completed.state.resolution)) + (event.suspected_breach ? 8 : 6);
            generated(id_size);
            constexpr std::string_view pending_fields[] = {
                "question",
                "question",
                "fulfilled",
                "broken",
                "left",
                "deliver",
                "Deliver the recorded material",
                "fulfilled",
                "can_deliver",
                "deliver_material",
                "break",
                "Declare the promise broken",
                "broken",
                "can_break",
                "break_promise",
                "leave",
                "Leave without settling",
                "left",
                "can_leave",
                "leave_pending"
            };
            constexpr std::string_view ack_fields[] = {
                "question",
                "question",
                "acknowledged",
                "left",
                "acknowledge",
                "Hear the steward's response",
                "acknowledged",
                "can_ack",
                "acknowledge_prior",
                "leave",
                "Leave without settling",
                "left",
                "can_leave",
                "leave_pending"
            };
            for (auto field : promise ? std::span<const std::string_view>(pending_fields) : std::span<const std::string_view>(ack_fields))
                generated(field.size());
            for (auto key :
                 {"can_deliver", "can_break", "can_ack", "can_leave", "deliver_material", "break_promise", "acknowledge_prior",
                  "leave_pending"})
                generated(std::string_view(key).size());
            if (promise) {
                generated(fragment(4).size());
                generated(fragment(5).size());
            } else
                generated(fragment(greeting).size() + 1 + fragment(6).size());
            generated(fragment(7).size());
            b.work(b.usage.owned.text_bytes); // Owned library/source/Draft copying.
            D::Draft draft{
                {"payoff-" + library.id + "-" + source->definition()->id().value + "-s" + std::to_string(source->definition()->revision()) +
                 "-v" + std::to_string(library.revision) + "-r" + std::to_string(static_cast<unsigned>(completed.state.resolution)) +
                 (event.suspected_breach ? "-suspect" : "-clear")},
                library.revision,
                {"question"},
                {}
            };
            D::Node opening{
                {"question"},
                std::string(prefix) + proposition->flavours[flavour] + " " + fragment(greeting) +
                    " Recorded due: " + std::to_string(completed.state.planks_due) + " planks.",
                D::NodeKind::Prompt,
                {}
            };
            if (promise) {
                opening.choices = {
                    {{"deliver"},
                     "Deliver the recorded material",
                     {"fulfilled"},
                     D::ConditionKey{"can_deliver"},
                     D::EffectKey{"deliver_material"}},
                    {{"break"}, "Declare the promise broken", {"broken"}, D::ConditionKey{"can_break"}, D::EffectKey{"break_promise"}}
                };
            } else
                opening.choices = {
                    {{"acknowledge"},
                     "Hear the steward's response",
                     {"acknowledged"},
                     D::ConditionKey{"can_ack"},
                     D::EffectKey{"acknowledge_prior"}}
                };
            opening.choices.push_back(
                {{"leave"}, "Leave without settling", {"left"}, D::ConditionKey{"can_leave"}, D::EffectKey{"leave_pending"}}
            );
            draft.nodes.reserve(promise ? 4 : 3);
            draft.nodes.push_back(std::move(opening));
            if (promise) {
                draft.nodes.push_back({{"fulfilled"}, fragment(4), D::NodeKind::Terminal, {}});
                draft.nodes.push_back({{"broken"}, fragment(5), D::NodeKind::Terminal, {}});
            } else
                draft.nodes.push_back({{"acknowledged"}, fragment(greeting) + " " + fragment(6), D::NodeKind::Terminal, {}});
            draft.nodes.push_back({{"left"}, fragment(7), D::NodeKind::Terminal, {}});
            D::OperationKeys keys{
                {{"can_deliver"}, {"can_break"}, {"can_ack"}, {"can_leave"}},
                {{"deliver_material"}, {"break_promise"}, {"acknowledge_prior"}, {"leave_pending"}}
            };
            auto compiled = D::compile(draft, keys);
            if (!compiled)
                return std::unexpected(Error{Code::Compile, "payoff", {}, 0, std::move(compiled.error()), {}});
            return PayoffHandle(new const PayoffContent(std::move(source), completed, event, library, std::move(*compiled), b.usage));
        } catch (const Stop&) {
            return std::unexpected(std::move(b.error));
        }
    }
    PayoffEncounter::PayoffEncounter(PayoffHandle c, D::Session s)
    : content_(std::move(c)), session_(std::move(s)), state_(initial(*content_)) {}
    std::expected<PayoffEncounter, Error> PayoffEncounter::start(PayoffHandle c, D::SessionId id) {
        if (!c)
            return std::unexpected(Error{Code::EmptyHandle, "payoff", {}});
        auto s = D::Session::start(c->definition(), std::move(id));
        if (!s)
            return std::unexpected(Error{Code::Resume, "payoff.session", {}, 0, {}, s.error()});
        return PayoffEncounter(std::move(c), std::move(*s));
    }
    bool PayoffEncounter::coherent(const PayoffState& s) const noexcept {
        if (!content_ || !valid_state(s.world) || (s.at_handoff && !s.encountered))
            return false;
        const auto& base = content_->source().state;
        auto facts = s.world.facts;
        facts.planks = base.world.facts.planks;
        if (facts != base.world.facts || s.world.knowledge != base.world.knowledge || s.world.attitude != base.world.attitude ||
            s.world.believes_unlicensed != base.world.believes_unlicensed)
            return false;
        const bool promise = pending(base.resolution);
        auto obligation = promise ? Obligation::Pending : Obligation::None;
        int due = base.planks_due, delivered = 0, attitude = base.requester_attitude;
        auto knowledge = initial_knowledge(base.resolution);
        bool favor = false, belief = promise && s.encountered && content_->event().suspected_breach;
        switch (s.outcome) {
            case PayoffOutcome::Open:
                if (s.settlement_planks != -1)
                    return false;
                break;
            case PayoffOutcome::Left:
                break;
            case PayoffOutcome::Acknowledged:
                if (promise || !s.at_handoff)
                    return false;
                break;
            case PayoffOutcome::Fulfilled:
                if (!promise || !s.at_handoff || s.settlement_planks < base.planks_due)
                    return false;
                obligation = Obligation::Fulfilled;
                due = 0;
                delivered = base.planks_due;
                favor = true;
                belief = false;
                attitude = 4;
                knowledge = RecipientKnowledge::CheckedDelivery;
                break;
            case PayoffOutcome::Broken:
                if (!promise || !s.at_handoff)
                    return false;
                obligation = Obligation::Broken;
                due = 0;
                belief = true;
                attitude = -3;
                knowledge = RecipientKnowledge::HeardBreach;
                break;
            default:
                return false;
        }
        if (s.outcome != PayoffOutcome::Open &&
            (s.settlement_planks < 0 || s.settlement_planks > 100 || s.world.facts.planks != s.settlement_planks - delivered))
            return false;
        return s.obligation == obligation && s.due == due && s.delivered == delivered && s.favor == favor &&
               s.recipient_knows == knowledge && s.recipient_attitude == attitude && s.believes_broken == belief;
    }
    bool PayoffEncounter::test(const D::ConditionKey& k) const {
        if (!coherent(state_) || state_.outcome != PayoffOutcome::Open)
            return false;
        if (k.value == "can_leave")
            return true;
        if (k.value == "can_deliver")
            return state_.at_handoff && state_.obligation == Obligation::Pending && state_.world.facts.planks >= state_.due;
        if (k.value == "can_break")
            return state_.at_handoff && state_.obligation == Obligation::Pending;
        if (k.value == "can_ack")
            return state_.at_handoff && state_.obligation == Obligation::None;
        throw std::logic_error("Unknown payoff condition");
    }
    std::expected<D::View, D::SessionError> PayoffEncounter::view() {
        return session_.view([this](const auto& k) { return test(k); });
    }
    std::expected<D::Transition, D::SessionError> PayoffEncounter::choose(const D::Selection& selected) {
        static_assert(std::is_nothrow_copy_assignable_v<PayoffState>);
        return session_.choose_with_transition(
            selected, [this](const auto& k) { return test(k); },
            [this](const auto& key) {
                const std::string_view condition = key.value == "deliver_material"    ? "can_deliver"
                                                   : key.value == "break_promise"     ? "can_break"
                                                   : key.value == "acknowledge_prior" ? "can_ack"
                                                   : key.value == "leave_pending"     ? "can_leave"
                                                                                      : "";
                // Keys are borrowed from the retained definition. Do not copy an owned string inside/post effect.
                bool eligible = coherent(state_) && state_.outcome == PayoffOutcome::Open;
                if (condition == "can_deliver")
                    eligible = eligible && state_.at_handoff && state_.obligation == Obligation::Pending &&
                               state_.world.facts.planks >= state_.due;
                else if (condition == "can_break")
                    eligible = eligible && state_.at_handoff && state_.obligation == Obligation::Pending;
                else if (condition == "can_ack")
                    eligible = eligible && state_.at_handoff && state_.obligation == Obligation::None;
                else if (condition != "can_leave")
                    eligible = false;
                if (!eligible)
                    throw std::logic_error("Invalid payoff effect");
                auto prepared = state_;
                prepared.settlement_planks = state_.world.facts.planks;
                if (condition == "can_deliver") {
                    prepared.world.facts.planks -= prepared.due;
                    prepared.delivered = prepared.due;
                    prepared.due = 0;
                    prepared.obligation = Obligation::Fulfilled;
                    prepared.favor = true;
                    prepared.outcome = PayoffOutcome::Fulfilled;
                    prepared.recipient_knows = RecipientKnowledge::CheckedDelivery;
                    prepared.recipient_attitude = 4;
                    prepared.believes_broken = false;
                } else if (condition == "can_break") {
                    prepared.due = 0;
                    prepared.obligation = Obligation::Broken;
                    prepared.outcome = PayoffOutcome::Broken;
                    prepared.recipient_knows = RecipientKnowledge::HeardBreach;
                    prepared.recipient_attitude = -3;
                    prepared.believes_broken = true;
                } else
                    prepared.outcome = condition == "can_ack" ? PayoffOutcome::Acknowledged : PayoffOutcome::Left;
                if (!coherent(prepared))
                    throw std::logic_error("Invalid staged payoff");
                state_ = prepared;
                if (effects_ != std::numeric_limits<std::uint64_t>::max())
                    ++effects_;
            }
        );
    }
    std::expected<PayoffCheckpoint, D::SessionError> PayoffEncounter::capture() {
        auto s = session_.snapshot();
        if (!s)
            return std::unexpected(s.error());
        return PayoffCheckpoint{std::move(*s), state_};
    }
    std::expected<void, Error> PayoffEncounter::restore(const PayoffCheckpoint& saved) {
        if (!content_)
            return std::unexpected(Error{Code::EmptyHandle, "payoff", {}});
        if (!coherent(saved.state))
            return std::unexpected(Error{Code::Context, "payoff.state", {}});
        const auto node = saved.state.outcome == PayoffOutcome::Open        ? "question"
                          : saved.state.outcome == PayoffOutcome::Fulfilled ? "fulfilled"
                          : saved.state.outcome == PayoffOutcome::Broken    ? "broken"
                          : saved.state.outcome == PayoffOutcome::Left      ? "left"
                                                                            : "acknowledged";
        if (saved.dialogue.node.value != node)
            return std::unexpected(Error{Code::Context, "payoff.cursor", {}});
        auto fresh = D::Session::resume(content_->definition(), saved.dialogue);
        if (!fresh)
            return std::unexpected(Error{Code::Resume, "payoff.dialogue", {}, 0, {}, fresh.error()});
        static_assert(std::is_nothrow_move_assignable_v<D::Session>);
        std::expected<void, Error> success;
        session_ = std::move(*fresh);
        state_ = saved.state;
        return success;
    }
    std::expected<void, Error> PayoffEncounter::handoff(bool present) {
        if (!content_)
            return std::unexpected(Error{Code::EmptyHandle, "payoff", {}});
        auto idle = session_.snapshot();
        if (!idle)
            return std::unexpected(Error{Code::Resume, "payoff.idle", {}, 0, {}, idle.error()});
        if (state_.outcome != PayoffOutcome::Open)
            return std::unexpected(Error{Code::Context, "payoff.closed", {}});
        auto prepared = state_;
        prepared.at_handoff = present;
        prepared.encountered = prepared.encountered || present;
        prepared.believes_broken =
            pending(content_->source().state.resolution) && prepared.encountered && content_->event().suspected_breach;
        if (!coherent(prepared))
            return std::unexpected(Error{Code::Context, "payoff.event", {}});
        std::expected<void, Error> success;
        state_ = prepared;
        return success;
    }
    bool PayoffEncounter::resources(int planks) noexcept {
        if (!content_ || state_.outcome != PayoffOutcome::Open)
            return false;
        auto prepared = state_;
        prepared.world.facts.planks = planks;
        if (!coherent(prepared))
            return false;
        state_ = prepared;
        return true;
    }
    PayoffLibrary manual_payoff_library() {
        return {
            Origin::Manual,
            "relief-handoff",
            1,
            {{"pending",
              {"The steward remembers your pending promise. Delivery is still your choice.",
               "Your word is recorded, but no delivery or favor has happened yet."}},
             {"help",
              {"The steward saw the boards you supplied earlier. They are already in use.",
               "Your earlier boards are already at work; the steward remembers them."}},
             {"paid",
              {"The steward remembers the relief payment already received.",
               "Your earlier coins already funded the work; there is no second bill."}},
             {"refused",
              {"You refused the earlier offer. The steward remembers; you owe no delivery.",
               "Your earlier refusal stands. This meeting creates no promise or debt."}},
             {"delivered",
              {"The recipient checks the delivered boards and grants one relief favor.",
               "The boards are checked in. Your promise is fulfilled and one favor is granted."}},
             {"broken",
              {"The steward hears your explicit breach. The promise is broken; no favor is granted.",
               "You explicitly break your word. The steward is disappointed; no favor is granted."}},
             {"ack",
              {"This acknowledgment takes no resources and grants no new favor.",
               "The earlier outcome is acknowledged without another charge or favor."}},
             {"left",
              {"You leave this meeting. An unsettled promise stays pending.",
               "You leave without settling; nothing has been delivered or rewarded here."}}}
        };
    }
}
