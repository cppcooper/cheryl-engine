#pragma once

#include "score-input.h"
#include "settings.h"

namespace Tutorial {
    // App-private CPU protocol. Borrowed active/score/file must outlive this object.
    // One serialized caller owns all access, token destruction and active values.
    // While this helper lives, it exclusively manages score's pending/generation
    // protocol; ordinary active observe/read remains allowed. No callbacks here.
    class SettingsApply final {
    public:
        using Generation = ScoreInput::Generation;
        enum class Status { Idle, Pending, Ready, Applied, Cancelled, Stale, Invalidated, Failed };

    private:
        GameSettings& active_;
        ScoreInput& score_;
        const SettingsFile& file_;
        GameSettings draft_;
        Generation generation_;
        Status state_ = Status::Idle;
        std::optional<ScoreInput::Candidate> candidate_;

        [[nodiscard]] Generation next_generation() const;
        void complete(GameSettings accepted, ScoreInput::Prepared&& prepared, bool replacing) noexcept;

    public:
        // Validates active values/key agreement; no attachment, I/O or autosave.
        SettingsApply(GameSettings& active, ScoreInput& score, const SettingsFile& file);
        ~SettingsApply() = default; // Drop only our pending lease; never autosave.
        SettingsApply(const SettingsApply&) = delete;
        SettingsApply& operator=(const SettingsApply&) = delete;
        SettingsApply(SettingsApply&&) = delete;
        SettingsApply& operator=(SettingsApply&&) = delete;

        [[nodiscard]] const GameSettings& draft() const noexcept { return draft_; }
        [[nodiscard]] CE::Input::Actions::ContextId pending_context() const noexcept;
        // Editing (even to the same value) advances generation and drops a pending
        // request. Invalid intermediate values are allowed only in this private draft.
        void edit(GameSettings draft);
        // Validate the complete draft first. A second pending request rejects;
        // after Applied an explicit edit is required. Failure permits explicit retry.
        // Each attempt gets a new generation; old requests cannot affect a retry.
        [[nodiscard]] Generation request();
        [[nodiscard]] Status status(Generation request) const noexcept;
        // Consumes only the supplied tick's published polls, never polls devices.
        [[nodiscard]] Status observe(Generation request, const CE::Input::Tick& tick) noexcept;
        [[nodiscard]] Status cancel(Generation request) noexcept;
        // Pending/stale/cancelled/invalid requests do not write. Applied is an
        // idempotent terminal result for this request, not another save/adoption.
        // Ready: all checks/preparation precede save; only noexcept completion follows.
        // Exclude input clear/shutdown, owner moves, edits and foreign/reentrant
        // callbacks throughout this synchronous call. This is not cross-owner atomicity.
        // Ordinary I/O throws SettingsFileError; allocation/programming exceptions
        // propagate distinctly. All failed saves drop pending but retain the draft.
        [[nodiscard]] Status commit(Generation request);
    };
}
