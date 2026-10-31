#include "aftermath.h"

#include <limits>
#include <stdexcept>

namespace OfflineStory {
    namespace {
        constexpr std::string_view beats[] = {"steward", "ferry", "reported", "reconciled", "booked", "left"};
        Reliability fact(const PayoffContent& c, const PayoffState& s) {
            if (s.obligation == Obligation::Fulfilled)
                return Reliability::Kept;
            if (s.obligation == Obligation::Broken)
                return Reliability::Broken;
            if (s.obligation == Obligation::Pending)
                return Reliability::Pending;
            switch (c.source().state.resolution) {
                case ReliefResolution::Refused:
                    return Reliability::Declined;
                case ReliefResolution::Helped:
                    return Reliability::Helped;
                case ReliefResolution::Paid:
                    return Reliability::Paid;
                default:
                    return Reliability::Unknown;
            }
        }
        bool witnessed(Reliability r, RecipientKnowledge k) noexcept {
            return (r == Reliability::Kept && k == RecipientKnowledge::CheckedDelivery) ||
                   (r == Reliability::Broken && k == RecipientKnowledge::HeardBreach) ||
                   (r == Reliability::Declined && k == RecipientKnowledge::HeardRefusal) ||
                   (r == Reliability::Helped && k == RecipientKnowledge::SawHelp) ||
                   (r == Reliability::Paid && k == RecipientKnowledge::SawPayment) ||
                   (r == Reliability::Pending && k == RecipientKnowledge::HeardPromise);
        }
        AftermathState initial(const AftermathContent& c) {
            AftermathState s;
            s.world = c.source().state.world;
            auto& nera = s.observers[0];
            nera.known = c.fact();
            nera.evidence = ReliabilityEvidence::Direct;
            nera.trust = c.source().state.recipient_attitude;
            nera.belief = c.source().state.believes_broken ? ReliabilityBelief::Unreliable
                          : c.fact() == Reliability::Kept || c.fact() == Reliability::Helped || c.fact() == Reliability::Paid
                              ? ReliabilityBelief::Reliable
                              : ReliabilityBelief::Uncertain;
            return s;
        }
        void report(AftermathState& s, Reliability r) noexcept {
            s.reported = true;
            auto& mara = s.observers[1];
            mara.known = r;
            mara.evidence = ReliabilityEvidence::Report;
            mara.trust = r == Reliability::Kept                               ? 2
                         : r == Reliability::Broken                           ? -2
                         : r == Reliability::Helped || r == Reliability::Paid ? 1
                                                                              : 0;
            mara.belief = mara.trust > 0   ? ReliabilityBelief::Reliable
                          : mara.trust < 0 ? ReliabilityBelief::Unreliable
                                           : ReliabilityBelief::Uncertain;
        }
        void reconcile(AftermathState& s) noexcept {
            s.reconciled = true;
            s.restitution_paid = 2;
            for (auto& observer : s.observers) {
                observer.trust = 0;
                observer.belief = ReliabilityBelief::Repairing;
            }
        }
        int fare(const AftermathState& s) noexcept {
            const auto& mara = s.observers[1];
            if (s.reconciled || mara.evidence == ReliabilityEvidence::None)
                return 3;
            if (mara.known == Reliability::Kept)
                return 1;
            if (mara.known == Reliability::Broken)
                return 5;
            if (mara.known == Reliability::Helped || mara.known == Reliability::Paid)
                return 2;
            return 3;
        }
        std::string_view steward(Reliability r) {
            switch (r) {
                case Reliability::Kept:
                    return "Nera: I checked your delivery. Your promise was kept.";
                case Reliability::Broken:
                    return "Nera: You declared the promise broken without delivering. I remember that.";
                case Reliability::Declined:
                    return "Nera: You declined the bargain. There was no promise to break.";
                case Reliability::Helped:
                    return "Nera: I saw your boards go into the work immediately.";
                case Reliability::Paid:
                    return "Nera: I accepted your payment for the relief work.";
                case Reliability::Pending:
                    return "Nera: Your recorded promise remains pending. I cannot report a checked delivery or a declared breach.";
                default:
                    throw std::logic_error("Unsupported reliability fact");
            }
        }
        std::string_view informed(Reliability r) {
            switch (r) {
                case Reliability::Kept:
                    return "Nera tells Mara she checked the delivery. Mara: A kept promise earns a one-coin return fare.";
                case Reliability::Broken:
                    return "Nera tells Mara she heard you declare the promise broken. Mara: The return fare is five coins. You can first pay Nera two coins in restitution to begin making amends.";
                case Reliability::Declined:
                    return "Nera tells Mara you declined without making a promise. Mara: That is no broken word. The return fare is three coins.";
                case Reliability::Helped:
                    return "Nera tells Mara she saw your immediate material help. Mara: Your help earns a two-coin return fare.";
                case Reliability::Paid:
                    return "Nera tells Mara she accepted your relief payment. Mara: Your help earns a two-coin return fare.";
                case Reliability::Pending:
                    return "Nera tells Mara only that a promise is still pending, with no checked delivery or declared breach. Mara: I will use the ordinary three-coin fare.";
                default:
                    throw std::logic_error("Unsupported report");
            }
        }
        struct Stop {};
        struct Budget {
            const AftermathLimits& limits;
            Usage usage;
            Error error{Code::Conflict, "aftermath", {}};
            void need(bool ok, Code code, std::string_view field) {
                if (!ok) {
                    error = {code, std::string(field), {}};
                    throw Stop{};
                }
            }
            void storage(std::size_t n) {
                need(n <= limits.storage_bytes - usage.storage_bytes, Code::StorageLimit, "aftermath.storage");
                usage.storage_bytes += n;
            }
            void work(std::size_t n) {
                need(n <= limits.work - usage.work, Code::WorkLimit, "aftermath.work");
                usage.work += n;
            }
            void text(std::string_view s) {
                need(
                    !s.empty() && s.size() <= limits.text_length && s.size() <= limits.text_bytes - usage.text_bytes, Code::TextLimit,
                    "aftermath.text"
                );
                usage.text_bytes += s.size();
                storage(s.size() + 1);
                work(s.size() + 1);
            }
        };
    }
    AftermathContent::AftermathContent(
        PayoffHandle p,
        const PayoffCheckpoint& s,
        D::DefinitionHandle d,
        Reliability r,
        Usage u,
        std::size_t retained
    )
    : previous_(std::move(p)), source_(s), definition_(std::move(d)), fact_(r), usage_(u), retained_bytes_(retained) {}

    std::expected<AftermathContent, Error>
    compose_aftermath(PayoffHandle previous, const PayoffCheckpoint& source, const AftermathLimits& limits) {
        if (!previous)
            return std::unexpected(Error{Code::EmptyHandle, "aftermath.previous", {}});
        for (auto f :
             {&AftermathLimits::nodes, &AftermathLimits::choices, &AftermathLimits::text_length, &AftermathLimits::text_bytes,
              &AftermathLimits::storage_bytes, &AftermathLimits::retained_bytes, &AftermathLimits::work})
            if (limits.*f > maximum_aftermath_limits.*f)
                return std::unexpected(Error{Code::InvalidLimits, "aftermath.limits", {}});
        Budget b{limits};
        try {
            b.need(limits.nodes >= 6 && limits.choices >= 11, Code::CountLimit, "aftermath.shape");
            const auto owned = previous->usage().owned.storage_bytes, retained = previous->usage().retained_bytes;
            b.need(
                retained <= limits.retained_bytes && owned <= limits.retained_bytes - retained, Code::StorageLimit, "aftermath.retained"
            );
            b.need(
                previous->event().location == ReliefLocation::Quay && previous->event().recipient == "nera", Code::Context,
                "aftermath.witness"
            );
            for (const auto* id : {&source.dialogue.definition.value, &source.dialogue.session.value, &source.dialogue.node.value})
                b.text(*id);
            b.work(128 * 1024); // Reserve bounded parent-pair validation before its Session allocation boundary.
            b.storage(
                sizeof(AftermathContent) + sizeof(AftermathState) + 2 * sizeof(D::Session) + sizeof(D::Draft) + 6 * sizeof(D::Node) +
                11 * sizeof(D::Choice) + 3 * 2049
            );
            auto validator = PayoffEncounter::start(previous, {"aftermath-validation"}).value();
            auto checked = validator.restore(source);
            if (!checked)
                return std::unexpected(std::move(checked.error()));
            const auto* terminal = previous->definition()->find_node(source.dialogue.node);
            b.need(
                terminal && terminal->kind == D::NodeKind::Terminal && source.state.outcome != PayoffOutcome::Open, Code::Context,
                "aftermath.completed"
            );
            const auto r = fact(*previous, source.state);
            b.need(witnessed(r, source.state.recipient_knows), Code::Context, "aftermath.knowledge");
            const auto choice = [](std::string id, std::string text, std::string target, std::string key) {
                return D::Choice{{std::move(id)}, std::move(text), {std::move(target)}, D::ConditionKey{"can_" + key}, D::EffectKey{key}};
            };
            const auto visit = choice("visit-ferry", "Visit Mara without a relief report", "ferry", "visit");
            const auto tell = choice("report", "Ask Nera to give Mara her witnessed account", "reported", "report");
            const auto buy = choice("book-passage", "Book the return passage at the displayed fare", "booked", "book");
            const auto amend = choice("reconcile", "Pay Nera two coins in restitution", "reconciled", "reconcile");
            const auto leave = choice("leave", "Leave without booking a return passage", "left", "leave");
            D::Draft draft{
                {"aftermath-v1-" + previous->definition()->id().value + "-r" + std::to_string(previous->definition()->revision()) + "-" +
                 source.dialogue.node.value + "-s" + std::to_string(source.dialogue.step)},
                1,
                {"steward"},
                {{{"steward"},
                  std::string(steward(r)) + " Ask me to speak to Mara, visit her ferry desk, or leave.",
                  D::NodeKind::Prompt,
                  {tell, visit, leave}},
                 {{"ferry"},
                  "Mara: I have no report about the relief work. A return seat costs three coins. Nera is nearby if you want her account.",
                  D::NodeKind::Prompt,
                  {tell, buy, leave}},
                 {{"reported"}, std::string(informed(r)), D::NodeKind::Prompt, {buy, amend, leave}},
                 {{"reconciled"},
                  "Nera accepts two coins in restitution. The past breach remains recorded. Mara: You are making amends; the return fare is now three coins.",
                  D::NodeKind::Prompt,
                  {buy, leave}},
                 {{"booked"}, "Mara takes the displayed fare. Your return passage is booked.", D::NodeKind::Terminal, {}},
                 {{"left"},
                  "You leave the quay without booking a return passage; earlier obligations are unchanged.",
                  D::NodeKind::Terminal,
                  {}}}
            };
            D::OperationKeys keys;
            for (auto key : {"visit", "report", "book", "reconcile", "leave"}) {
                keys.conditions.push_back({"can_" + std::string(key)});
                keys.effects.push_back({key});
            }
            b.text(draft.id.value);
            for (const auto& node : draft.nodes) {
                b.text(node.id.value);
                b.text(node.text);
                for (const auto& c : node.choices) {
                    b.text(c.id.value);
                    b.text(c.text);
                    b.text(c.target.value);
                    b.text(c.condition->value);
                    b.text(c.effect->value);
                }
            }
            for (const auto& key : keys.conditions)
                b.text(key.value);
            for (const auto& key : keys.effects)
                b.text(key.value);
            b.work(b.usage.text_bytes); // Freeze/copy charge; this is logical work, not a CPU-instruction bound.
            auto compiled = D::compile(draft, keys);
            if (!compiled)
                return std::unexpected(Error{Code::Compile, "aftermath.definition", {}, 0, std::move(compiled.error())});
            return AftermathContent(std::move(previous), source, std::move(*compiled), r, b.usage, retained + owned);
        } catch (const Stop&) {
            return std::unexpected(std::move(b.error));
        }
    }
    AftermathEncounter::AftermathEncounter(AftermathContent c, D::Session s)
    : content_(std::move(c)), session_(std::move(s)), state_(initial(content_)) {}
    std::expected<AftermathEncounter, Error> AftermathEncounter::start(AftermathContent c, D::SessionId id) {
        auto session = D::Session::start(c.definition(), std::move(id));
        if (!session)
            return std::unexpected(Error{Code::Resume, "aftermath.session", {}, 0, {}, session.error()});
        return AftermathEncounter(std::move(c), std::move(*session));
    }
    int AftermathEncounter::fare() const noexcept {
        return OfflineStory::fare(state_);
    }
    bool AftermathEncounter::coherent(const AftermathState& s) const noexcept {
        if (!content_.previous() || !valid_state(s.world) || s.beat < AftermathBeat::Steward || s.beat > AftermathBeat::Left)
            return false;
        auto expected = initial(content_);
        auto world = s.world;
        world.facts.coins = expected.world.facts.coins;
        if (world != expected.world || (s.reconciled && (!s.reported || content_.fact() != Reliability::Broken)))
            return false;
        if (s.reported)
            report(expected, content_.fact());
        if (s.reconciled)
            reconcile(expected);
        if (s.observers != expected.observers || s.restitution_paid != expected.restitution_paid)
            return false;
        if ((s.beat == AftermathBeat::Steward || s.beat == AftermathBeat::Ferry) && (s.reported || s.reconciled))
            return false;
        if (s.beat == AftermathBeat::Reported && (!s.reported || s.reconciled))
            return false;
        if (s.beat == AftermathBeat::Reconciled && !s.reconciled)
            return false;
        if (s.beat == AftermathBeat::Booked || s.beat == AftermathBeat::Left) {
            const auto charge = s.beat == AftermathBeat::Booked ? OfflineStory::fare(s) : 0;
            return s.settlement_coins >= charge && s.settlement_coins <= 100 && s.world.facts.coins == s.settlement_coins - charge &&
                   s.fare_paid == charge && s.return_passage == (s.beat == AftermathBeat::Booked);
        }
        return s.fare_paid == 0 && !s.return_passage && s.settlement_coins == -1;
    }
    bool AftermathEncounter::eligible(std::string_view key) const noexcept {
        if (!coherent(state_) || state_.beat == AftermathBeat::Booked || state_.beat == AftermathBeat::Left)
            return false;
        if (key == "can_visit")
            return state_.beat == AftermathBeat::Steward;
        if (key == "can_report")
            return !state_.reported && (state_.beat == AftermathBeat::Steward || state_.beat == AftermathBeat::Ferry) &&
                   witnessed(content_.fact(), content_.source().state.recipient_knows);
        if (key == "can_book")
            return state_.beat != AftermathBeat::Steward && state_.world.facts.coins >= fare();
        if (key == "can_reconcile")
            return state_.beat == AftermathBeat::Reported && content_.fact() == Reliability::Broken && state_.reported &&
                   !state_.reconciled && state_.world.facts.coins >= 2;
        return key == "can_leave";
    }
    std::expected<D::View, D::SessionError> AftermathEncounter::view() {
        return session_.view([this](const auto& key) { return eligible(key.value); });
    }
    std::expected<D::Transition, D::SessionError> AftermathEncounter::choose(const D::Selection& selected) {
        static_assert(std::is_nothrow_copy_assignable_v<AftermathState>);
        return session_.choose_with_transition(
            selected, [this](const auto& key) { return eligible(key.value); },
            [this](const auto& key) {
                const std::string_view condition = key.value == "visit"       ? "can_visit"
                                                   : key.value == "report"    ? "can_report"
                                                   : key.value == "book"      ? "can_book"
                                                   : key.value == "reconcile" ? "can_reconcile"
                                                   : key.value == "leave"     ? "can_leave"
                                                                              : "";
                if (!eligible(condition))
                    throw std::logic_error("Invalid aftermath effect");
                auto next = state_;
                if (key.value == "visit")
                    next.beat = AftermathBeat::Ferry;
                else if (key.value == "report") {
                    report(next, content_.fact());
                    next.beat = AftermathBeat::Reported;
                } else if (key.value == "reconcile") {
                    next.world.facts.coins -= 2;
                    reconcile(next);
                    next.beat = AftermathBeat::Reconciled;
                } else {
                    next.settlement_coins = next.world.facts.coins;
                    if (key.value == "book") {
                        next.fare_paid = fare();
                        next.world.facts.coins -= next.fare_paid;
                        next.return_passage = true;
                        next.beat = AftermathBeat::Booked;
                    } else
                        next.beat = AftermathBeat::Left;
                }
                if (!coherent(next))
                    throw std::logic_error("Invalid staged aftermath");
                state_ = next;
                if (effects_ != std::numeric_limits<std::uint64_t>::max())
                    ++effects_;
            }
        );
    }
    std::expected<AftermathCheckpoint, D::SessionError> AftermathEncounter::capture() {
        auto snapshot = session_.snapshot();
        if (!snapshot)
            return std::unexpected(snapshot.error());
        return AftermathCheckpoint{std::move(*snapshot), state_};
    }
    std::expected<void, Error> AftermathEncounter::restore(const AftermathCheckpoint& saved) {
        if (!coherent(saved.state))
            return std::unexpected(Error{Code::Context, "aftermath.state", {}});
        if (saved.dialogue.node.value != beats[static_cast<std::size_t>(saved.state.beat)])
            return std::unexpected(Error{Code::Context, "aftermath.cursor", {}});
        auto fresh = D::Session::resume(content_.definition(), saved.dialogue);
        if (!fresh)
            return std::unexpected(Error{Code::Resume, "aftermath.dialogue", {}, 0, {}, fresh.error()});
        static_assert(std::is_nothrow_move_assignable_v<D::Session>);
        std::expected<void, Error> success;
        session_ = std::move(*fresh);
        state_ = saved.state;
        return success;
    }
    bool AftermathEncounter::resources(int coins) noexcept {
        if (state_.beat == AftermathBeat::Booked || state_.beat == AftermathBeat::Left)
            return false;
        auto next = state_;
        next.world.facts.coins = coins;
        if (!coherent(next))
            return false;
        state_ = next;
        return true;
    }
}
