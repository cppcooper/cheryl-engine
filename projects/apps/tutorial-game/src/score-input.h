#pragma once

#include <cheryl/core/controls/action-context.h>
#include <cheryl/core/controls/tick-input.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace Tutorial {
    // App-private protocol. All access belongs to one serialized caller, including
    // token moves/destruction. The endpoint does not retain the native input owner.
    class ScoreInput final {
        struct Replacement;

    public:
        using Generation = std::uint64_t;
        enum class Status { Waiting, Ready, Cancelled, Stale, Invalidated, Foreign };

        class Candidate final {
            std::shared_ptr<Replacement> replacement_;
            explicit Candidate(std::shared_ptr<Replacement> replacement) noexcept;
            friend class ScoreInput;

        public:
            Candidate() = default;
            Candidate(Candidate&&) noexcept = default;
            Candidate& operator=(Candidate&&) noexcept = default;
            Candidate(const Candidate&) = delete;
            Candidate& operator=(const Candidate&) = delete;
            // Diagnostic identities, never proof of readiness or live ownership.
            [[nodiscard]] CE::Input::Actions::ContextId id() const noexcept;
            [[nodiscard]] Generation generation() const noexcept;
        };

        class Prepared final {
            std::shared_ptr<Replacement> replacement_;
            explicit Prepared(std::shared_ptr<Replacement> replacement) noexcept;
            friend class ScoreInput;

        public:
            Prepared() = default;
            Prepared(Prepared&&) noexcept = default;
            Prepared& operator=(Prepared&&) noexcept = default;
            Prepared(const Prepared&) = delete;
            Prepared& operator=(const Prepared&) = delete;
            [[nodiscard]] CE::Input::Actions::ContextId id() const noexcept;
        };

    private:
        CE::Input::Actions::ContextSubmission endpoint_;
        CE::Input::DeviceId keyboard_;
        CE::Input::Actions::Context active_;
        CE::Input::KeyboardKey key_;
        bool observed_ = false;
        Generation generation_ = 0;
        // Only Candidate or Prepared owns the pending record/lease. Dropping the
        // token releases it; this weak slot associates it with exactly one owner.
        std::weak_ptr<Replacement> pending_;

        [[nodiscard]] Status status(const std::shared_ptr<Replacement>& replacement) const noexcept;
        void invalidate(Status reason) noexcept;

    public:
        // Validates using the settings validator; attachment may throw. Starts Waiting.
        ScoreInput(
            CE::Input::Actions::ContextSubmission endpoint,
            CE::Input::DeviceId keyboard,
            CE::Input::KeyboardKey key = CE::Input::KeyboardKey::Space
        );
        ~ScoreInput();
        ScoreInput(ScoreInput&& other) noexcept;
        ScoreInput& operator=(ScoreInput&& other) noexcept;
        ScoreInput(const ScoreInput&) = delete;
        ScoreInput& operator=(const ScoreInput&) = delete;

        [[nodiscard]] CE::Input::KeyboardKey key() const noexcept { return key_; }
        [[nodiscard]] CE::Input::Actions::ContextId active_context() const noexcept { return active_.id(); }
        // Changing generation permanently invalidates the outstanding token, even a Prepared.
        void set_generation(Generation generation) noexcept;
        // A second pending request rejects. nullopt explicitly means unchanged;
        // validation/attachment failure leaves active and prior pending ownership intact.
        [[nodiscard]] std::optional<Candidate> stage(CE::Input::KeyboardKey key);
        // Only tick.polls() establishes installation. Evidence latches for this
        // exact lease/generation; an empty tick cannot create it. Recheck liveness.
        Status observe(const CE::Input::Tick& tick) noexcept;
        [[nodiscard]] Status observe(Candidate& candidate, const CE::Input::Tick& tick) noexcept;
        // Inactive until active installation is observed, or after invalidation.
        // Summarizes only the active namespace; owned Tick state may allocate.
        [[nodiscard]] CE::Input::Actions::ButtonTickState read(const CE::Input::Tick& tick) const;
        // Non-ready/stale/foreign tokens throw logic_error without consumption.
        [[nodiscard]] Prepared prepare(Candidate&& candidate);
        // No allocation/exception. Returns false without changing active ownership
        // for invalid tokens. After prepare, serialize caller operations and exclude
        // concurrent input teardown; this is not a cross-owner/file transaction.
        [[nodiscard]] bool adopt(Prepared&& prepared) noexcept;
        // Releases only the outstanding candidate/prepared lease; active stays owned.
        void cancel() noexcept;
    };
}
