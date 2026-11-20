#pragma once

#include "settlement-sites.h"

#include <memory>
#include <utility>

namespace CE::LivingWorld::Sites {
    using ProcGen::Index;
    inline constexpr std::size_t capacity = 16;
    inline constexpr std::size_t receipt_capacity = 8;

    enum class Error {
        budget, invalid_baseline, invalid_frame, invalid_stamp, invalid_facts,
        invalid_preserved, denied, stale, fence, edits_conflict, body_conflict,
        receipt_full, revision_exhausted, producer_failure
    };
    struct Failure {
        Error reason;
        std::optional<ProcGen::SettlementFailure> producer;
        bool operator==(const Failure&) const = default;
    };
    template <class Value> using Result = std::expected<Value, Failure>;
    struct Budget {
        Index storage_bytes = 2 * 1024 * 1024;
        Index work = 2'000'000;
    };
    struct Accounting {
        Index storage_bytes;
        Index work;
    };
    struct Scope {
        Index world = 0, branch = 0;
        bool operator==(const Scope&) const = default;
    };
    struct Fence {
        Index incarnation = 0, epoch = 0;
        bool operator==(const Fence&) const = default;
    };
    class Text {
        std::array<char, 64> bytes_{};
        std::size_t size_ = 0;
    public:
        [[nodiscard]] static Result<Text> from(std::string_view value);
        [[nodiscard]] std::string_view view() const { return {bytes_.data(), size_}; }
        bool operator==(const Text&) const = default;
    };
    struct Frame {
        Index revision = 0, width = 0, height = 0;
        std::int64_t origin_x = 0, origin_y = 0;
        bool operator==(const Frame&) const = default;
    };
    class Baseline {
        Index identity_, literal_schema_;
        ProcGen::Domain domain_;
        std::optional<ProcGen::ElevationReplay> recipe_;
        std::vector<ProcGen::Height> heights_;
        Baseline(Index identity, Index schema, ProcGen::Domain domain,
                 std::optional<ProcGen::ElevationReplay> recipe, std::span<const ProcGen::Height> heights);
    public:
        Baseline(const Baseline&) = delete;
        Baseline& operator=(const Baseline&) = delete;
        [[nodiscard]] static Result<std::shared_ptr<const Baseline>> literal(
            Index identity, Index schema, const ProcGen::Domain& domain,
            std::span<const ProcGen::Height> heights, Budget budget = {});
        [[nodiscard]] static Result<std::shared_ptr<const Baseline>> generated(
            Index identity, const ProcGen::Elevation& elevation, Budget budget = {});
        [[nodiscard]] const ProcGen::Domain& domain() const { return domain_; }
        [[nodiscard]] Index identity() const { return identity_; }
        [[nodiscard]] std::span<const ProcGen::Height> heights() const { return heights_; }
        [[nodiscard]] bool matches(const ProcGen::Elevation& elevation) const;
        [[nodiscard]] bool equivalent(const Baseline& other) const;
        [[nodiscard]] static Accounting requirements(Index cells);
    };
    struct Stamp {
        Scope scope;
        std::shared_ptr<const Baseline> baseline;
        Frame frame;
        Index terrain = 0, water = 0, policy = 0;
        [[nodiscard]] bool equivalent(const Stamp& other) const;
    };
    struct SiteKey {
        Scope scope;
        Index baseline = 0, frame = 0, cell = 0;
        bool operator==(const SiteKey&) const = default;
    };
    enum class WaterUse { usable, not_usable, unknown };
    enum class Reason { supply_available, no_supply, supply_lost, no_measurement, missing_binding, unmapped, missing_fact, missing_evidence };
    struct Binding {
        SiteKey site;
        Index intake = 0, reach = 0;
        bool operator==(const Binding&) const = default;
    };
    struct ReachFact {
        Index reach = 0;
        WaterUse use = WaterUse::unknown;
        Reason reason = Reason::no_measurement;
        Text evidence;
        bool operator==(const ReachFact&) const = default;
    };
    class WaterFacts {
        Stamp stamp_;
        std::array<Binding, capacity> bindings_{};
        std::array<ReachFact, capacity> facts_{};
        std::size_t binding_count_ = 0, fact_count_ = 0;
        WaterFacts() = default;
    public:
        [[nodiscard]] static Result<WaterFacts> make(
            const Stamp& stamp, std::span<const Binding> bindings, std::span<const ReachFact> facts, Budget budget = {});
        [[nodiscard]] const Stamp& stamp() const { return stamp_; }
        [[nodiscard]] std::span<const Binding> bindings() const { return {bindings_.data(), binding_count_}; }
        [[nodiscard]] std::span<const ReachFact> facts() const { return {facts_.data(), fact_count_}; }
        [[nodiscard]] bool equivalent(const WaterFacts& other) const;
        [[nodiscard]] static Accounting requirements();
    };
    enum class Verdict { suitable, unsuitable, unknown };
    struct Assessment {
        ProcGen::SettlementSite terrain;
        SiteKey key;
        Verdict verdict = Verdict::unknown;
        Reason reason = Reason::missing_binding;
        Index intake = 0, reach = 0;
        Text evidence;
    };
    enum class ShortlistOutcome { has_suitable, no_eligible_in_shortlist, incomplete_evidence };
    class Batch {
        Stamp stamp_;
        std::optional<WaterFacts> input_water_;
        std::array<Assessment, capacity> sites_{};
        std::size_t count_ = 0;
        ProcGen::SettlementOptions options_;
        Index selector_work_ = 0, selector_storage_ = 0;
        friend class AssessmentBuilder;
        friend class Owner;
    public:
        [[nodiscard]] const Stamp& stamp() const { return stamp_; }
        [[nodiscard]] std::span<const Assessment> sites() const { return {sites_.data(), count_}; }
        [[nodiscard]] Index requested() const { return options_.requested; }
        [[nodiscard]] Index achieved() const { return static_cast<Index>(count_); }
        [[nodiscard]] Index shortfall() const { return requested() - achieved(); }
        [[nodiscard]] Index selector_work() const { return selector_work_; }
        [[nodiscard]] Index selector_storage() const { return selector_storage_; }
        [[nodiscard]] ProcGen::SettlementOptions options() const { return options_; }
        [[nodiscard]] Index count(Verdict verdict) const;
        [[nodiscard]] ShortlistOutcome outcome() const;
        [[nodiscard]] static constexpr bool water_search_complete() { return false; }
        [[nodiscard]] static Accounting requirements(Index cells);
    };
    [[nodiscard]] Result<Batch> select_and_assess(const Stamp& stamp, const WaterFacts& water,
        ProcGen::SettlementOptions options, ProcGen::CaveBudgets producer_budget, Budget budget = {});
    [[nodiscard]] Result<Batch> select_and_assess(const ProcGen::Domain& domain,
        std::span<const ProcGen::Height> heights, const Stamp& stamp, const WaterFacts& water,
        ProcGen::SettlementOptions options, ProcGen::CaveBudgets producer_budget, Budget budget = {});
    [[nodiscard]] Result<Batch> select_and_assess(const ProcGen::Elevation& elevation, const Stamp& stamp,
        const WaterFacts& water, ProcGen::SettlementOptions options, ProcGen::CaveBudgets producer_budget, Budget budget = {});

    struct Town {
        Index identity = 0, cell = 0, history = 0;
        bool occupied = false;
        bool operator==(const Town&) const = default;
    };
    struct Edit {
        Index cell = 0, action = 0;
        bool excavated = false;
        Text provenance;
        bool operator==(const Edit&) const = default;
    };
    struct Debt {
        Index identity = 0, amount = 0, revision = 0;
        bool operator==(const Debt&) const = default;
    };
    struct Knowledge {
        Index npc = 0, acquisition = 0;
        Text evidence;
        bool operator==(const Knowledge&) const = default;
    };
    struct Preserved {
        std::array<Town, capacity> towns{};
        std::array<Edit, capacity> edits{};
        std::array<Debt, capacity> debts{};
        std::array<Knowledge, capacity> knowledge{};
        std::size_t town_count = 0, edit_count = 0, debt_count = 0, knowledge_count = 0;
        bool operator==(const Preserved&) const = default;
    };
    struct Attempt {
        Scope scope;
        Index issuer = 0, sequence = 0;
        bool operator==(const Attempt&) const = default;
    };
    class OwnerIdentity {
        OwnerIdentity() = default;
        friend class Owner;
    };
    class Image {
    public:
        WaterFacts water;
        Index edits_revision;
        Fence fence;
    private:
        std::shared_ptr<const OwnerIdentity> owner_;
        Image(WaterFacts facts, Index edits, Fence live_fence, std::shared_ptr<const OwnerIdentity> owner)
        : water(std::move(facts)), edits_revision(edits), fence(live_fence), owner_(std::move(owner)) {}
        friend class Owner;
        friend class Candidate;
    };
    class Candidate {
        Attempt attempt_;
        WaterFacts expected_water_;
        Index expected_edits_;
        Fence fence_;
        std::shared_ptr<const OwnerIdentity> owner_;
        WaterFacts replacement_;
        Candidate(Attempt attempt, const Image& image, WaterFacts replacement);
        friend class Owner;
    public:
        [[nodiscard]] static Result<Candidate> prepare(const Image& image, Attempt attempt,
            std::span<const Binding> bindings, std::span<const ReachFact> facts, Budget budget = {});
        [[nodiscard]] bool same_body(const Candidate& other) const;
        [[nodiscard]] static Accounting requirements(Index cells);
    };
    struct Receipt {
        Attempt attempt;
        Index water_revision;
        bool operator==(const Receipt&) const = default;
    };
    struct Adoption {
        Receipt receipt;
        bool existing;
    };
    class Owner {
    public:
        class Access {
            std::shared_ptr<const OwnerIdentity> owner_;
            Fence fence_;
            Access(std::shared_ptr<const OwnerIdentity> owner, Fence fence) : owner_(std::move(owner)), fence_(fence) {}
            friend class Owner;
        };
    private:
        struct Applied {
            Candidate body;
            Receipt receipt;
        };
        WaterFacts water_;
        Preserved preserved_;
        Index edits_revision_;
        Fence fence_;
        std::shared_ptr<const OwnerIdentity> identity_;
        std::array<std::optional<Applied>, receipt_capacity> applied_{};
        std::size_t applied_count_ = 0;
        Owner(WaterFacts water, Preserved preserved, Index edits_revision, Fence fence,
              std::shared_ptr<const OwnerIdentity> identity);
        [[nodiscard]] bool authorized(const Access& access) const;
    public:
        Owner(const Owner&) = delete;
        Owner& operator=(const Owner&) = delete;
        [[nodiscard]] static Result<std::unique_ptr<Owner>> create(WaterFacts water, Preserved preserved,
            Index edits_revision, Fence fence, Budget budget = {});
        [[nodiscard]] Access access() const { return Access(identity_, fence_); }
        [[nodiscard]] Result<Image> capture(const Access& access) const;
        [[nodiscard]] Result<Preserved> preserved(const Access& access) const;
        [[nodiscard]] Result<Batch> read(const Access& access, const Batch& batch, Budget budget = {}) const;
        [[nodiscard]] Result<Adoption> adopt(const Access& access, const Candidate& candidate, Budget budget = {});
        [[nodiscard]] Result<std::optional<Receipt>> lookup(const Access& access, const Candidate& candidate, Budget budget = {}) const;
        [[nodiscard]] Result<Index> excavate(const Access& access, Index expected_revision, Edit edit);
        [[nodiscard]] Result<void> renew_fence(const Access& access, Fence next);
        [[nodiscard]] static Accounting requirements(Index cells);
    };
}
