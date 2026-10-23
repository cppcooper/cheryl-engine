#include "score-input.h"
#include "settings.h"

#include <stdexcept>
#include <utility>

namespace Tutorial {
    namespace {
        constexpr CE::Input::Actions::Id score_action{1};

        void validate_key(CE::Input::KeyboardKey key) {
            GameSettings value;
            value.add_score = key;
            validate_settings(value);
        }

        CE::Input::Actions::ContextDefinition score_definition(CE::Input::DeviceId keyboard, CE::Input::KeyboardKey key) {
            validate_key(key);
            return {.buttons = {{{{{keyboard, key}}}, score_action}}};
        }

        bool contains(const CE::Input::Tick& tick, CE::Input::Actions::ContextId context) noexcept {
            for (const auto& poll : tick.polls())
                if (poll->has_context(context))
                    return true;
            return false;
        }
    }

    struct ScoreInput::Replacement {
        CE::Input::Actions::Context lease;
        CE::Input::KeyboardKey key;
        Generation generation;
        Status state = Status::Waiting;
    };

    ScoreInput::Candidate::Candidate(std::shared_ptr<Replacement> replacement) noexcept
    : replacement_(std::move(replacement)) {}
    CE::Input::Actions::ContextId ScoreInput::Candidate::id() const noexcept {
        return replacement_ ? replacement_->lease.id() : 0;
    }
    ScoreInput::Generation ScoreInput::Candidate::generation() const noexcept {
        return replacement_ ? replacement_->generation : 0;
    }
    ScoreInput::Prepared::Prepared(std::shared_ptr<Replacement> replacement) noexcept
    : replacement_(std::move(replacement)) {}
    CE::Input::Actions::ContextId ScoreInput::Prepared::id() const noexcept {
        return replacement_ ? replacement_->lease.id() : 0;
    }

    ScoreInput::ScoreInput(CE::Input::Actions::ContextSubmission endpoint, CE::Input::DeviceId keyboard, CE::Input::KeyboardKey key)
    : endpoint_(std::move(endpoint)), keyboard_(keyboard), active_(endpoint_.attach(score_definition(keyboard, key))), key_(key) {}

    ScoreInput::~ScoreInput() {
        cancel();
    }

    ScoreInput::ScoreInput(ScoreInput&& other) noexcept
    : endpoint_(std::move(other.endpoint_)),
      keyboard_(other.keyboard_),
      active_(std::move(other.active_)),
      key_(other.key_),
      observed_(std::exchange(other.observed_, false)),
      generation_(other.generation_),
      pending_(std::move(other.pending_)) {}

    ScoreInput& ScoreInput::operator=(ScoreInput&& other) noexcept {
        if (this != &other) {
            cancel();
            endpoint_ = std::move(other.endpoint_);
            keyboard_ = other.keyboard_;
            active_ = std::move(other.active_);
            key_ = other.key_;
            observed_ = std::exchange(other.observed_, false);
            generation_ = other.generation_;
            pending_ = std::move(other.pending_);
        }
        return *this;
    }

    void ScoreInput::invalidate(Status reason) noexcept {
        if (const auto pending = pending_.lock()) {
            pending->state = reason;
            pending->lease.reset();
        }
        pending_.reset();
    }

    void ScoreInput::cancel() noexcept {
        invalidate(Status::Cancelled);
    }

    void ScoreInput::set_generation(Generation generation) noexcept {
        if (generation_ != generation) {
            invalidate(Status::Stale);
            generation_ = generation;
        }
    }

    std::optional<ScoreInput::Candidate> ScoreInput::stage(CE::Input::KeyboardKey key) {
        // Validate before any attachment/slot changes, using the sole key whitelist.
        validate_key(key);
        if (!active_)
            throw std::logic_error("Score input active context is invalidated or moved from");
        if (!pending_.expired())
            throw std::logic_error("Score input already has a pending replacement");
        if (key == key_)
            return std::nullopt;
        auto pending = std::make_shared<Replacement>();
        pending->key = key;
        pending->generation = generation_;
        pending->lease = endpoint_.attach(score_definition(keyboard_, key));
        pending_ = pending;
        return Candidate(std::move(pending));
    }

    ScoreInput::Status ScoreInput::status(const std::shared_ptr<Replacement>& replacement) const noexcept {
        if (!replacement)
            return Status::Invalidated;
        if (replacement->state == Status::Cancelled || replacement->state == Status::Stale)
            return replacement->state;
        if (pending_.lock() != replacement)
            return Status::Foreign;
        if (replacement->generation != generation_)
            return Status::Stale;
        if (!active_ || !replacement->lease)
            return Status::Invalidated;
        return replacement->state;
    }

    ScoreInput::Status ScoreInput::observe(const CE::Input::Tick& tick) noexcept {
        if (!active_) {
            observed_ = false;
            return Status::Invalidated;
        }
        observed_ = observed_ || contains(tick, active_.id());
        return active_ ? (observed_ ? Status::Ready : Status::Waiting) : Status::Invalidated;
    }

    ScoreInput::Status ScoreInput::observe(Candidate& candidate, const CE::Input::Tick& tick) noexcept {
        auto& pending = candidate.replacement_;
        if (status(pending) == Status::Waiting && contains(tick, pending->lease.id()))
            pending->state = Status::Ready;
        return status(pending);
    }

    CE::Input::Actions::ButtonTickState ScoreInput::read(const CE::Input::Tick& tick) const {
        return active_ && observed_ ? tick.actions(active_).button(score_action) : CE::Input::Actions::ButtonTickState{};
    }

    ScoreInput::Prepared ScoreInput::prepare(Candidate&& candidate) {
        if (status(candidate.replacement_) != Status::Ready)
            throw std::logic_error("Score input candidate must be owned, live, current and observed before preparation");
        return Prepared(std::move(candidate.replacement_));
    }

    bool ScoreInput::adopt(Prepared&& prepared) noexcept {
        if (status(prepared.replacement_) != Status::Ready)
            return false;
        auto replacement = std::move(prepared.replacement_);
        active_ = std::move(replacement->lease);
        key_ = replacement->key;
        observed_ = true;
        pending_.reset();
        return true;
    }
}
