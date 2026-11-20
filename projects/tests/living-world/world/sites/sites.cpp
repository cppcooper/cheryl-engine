#include "sites.h"

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>

namespace CE::LivingWorld::Sites {
    namespace {
        constexpr Index max_cells = ProcGen::settlement_hard_limits.cells;
        constexpr Budget hard_budget{2 * 1024 * 1024, 8'000'000};
        Failure fail(Error reason) { return {reason, std::nullopt}; }
        Index sum(Index left, Index right) {
            return left > UINT64_MAX - right ? UINT64_MAX : left + right;
        }
        Index product(Index left, Index right) {
            return right != 0 && left > UINT64_MAX / right ? UINT64_MAX : left * right;
        }
        bool admitted(Accounting needed, Budget budget) {
            return needed.storage_bytes <= std::min(budget.storage_bytes, hard_budget.storage_bytes)
                && needed.work <= std::min(budget.work, hard_budget.work);
        }
        bool valid_scope(Scope scope) { return scope.world != 0 && scope.branch != 0; }
        bool valid_fence(Fence fence) { return fence.incarnation != 0 && fence.epoch != 0; }
        Result<void> validate(const Stamp& stamp) {
            if (!stamp.baseline) {
                return std::unexpected(fail(Error::invalid_baseline));
            }
            const auto& domain = stamp.baseline->domain();
            if (domain.dimensions() != 2 || stamp.frame.width != domain.extents()[0]
                || stamp.frame.height != domain.extents()[1] || stamp.frame.revision == 0) {
                return std::unexpected(fail(Error::invalid_frame));
            }
            if (!valid_scope(stamp.scope) || stamp.terrain == 0 || stamp.water == 0 || stamp.policy == 0) {
                return std::unexpected(fail(Error::invalid_stamp));
            }
            return {};
        }
        bool valid_fact(const ReachFact& fact) {
            if (fact.reach == 0) {
                return false;
            }
            switch (fact.use) {
                case WaterUse::usable: return fact.reason == Reason::supply_available;
                case WaterUse::not_usable: return fact.reason == Reason::no_supply || fact.reason == Reason::supply_lost;
                case WaterUse::unknown: return fact.reason == Reason::no_measurement;
            }
            return false;
        }
        bool valid_preserved(const Preserved& state, Index cells) {
            if (state.town_count > capacity || state.edit_count > capacity
                || state.debt_count > capacity || state.knowledge_count > capacity) {
                return false;
            }
            for (std::size_t index = 0; index < state.town_count; ++index) {
                const auto& town = state.towns[index];
                if (town.identity == 0 || town.cell >= cells || town.history == 0) {
                    return false;
                }
                for (std::size_t prior = 0; prior < index; ++prior) {
                    if (state.towns[prior].identity == town.identity) { return false; }
                }
            }
            for (std::size_t index = 0; index < state.edit_count; ++index) {
                const auto& edit = state.edits[index];
                if (edit.cell >= cells || edit.action == 0 || edit.provenance.view().empty()) { return false; }
                for (std::size_t prior = 0; prior < index; ++prior) {
                    if (state.edits[prior].cell == edit.cell) { return false; }
                }
            }
            for (std::size_t index = 0; index < state.debt_count; ++index) {
                const auto& debt = state.debts[index];
                if (debt.identity == 0 || debt.revision == 0) { return false; }
                for (std::size_t prior = 0; prior < index; ++prior) {
                    if (state.debts[prior].identity == debt.identity) { return false; }
                }
            }
            for (std::size_t index = 0; index < state.knowledge_count; ++index) {
                const auto& known = state.knowledge[index];
                if (known.npc == 0 || known.acquisition == 0 || known.evidence.view().empty()) { return false; }
                for (std::size_t prior = 0; prior < index; ++prior) {
                    if (state.knowledge[prior].npc == known.npc
                        && state.knowledge[prior].acquisition == known.acquisition) { return false; }
                }
            }
            return true;
        }
        Accounting operation_cost(Index cells, Index payload) {
            return {payload, sum(product(32, cells), sum(product(8, payload), 65'536))};
        }
    }

    Result<Text> Text::from(std::string_view value) {
        if (value.size() > 64 || !std::all_of(value.begin(), value.end(), [](char byte) {
                return byte >= 32 && byte <= 126;
            })) {
            return std::unexpected(fail(Error::invalid_facts));
        }
        Text text;
        text.size_ = value.size();
        std::copy(value.begin(), value.end(), text.bytes_.begin());
        return text;
    }

    Baseline::Baseline(Index identity, Index schema, ProcGen::Domain domain,
                       std::optional<ProcGen::ElevationReplay> recipe, std::span<const ProcGen::Height> heights)
    : identity_(identity), literal_schema_(schema), domain_(std::move(domain)), recipe_(std::move(recipe)),
      heights_(heights.begin(), heights.end()) {}

    Accounting Baseline::requirements(Index cells) {
        const auto bytes = sum(sizeof(Baseline), product(cells, sizeof(ProcGen::Height)));
        return {bytes, sum(cells, bytes)};
    }
    Result<std::shared_ptr<const Baseline>> Baseline::literal(Index identity, Index schema,
        const ProcGen::Domain& domain, std::span<const ProcGen::Height> heights, Budget budget) {
        if (identity == 0 || schema == 0 || domain.dimensions() != 2 || heights.size() != domain.cells()) {
            return std::unexpected(fail(Error::invalid_baseline));
        }
        if (domain.cells() > max_cells || !admitted(requirements(domain.cells()), budget)) {
            return std::unexpected(fail(Error::budget));
        }
        return std::shared_ptr<const Baseline>(new Baseline(identity, schema, domain, std::nullopt, heights));
    }
    Result<std::shared_ptr<const Baseline>> Baseline::generated(Index identity, const ProcGen::Elevation& elevation, Budget budget) {
        const auto& replay = elevation.descriptor();
        if (identity == 0 || replay.domain.dimensions() != 2 || elevation.heights().size() != replay.domain.cells()
            || replay.version != 1 || replay.algorithm != ProcGen::ElevationAlgorithm::lattice_bilinear) {
            return std::unexpected(fail(Error::invalid_baseline));
        }
        if (replay.domain.cells() > max_cells || !admitted(requirements(replay.domain.cells()), budget)) {
            return std::unexpected(fail(Error::budget));
        }
        return std::shared_ptr<const Baseline>(new Baseline(identity, 0, replay.domain, replay, elevation.heights()));
    }
    bool Baseline::matches(const ProcGen::Elevation& elevation) const {
        return recipe_ && *recipe_ == elevation.descriptor()
            && std::ranges::equal(heights_, elevation.heights());
    }
    bool Baseline::equivalent(const Baseline& other) const {
        return identity_ == other.identity_ && literal_schema_ == other.literal_schema_
            && domain_ == other.domain_ && recipe_ == other.recipe_ && heights_ == other.heights_;
    }
    bool Stamp::equivalent(const Stamp& other) const {
        return scope == other.scope && frame == other.frame && terrain == other.terrain
            && water == other.water && policy == other.policy && baseline && other.baseline
            && baseline->equivalent(*other.baseline);
    }

    Accounting WaterFacts::requirements() { return operation_cost(0, sizeof(WaterFacts)); }
    Result<WaterFacts> WaterFacts::make(const Stamp& stamp, std::span<const Binding> bindings,
                                      std::span<const ReachFact> facts, Budget budget) {
        if (const auto valid = validate(stamp); !valid) { return std::unexpected(valid.error()); }
        if (bindings.size() > capacity || facts.size() > capacity) {
            return std::unexpected(fail(Error::invalid_facts));
        }
        if (!admitted(requirements(), budget)) { return std::unexpected(fail(Error::budget)); }
        for (std::size_t index = 0; index < bindings.size(); ++index) {
            const auto& binding = bindings[index];
            if (binding.site.scope != stamp.scope || binding.site.frame != stamp.frame.revision
                || binding.site.cell >= stamp.baseline->domain().cells() || binding.intake == 0) {
                return std::unexpected(fail(Error::invalid_facts));
            }
            for (std::size_t prior = 0; prior < index; ++prior) {
                if (bindings[prior].site == binding.site || bindings[prior].intake == binding.intake) {
                    return std::unexpected(fail(Error::invalid_facts));
                }
            }
        }
        for (std::size_t index = 0; index < facts.size(); ++index) {
            if (!valid_fact(facts[index])) { return std::unexpected(fail(Error::invalid_facts)); }
            for (std::size_t prior = 0; prior < index; ++prior) {
                if (facts[prior].reach == facts[index].reach) { return std::unexpected(fail(Error::invalid_facts)); }
            }
        }
        WaterFacts water;
        water.stamp_ = stamp;
        water.binding_count_ = bindings.size();
        water.fact_count_ = facts.size();
        std::copy(bindings.begin(), bindings.end(), water.bindings_.begin());
        std::copy(facts.begin(), facts.end(), water.facts_.begin());
        std::ranges::sort(std::span(water.bindings_.data(), water.binding_count_), {}, [](const Binding& binding) {
            return binding.site.cell;
        });
        std::ranges::sort(std::span(water.facts_.data(), water.fact_count_), {}, &ReachFact::reach);
        return water;
    }
    bool WaterFacts::equivalent(const WaterFacts& other) const {
        return stamp_.equivalent(other.stamp_) && std::ranges::equal(bindings(), other.bindings())
            && std::ranges::equal(facts(), other.facts());
    }

    Accounting Batch::requirements(Index cells) {
        return operation_cost(cells, sizeof(Batch) + sizeof(ProcGen::SettlementSites) + sizeof(Assessment));
    }
    class AssessmentBuilder {
    public:
        static Result<Batch> run(const Stamp& stamp, const WaterFacts& water,
                                 const ProcGen::SettlementSites& selection, Budget budget);
    };
    Result<Batch> AssessmentBuilder::run(const Stamp& stamp, const WaterFacts& water,
                                       const ProcGen::SettlementSites& selection, Budget budget) {
        if (const auto valid = validate(stamp); !valid) { return std::unexpected(valid.error()); }
        if (!admitted(Batch::requirements(stamp.baseline->domain().cells()), budget)) {
            return std::unexpected(fail(Error::budget));
        }
        if (!stamp.equivalent(water.stamp())) { return std::unexpected(fail(Error::invalid_facts)); }
        Batch batch;
        batch.stamp_ = stamp;
        batch.options_ = selection.options();
        batch.selector_work_ = selection.work();
        batch.selector_storage_ = selection.storage_bytes();
        for (const auto& site : selection.sites()) {
            auto& assessment = batch.sites_[batch.count_++];
            assessment.terrain = site;
            assessment.key = {stamp.scope, stamp.frame.revision, site.index};
            const auto binding = std::ranges::find(water.bindings(), assessment.key, &Binding::site);
            if (binding == water.bindings().end()) { continue; }
            assessment.intake = binding->intake;
            assessment.reach = binding->reach;
            if (binding->reach == 0) { assessment.reason = Reason::unmapped; continue; }
            const auto fact = std::ranges::find(water.facts(), binding->reach, &ReachFact::reach);
            if (fact == water.facts().end()) { assessment.reason = Reason::missing_fact; continue; }
            assessment.reason = fact->reason;
            assessment.evidence = fact->evidence;
            if (fact->evidence.view().empty()) { assessment.reason = Reason::missing_evidence; continue; }
            switch (fact->use) {
                case WaterUse::usable: assessment.verdict = Verdict::suitable; break;
                case WaterUse::not_usable: assessment.verdict = Verdict::unsuitable; break;
                case WaterUse::unknown: break;
            }
        }
        return batch;
    }
    Index Batch::count(Verdict verdict) const {
        return static_cast<Index>(std::ranges::count(sites(), verdict, &Assessment::verdict));
    }
    ShortlistOutcome Batch::outcome() const {
        if (count(Verdict::suitable) != 0) { return ShortlistOutcome::has_suitable; }
        if (count(Verdict::unknown) != 0) { return ShortlistOutcome::incomplete_evidence; }
        return ShortlistOutcome::no_eligible_in_shortlist;
    }
    Result<Batch> select_and_assess(const Stamp& stamp, const WaterFacts& water,
        ProcGen::SettlementOptions options, ProcGen::CaveBudgets producer_budget, Budget budget) {
        if (const auto valid = validate(stamp); !valid) { return std::unexpected(valid.error()); }
        if (!admitted(Batch::requirements(stamp.baseline->domain().cells()), budget)) {
            return std::unexpected(fail(Error::budget));
        }
        const auto selection = ProcGen::select_settlement_sites(stamp.baseline->domain(), stamp.baseline->heights(), options, producer_budget);
        if (!selection) { return std::unexpected(Failure{Error::producer_failure, selection.error()}); }
        return AssessmentBuilder::run(stamp, water, *selection, budget);
    }
    Result<Batch> select_and_assess(const ProcGen::Elevation& elevation, const Stamp& stamp,
        const WaterFacts& water, ProcGen::SettlementOptions options, ProcGen::CaveBudgets producer_budget, Budget budget) {
        if (const auto valid = validate(stamp); !valid) { return std::unexpected(valid.error()); }
        if (!admitted(Batch::requirements(stamp.baseline->domain().cells()), budget)) {
            return std::unexpected(fail(Error::budget));
        }
        if (!stamp.baseline->matches(elevation)) { return std::unexpected(fail(Error::invalid_baseline)); }
        const auto selection = ProcGen::select_settlement_sites(elevation, options, producer_budget);
        if (!selection) { return std::unexpected(Failure{Error::producer_failure, selection.error()}); }
        return AssessmentBuilder::run(stamp, water, *selection, budget);
    }

    Candidate::Candidate(Attempt attempt, const Image& image, WaterFacts replacement)
    : attempt_(attempt), expected_(image.water.stamp()), expected_edits_(image.edits_revision),
      fence_(image.fence), replacement_(std::move(replacement)) {}

    Accounting Candidate::requirements(Index cells) {
        return operation_cost(cells, sizeof(Candidate) + sizeof(WaterFacts));
    }
    Result<Candidate> Candidate::prepare(const Image& image, Attempt attempt,
        std::span<const Binding> bindings, std::span<const ReachFact> facts, Budget budget) {
        const auto& current = image.water.stamp();
        if (const auto valid = validate(current); !valid) { return std::unexpected(valid.error()); }
        if (!admitted(requirements(current.baseline->domain().cells()), budget)) {
            return std::unexpected(fail(Error::budget));
        }
        if (attempt.scope != current.scope || attempt.issuer == 0 || attempt.sequence == 0) {
            return std::unexpected(fail(Error::denied));
        }
        if (!valid_fence(image.fence)) { return std::unexpected(fail(Error::fence)); }
        if (image.edits_revision == 0) { return std::unexpected(fail(Error::invalid_preserved)); }
        if (current.water == UINT64_MAX) { return std::unexpected(fail(Error::revision_exhausted)); }
        auto next = current;
        ++next.water;
        auto replacement = WaterFacts::make(next, bindings, facts, budget);
        if (!replacement) { return std::unexpected(replacement.error()); }
        return Candidate(attempt, image, std::move(*replacement));
    }
    bool Candidate::same_body(const Candidate& other) const {
        return attempt_ == other.attempt_ && expected_.equivalent(other.expected_)
            && expected_edits_ == other.expected_edits_ && replacement_.equivalent(other.replacement_);
    }

    Owner::Owner(WaterFacts water, Preserved preserved, Index edits_revision, Fence fence)
    : water_(std::move(water)), preserved_(std::move(preserved)), edits_revision_(edits_revision), fence_(fence) {}

    Accounting Owner::requirements(Index cells) {
        return operation_cost(cells, sizeof(Owner) + sizeof(Preserved) + sizeof(Candidate) + sizeof(WaterFacts));
    }
    Result<std::unique_ptr<Owner>> Owner::create(WaterFacts water, Preserved preserved,
        Index edits_revision, Fence fence, Budget budget) {
        if (const auto valid = validate(water.stamp()); !valid) { return std::unexpected(valid.error()); }
        const auto cells = water.stamp().baseline->domain().cells();
        if (!admitted(requirements(cells), budget)) { return std::unexpected(fail(Error::budget)); }
        if (!valid_fence(fence)) { return std::unexpected(fail(Error::fence)); }
        if (edits_revision == 0 || !valid_preserved(preserved, cells)) {
            return std::unexpected(fail(Error::invalid_preserved));
        }
        return std::unique_ptr<Owner>(new Owner(std::move(water), std::move(preserved), edits_revision, fence));
    }
    bool Owner::authorized(const Access& access) const { return access.owner_ == this && access.fence_ == fence_; }
    Result<Image> Owner::capture(const Access& access) const {
        if (!authorized(access)) { return std::unexpected(fail(Error::denied)); }
        return Image{water_, edits_revision_, fence_};
    }
    Result<Preserved> Owner::preserved(const Access& access) const {
        if (!authorized(access)) { return std::unexpected(fail(Error::denied)); }
        return preserved_;
    }
    Result<Batch> Owner::read(const Access& access, const Batch& batch, Budget budget) const {
        if (!authorized(access) || batch.stamp().scope != water_.stamp().scope) {
            return std::unexpected(fail(Error::denied));
        }
        if (!admitted(Batch::requirements(water_.stamp().baseline->domain().cells()), budget)) {
            return std::unexpected(fail(Error::budget));
        }
        if (!batch.stamp().equivalent(water_.stamp())) { return std::unexpected(fail(Error::stale)); }
        return batch;
    }
    Result<std::optional<Receipt>> Owner::lookup(const Access& access, const Candidate& candidate, Budget budget) const {
        if (!authorized(access) || candidate.attempt_.scope != water_.stamp().scope) {
            return std::unexpected(fail(Error::denied));
        }
        if (!admitted(requirements(water_.stamp().baseline->domain().cells()), budget)) {
            return std::unexpected(fail(Error::budget));
        }
        for (std::size_t index = 0; index < applied_count_; ++index) {
            const auto& applied = *applied_[index];
            if (applied.receipt.attempt == candidate.attempt_) {
                if (!applied.body.same_body(candidate)) { return std::unexpected(fail(Error::body_conflict)); }
                return std::optional<Receipt>(applied.receipt);
            }
        }
        return std::optional<Receipt>{};
    }
    Result<Adoption> Owner::adopt(const Access& access, const Candidate& candidate, Budget budget) {
        const auto prior = lookup(access, candidate, budget);
        if (!prior) { return std::unexpected(prior.error()); }
        if (*prior) { return Adoption{**prior, true}; }
        if (candidate.fence_ != fence_) { return std::unexpected(fail(Error::fence)); }
        if (!candidate.expected_.equivalent(water_.stamp())) { return std::unexpected(fail(Error::stale)); }
        if (candidate.expected_edits_ != edits_revision_) { return std::unexpected(fail(Error::edits_conflict)); }
        if (applied_count_ == receipt_capacity) { return std::unexpected(fail(Error::receipt_full)); }
        if (water_.stamp().water == UINT64_MAX) { return std::unexpected(fail(Error::revision_exhausted)); }
        auto expected_next = water_.stamp();
        ++expected_next.water;
        if (!candidate.replacement_.stamp().equivalent(expected_next)) {
            return std::unexpected(fail(Error::invalid_facts));
        }
        const auto before = preserved_;
        auto replacement = candidate.replacement_;
        Applied staged{candidate, Receipt{candidate.attempt_, expected_next.water}};
        if (!valid_preserved(preserved_, water_.stamp().baseline->domain().cells()) || preserved_ != before) {
            return std::unexpected(fail(Error::invalid_preserved));
        }
        static_assert(std::is_nothrow_move_assignable_v<WaterFacts>);
        static_assert(std::is_nothrow_move_constructible_v<Applied>);
        water_ = std::move(replacement);
        applied_[applied_count_++].emplace(std::move(staged));
        return Adoption{applied_[applied_count_ - 1]->receipt, false};
    }
    Result<Index> Owner::excavate(const Access& access, Index expected_revision, Edit edit) {
        if (!authorized(access)) { return std::unexpected(fail(Error::denied)); }
        if (expected_revision != edits_revision_) { return std::unexpected(fail(Error::edits_conflict)); }
        if (edits_revision_ == UINT64_MAX) { return std::unexpected(fail(Error::revision_exhausted)); }
        if (preserved_.edit_count == capacity || edit.cell >= water_.stamp().baseline->domain().cells()
            || edit.action == 0 || edit.provenance.view().empty()) {
            return std::unexpected(fail(Error::invalid_preserved));
        }
        for (std::size_t index = 0; index < preserved_.edit_count; ++index) {
            if (preserved_.edits[index].cell == edit.cell) { return std::unexpected(fail(Error::invalid_preserved)); }
        }
        preserved_.edits[preserved_.edit_count++] = std::move(edit);
        return ++edits_revision_;
    }
    Result<void> Owner::renew_fence(const Access& access, Fence next) {
        if (!authorized(access)) { return std::unexpected(fail(Error::denied)); }
        if (!valid_fence(next) || next.incarnation != fence_.incarnation || next.epoch <= fence_.epoch) {
            return std::unexpected(fail(Error::fence));
        }
        fence_ = next;
        return {};
    }
}
