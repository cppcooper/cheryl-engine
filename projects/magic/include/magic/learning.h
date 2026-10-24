#pragma once

#include <magic/compiler.h>

#include <array>
#include <optional>

namespace Magic::Learning {
    inline constexpr std::size_t envelope = 12288;
    struct Limits {
        std::size_t entries = 8;
        std::size_t text_bytes = 8192;
        std::size_t work = 262144;           // >=envelope: reserves bounded failure publication
        std::size_t retained_bytes = 524288; // logical model, not allocator/RSS
        std::uint64_t max_revision = 1'000'000;
    };
    enum class Error {
        InvalidLimits,
        InvalidIdentity,
        DuplicateIdentity,
        MissingEntry,
        EntryLimit,
        TextLimit,
        WorkLimit,
        StorageLimit,
        RevisionLimit,
        Compile,
        UnknownKnowledge,
        UnknownRule,
        ContradictoryLesson,
        Prerequisite
    };
    struct Problem {
        Error code;
        std::optional<Diagnostic> diagnostic;
        RuleMask missing = 0;
    };
    struct Usage {
        std::size_t work = 0, retained_bytes = 0;
    };
    class Lesson;
    struct Knowledge {
        std::uint32_t version = 1;
        RuleMask known = 0;
        std::uint64_t revision = 0;
        bool operator==(const Knowledge&) const = default;
    };
    // Fixture-owned knowledge; edits cannot create ReferenceHost tickets or permissions.
    class Learner final {
        Knowledge state_;
        std::array<std::shared_ptr<const Lesson>, 4> studied_;
        std::uint64_t max_revision_;
        Learner(RuleMask known, std::uint64_t max_revision)
        : state_{1, known, 0}, max_revision_(max_revision) {}
    public:
        [[nodiscard]] static std::expected<Learner, Problem> create(RuleMask known, std::uint64_t max_revision = 1'000'000);
        [[nodiscard]] Knowledge snapshot() const noexcept { return state_; }
        [[nodiscard]] RuleMask missing(const Plan& plan) const noexcept { return plan.requirements() & ~state_.known; }
        // Idempotent; only one registered school production changes, never an operation concept.
        [[nodiscard]] std::expected<bool, Problem> learn(const std::shared_ptr<const Lesson>& lesson);
    };
    class Spellbook;
    class Entry final {
        std::string identity_, title_, annotation_;
        std::uint64_t revision_ = 0;
        PlanHandle plan_;
        friend class Spellbook;
        Entry() = default;
    public:
        [[nodiscard]] std::string_view identity() const noexcept { return identity_; }
        [[nodiscard]] std::string_view title() const noexcept { return title_; }
        [[nodiscard]] std::string_view annotation() const noexcept { return annotation_; }
        [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
        [[nodiscard]] const PlanHandle& plan() const noexcept { return plan_; }
        [[nodiscard]] std::string_view source() const noexcept { return plan_->source(); }
        [[nodiscard]] std::string_view common() const noexcept { return plan_->common(); }
    };
    using EntryHandle = std::shared_ptr<const Entry>;
    class Attempt final {
        std::string draft_;
        EntryHandle accepted_;
        std::optional<Problem> problem_;
        std::size_t submitted_bytes_ = 0;
        Usage usage_;
        friend class Spellbook;
        Attempt() = default;
    public:
        [[nodiscard]] const EntryHandle& accepted() const noexcept { return accepted_; }
        [[nodiscard]] const std::optional<Problem>& problem() const noexcept { return problem_; }
        [[nodiscard]] std::string_view draft() const noexcept { return problem_ ? std::string_view(draft_) : accepted_->source(); }
        [[nodiscard]] std::size_t submitted_bytes() const noexcept { return submitted_bytes_; }
        [[nodiscard]] bool truncated() const noexcept { return draft().size() != submitted_bytes_; }
        [[nodiscard]] Usage usage() const noexcept { return usage_; }
    };
    using AttemptHandle = std::shared_ptr<const Attempt>;
    // Eight identities, one last accepted revision and one current draft each. No history/removal API.
    // Inputs borrowed synchronously; returned const handles own all text and plans. Allocation exceptions
    // precede pointer adoption. Caller-retained old handles are outside this owner's quotas.
    class Spellbook final {
        struct Slot {
            std::string id, title;
            EntryHandle accepted;
            AttemptHandle attempt;
        };
        std::array<Slot, 8> slots_;
        Limits limits_;
        std::size_t count_ = 0;
        explicit Spellbook(Limits limits)
        : limits_(limits) {}
        [[nodiscard]] std::size_t retained(std::optional<std::size_t> replacing = {}, const PlanHandle& plan = {}) const;
    public:
        Spellbook(const Spellbook&) = delete;
        Spellbook& operator=(const Spellbook&) = delete;
        [[nodiscard]] static std::expected<std::unique_ptr<Spellbook>, Problem> create(Limits limits = {});
        [[nodiscard]] std::expected<void, Problem> add(std::string_view id, std::string_view title);
        // Once ID resolves, even a rejected edit publishes an owned bounded Attempt and keeps accepted unchanged.
        [[nodiscard]] std::expected<AttemptHandle, Problem>
        edit(std::string_view id, std::string_view source, std::string_view annotation, const Catalog& catalog, const Learner& learner);
        [[nodiscard]] EntryHandle accepted(std::string_view id) const noexcept;
        [[nodiscard]] AttemptHandle draft(std::string_view id) const noexcept;
    };
    struct LessonInput {
        std::string_view identity, rule, annotation;
        std::string_view original, exact_gloss, practice, practice_gloss;
    };
    struct Alignment {
        std::string original, gloss;
        Rule production;
    };
    class Lesson final {
        std::string identity_, annotation_;
        Rule rule_ = Rule::CinderSlow;
        PlanHandle worked_, practice_;
        std::vector<Alignment> aligned_;
        Usage usage_;
        Lesson() = default;
        friend std::expected<std::shared_ptr<const Lesson>, Problem> make_lesson(const LessonInput&, const Catalog&, Limits);
    public:
        [[nodiscard]] std::string_view identity() const noexcept { return identity_; }
        [[nodiscard]] std::string_view annotation() const noexcept { return annotation_; }
        [[nodiscard]] Rule rule() const noexcept { return rule_; }
        [[nodiscard]] const PlanHandle& worked() const noexcept { return worked_; }
        [[nodiscard]] const PlanHandle& practice() const noexcept { return practice_; }
        [[nodiscard]] const std::vector<Alignment>& aligned() const noexcept { return aligned_; }
        [[nodiscard]] Usage usage() const noexcept { return usage_; }
    };
    using LessonHandle = std::shared_ptr<const Lesson>;
    // One immutable lesson per object, at most four compiler-derived aligned clauses; no content interpreter.
    [[nodiscard]] std::expected<LessonHandle, Problem> make_lesson(const LessonInput& input, const Catalog& catalog, Limits limits = {});
    [[nodiscard]] std::string_view error_name(Error error) noexcept;
}
