#include "story.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace OfflineStory {
    bool valid_state(const State& state) noexcept {
        const auto& f = state.facts;
        if (f.coins < 0 || f.coins > 100 || f.planks < 0 || f.planks > 100 || f.paid + f.repaired + f.declined > 1 ||
            f.passage != (f.paid || f.repaired) || f.licensed != f.passage)
            return false;
        for (std::size_t i = 0; i < 3; ++i) {
            if (state.knowledge[i] < Knowledge::None || state.knowledge[i] > Knowledge::HeardPassage || state.attitude[i] < -100 ||
                state.attitude[i] > 100)
                return false;
            if (state.knowledge[i] == Knowledge::None && (state.attitude[i] != 0 || state.believes_unlicensed[i]))
                return false;
        }
        return true;
    }
    bool eligible(const Outcome& outcome, const State& state) noexcept {
        const auto& f = state.facts;
        const auto& c = outcome.consequence;
        if (!valid_state(state) || c.coins < 0 || c.coins > 100 || c.planks < 0 || c.planks > 100 || f.passage || f.declined ||
            f.coins < c.coins || f.planks < c.planks)
            return false;
        for (std::size_t i = 0; i < 3; ++i)
            if (c.attitude[i] < -10 || c.attitude[i] > 10 || state.attitude[i] + c.attitude[i] < -100 ||
                state.attitude[i] + c.attitude[i] > 100)
                return false;
        return true;
    }
    State stage(const Outcome& outcome, const State& source) {
        if (!eligible(outcome, source))
            throw std::logic_error("Host outcome is no longer eligible");
        auto result = source;
        auto& f = result.facts;
        const auto& c = outcome.consequence;
        f.coins -= c.coins;
        f.planks -= c.planks;
        f.passage = c.passage;
        f.licensed = c.passage;
        f.paid = c.paid;
        f.repaired = c.repaired;
        f.declined = c.declined;
        result.knowledge[0] = c.paid ? Knowledge::SawFare : c.repaired ? Knowledge::SawRepair : Knowledge::SawDecline;
        if (c.passage) {
            result.knowledge[1] = Knowledge::HeardPassage;
            result.believes_unlicensed[1] = true; // Watch belief, deliberately different from authoritative licensed passage.
        }
        for (std::size_t i = 0; i < 3; ++i)
            result.attitude[i] += c.attitude[i];
        if (!valid_state(result))
            throw std::logic_error("Host outcome produced an invalid staged state");
        return result;
    }
    Encounter::Encounter(ProjectionHandle projection, D::Session session)
    : projection_(std::move(projection)), session_(std::move(session)), state_(projection_->accepted().context().state) {}

    std::expected<Encounter, Error> Encounter::start(ProjectionHandle projection, D::SessionId id) {
        if (!projection)
            return std::unexpected(Error{Code::EmptyHandle, "projection", {}});
        auto session = D::Session::start(projection->definition(), std::move(id));
        if (!session)
            return std::unexpected(Error{Code::Resume, "session", {}, 0, {}, session.error()});
        return Encounter(std::move(projection), std::move(*session));
    }
    const Outcome& Encounter::rule(std::string_view key, std::string_view prefix) const {
        if (!key.starts_with(prefix))
            throw std::logic_error("Unknown projected operation prefix");
        const auto& content = projection_->accepted();
        const auto found = std::find_if(content.set().actions.begin(), content.set().actions.end(), [&](const auto& row) {
            return row.id == key.substr(prefix.size());
        });
        if (found == content.set().actions.end())
            throw std::logic_error("Unknown projected action key");
        const auto policy = std::find_if(content.catalogue().outcomes.begin(), content.catalogue().outcomes.end(), [&](const auto& row) {
            return row.id == found->outcome;
        });
        if (policy == content.catalogue().outcomes.end())
            throw std::logic_error("Accepted outcome lost");
        return *policy;
    }
    std::expected<D::View, D::SessionError> Encounter::view() {
        return session_.view([this](const auto& key) { return eligible(rule(key.value, "eligible-"), state_); });
    }
    std::expected<D::Transition, D::SessionError> Encounter::choose(const D::Selection& selection) {
        static_assert(std::is_nothrow_copy_assignable_v<State>);
        return session_.choose_with_transition(
            selection, [this](const auto& key) { return eligible(rule(key.value, "eligible-"), state_); },
            [this](const auto& key) {
                const auto prepared = stage(rule(key.value, "apply-"), state_);
                // Serialized owner; all checks/staging precede nonthrowing adoption.
                state_ = prepared;
                if (effects_ != std::numeric_limits<std::uint64_t>::max())
                    ++effects_;
            }
        );
    }
    std::expected<Checkpoint, D::SessionError> Encounter::capture() {
        auto saved = session_.snapshot();
        if (!saved)
            return std::unexpected(saved.error());
        return Checkpoint{std::move(*saved), state_};
    }
    std::expected<void, Error> Encounter::restore(const Checkpoint& saved) {
        if (!projection_)
            return std::unexpected(Error{Code::EmptyHandle, "projection", {}});
        if (!valid_state(saved.state))
            return std::unexpected(Error{Code::Context, "checkpoint.state", {}});
        auto loaded = D::Session::resume(projection_->definition(), saved.dialogue);
        if (!loaded)
            return std::unexpected(Error{Code::Resume, "checkpoint.dialogue", {}, 0, {}, loaded.error()});
        static_assert(std::is_nothrow_move_assignable_v<D::Session>);
        static_assert(std::is_nothrow_move_constructible_v<std::expected<void, Error>>);
        std::expected<void, Error> success;
        session_ = std::move(*loaded);
        state_ = saved.state;
        return success;
    }
    bool Encounter::resources(int coins, int planks) noexcept {
        if (!projection_)
            return false;
        auto staged = state_;
        staged.facts.coins = coins;
        staged.facts.planks = planks;
        if (!valid_state(staged))
            return false;
        state_ = staged;
        return true;
    }
    void print_state(std::ostream& out, const State& state, const Accepted& accepted) {
        const auto& f = state.facts;
        out << "Actual facts: coins=" << f.coins << " planks=" << f.planks << " passage=" << f.passage << " licensed=" << f.licensed
            << " paid=" << f.paid << " repaired=" << f.repaired << " declined=" << f.declined << '\n';
        for (std::size_t i = 0; i < 3; ++i) {
            out << "Observer " << accepted.context().observers[i] << ": knowledge=";
            switch (state.knowledge[i]) {
                case Knowledge::None:
                    out << "unaware";
                    break;
                case Knowledge::SawFare:
                    out << "saw-fare";
                    break;
                case Knowledge::SawRepair:
                    out << "saw-repair";
                    break;
                case Knowledge::SawDecline:
                    out << "saw-decline";
                    break;
                case Knowledge::HeardPassage:
                    out << "heard-passage";
                    break;
            }
            out << " belief-unlicensed=" << state.believes_unlicensed[i] << " attitude=" << state.attitude[i] << '\n';
        }
    }
}
