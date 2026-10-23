#include "settings-apply.h"

#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Tutorial {
    static_assert(std::is_trivially_copyable_v<GameSettings>);
    static_assert(std::is_nothrow_copy_assignable_v<GameSettings> && std::is_nothrow_copy_constructible_v<GameSettings>);
    static_assert(std::is_nothrow_move_constructible_v<ScoreInput::Prepared> && std::is_nothrow_destructible_v<ScoreInput::Prepared>);

    SettingsApply::SettingsApply(GameSettings& active, ScoreInput& score, const SettingsFile& file)
    : active_(active), score_(score), file_(file), draft_(active), generation_(score.generation_) {
        validate_settings(active);
        if (active.add_score != score.key())
            throw std::invalid_argument("Settings active add_score differs from the score owner key");
        if (!score.pending_.expired())
            throw std::logic_error("Settings Apply requires exclusive score candidate management");
    }

    SettingsApply::Generation SettingsApply::next_generation() const {
        if (generation_ == std::numeric_limits<Generation>::max())
            throw std::overflow_error("Settings Apply generation is exhausted");
        return generation_ + 1;
    }

    CE::Input::Actions::ContextId SettingsApply::pending_context() const noexcept {
        return candidate_ ? candidate_->id() : 0;
    }

    void SettingsApply::edit(GameSettings draft) {
        const auto generation = next_generation();
        candidate_.reset();
        score_.set_generation(generation);
        generation_ = generation;
        draft_ = draft;
        state_ = Status::Idle;
    }

    SettingsApply::Generation SettingsApply::request() {
        validate_settings(draft_);
        if (state_ == Status::Pending || state_ == Status::Ready)
            throw std::logic_error("Settings Apply already has a pending request");
        if (state_ == Status::Applied)
            throw std::logic_error("Settings Apply requires an edit after completion");
        if (!score_.pending_.expired())
            throw std::logic_error("Settings Apply score owner has a foreign pending request");
        if (active_.add_score != score_.key())
            throw std::logic_error("Settings Apply active values and score key disagree");
        const auto generation = next_generation();
        score_.set_generation(generation);
        generation_ = generation;
        state_ = Status::Idle;
        candidate_ = score_.stage(draft_.add_score);
        state_ = Status::Pending;
        return generation;
    }

    SettingsApply::Status SettingsApply::status(Generation request) const noexcept {
        if (request != generation_)
            return Status::Stale;
        if (state_ != Status::Pending && state_ != Status::Ready)
            return state_;
        if (score_.generation_ != generation_)
            return Status::Stale;
        if (!score_.active_ || active_.add_score != score_.key())
            return Status::Invalidated;
        if (candidate_) {
            switch (score_.status(*candidate_)) {
                case ScoreInput::Status::Waiting:
                    return Status::Pending;
                case ScoreInput::Status::Ready:
                    return score_.observed_ ? Status::Ready : Status::Pending;
                case ScoreInput::Status::Cancelled:
                    return Status::Cancelled;
                case ScoreInput::Status::Stale:
                    return Status::Stale;
                default:
                    return Status::Invalidated;
            }
        }
        return score_.observed_ ? Status::Ready : Status::Pending;
    }

    SettingsApply::Status SettingsApply::observe(Generation request, const CE::Input::Tick& tick) noexcept {
        const auto current = status(request);
        if (request != generation_ || (state_ != Status::Pending && state_ != Status::Ready))
            return current;
        score_.observe(tick);
        if (candidate_)
            static_cast<void>(score_.observe(*candidate_, tick));
        state_ = status(request);
        if (state_ != Status::Pending && state_ != Status::Ready)
            candidate_.reset();
        return state_;
    }

    SettingsApply::Status SettingsApply::cancel(Generation request) noexcept {
        if (request != generation_)
            return Status::Stale;
        if (state_ == Status::Pending || state_ == Status::Ready) {
            candidate_.reset();
            state_ = Status::Cancelled;
        }
        return state_;
    }

    void SettingsApply::complete(GameSettings accepted, ScoreInput::Prepared&& prepared, bool replacing) noexcept {
        static_assert(noexcept(score_.adopt_ready(std::move(prepared))));
        if (replacing)
            score_.adopt_ready(std::move(prepared));
        active_ = accepted;
        state_ = Status::Applied;
    }

    SettingsApply::Status SettingsApply::commit(Generation request) {
        const auto current = status(request);
        if (current != Status::Ready) {
            if (request == generation_ && (state_ == Status::Pending || state_ == Status::Ready) && current != Status::Pending) {
                candidate_.reset();
                state_ = current;
            }
            return current;
        }
        const GameSettings accepted = draft_;
        const bool replacing = candidate_.has_value();
        ScoreInput::Prepared prepared;
        static_assert(noexcept(complete(accepted, std::move(prepared), replacing)));
        try {
            if (replacing)
                prepared = score_.prepare(std::move(*candidate_));
            candidate_.reset();
            file_.save(accepted);
        } catch (...) {
            candidate_.reset();
            state_ = Status::Failed;
            throw; // Preserve the primary diagnostic/type; Prepared releases its lease.
        }
        // Save has committed. No checks, callbacks, formatting or fallible results
        // remain. Serialized access/no teardown keeps the preflight proof valid.
        complete(accepted, std::move(prepared), replacing);
        return Status::Applied;
    }
}
