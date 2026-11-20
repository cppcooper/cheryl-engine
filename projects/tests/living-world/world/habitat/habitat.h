#pragma once

#include "eligibility.h"

namespace CE::LivingWorld::Habitat {
    enum class Unit { resident_slots, activity_slots };
    struct Capacity {
        Unit unit = Unit::resident_slots;
        Index slots = 0;
        bool operator==(const Capacity&) const = default;
    };
    struct Policy {
        Index version = 1, capacity_version = 1;
        Capacity residential{Unit::resident_slots, 4}, activity{Unit::activity_slots, 2};
        Sites::Text opportunity;
        bool operator==(const Policy&) const = default;
    };
    struct Place {
        Sites::SiteKey key;
        ProcGen::SettlementSite terrain;
        Capacity residential, activity;
        Sites::Text opportunity;
    };
    class AdviceIdentity;
    class HabitatPlan {
        Sites::Image read_;
        std::shared_ptr<const Eligibility> evidence_;
        std::shared_ptr<const AdviceIdentity> advisor_;
        ProcGen::EligibleSettlementSites selected_;
        Policy policy_;
        std::array<Place, ProcGen::settlement_capacity> places_{};
        Sites::Accounting accounting_;
        HabitatPlan(Sites::Image read, std::shared_ptr<const Eligibility> evidence,
            std::shared_ptr<const AdviceIdentity> advisor, ProcGen::EligibleSettlementSites selected,
            Policy policy, Sites::Accounting accounting);
        friend class Advisor;
    public:
        [[nodiscard]] std::span<const Place> places() const { return {places_.data(), static_cast<std::size_t>(selected_.achieved())}; }
        [[nodiscard]] const Sites::Stamp& stamp() const { return read_.water.stamp(); }
        [[nodiscard]] const Eligibility& evidence() const { return *evidence_; }
        [[nodiscard]] Index edits_revision() const { return read_.edits_revision; }
        [[nodiscard]] Sites::Fence fence() const { return read_.fence; }
        [[nodiscard]] const ProcGen::EligibleSettlementSites& selection() const { return selected_; }
        [[nodiscard]] Policy policy() const { return policy_; }
        [[nodiscard]] Sites::Accounting accounting() const { return accounting_; }
        [[nodiscard]] Index resident_slots() const { return selected_.achieved() * policy_.residential.slots; }
        [[nodiscard]] Index activity_slots() const { return selected_.achieved() * policy_.activity.slots; }
    };
    class Advisor {
        std::shared_ptr<Sites::Owner> owner_;
        Sites::Owner::Access access_;
        std::shared_ptr<const Eligibility> evidence_;
        std::shared_ptr<const AdviceIdentity> identity_;
        Advisor(std::shared_ptr<Sites::Owner> owner, Sites::Owner::Access access,
            std::shared_ptr<const Eligibility> evidence, std::shared_ptr<const AdviceIdentity> identity);
        [[nodiscard]] Result<Sites::Image> current(Sites::Budget budget) const;
    public:
        Advisor(const Advisor&) = delete;
        Advisor& operator=(const Advisor&) = delete;
        [[nodiscard]] static Result<std::unique_ptr<Advisor>> create(std::shared_ptr<Sites::Owner> owner,
            Sites::Owner::Access access, std::shared_ptr<const Eligibility> evidence, Sites::Budget budget = hard_limits);
        [[nodiscard]] static Result<Sites::Accounting> requirements(const ProcGen::Domain& domain, ProcGen::SettlementOptions options);
        [[nodiscard]] Result<HabitatPlan> prepare(Policy policy, ProcGen::SettlementOptions options,
            ProcGen::CaveBudgets producer_budget = ProcGen::eligibility_hard_limits, Sites::Budget budget = hard_limits) const;
        [[nodiscard]] Result<HabitatPlan> read(const HabitatPlan& plan, Sites::Budget budget = hard_limits) const;
        [[nodiscard]] Result<void> replace_eligibility(std::shared_ptr<const Eligibility> evidence, Sites::Budget budget = hard_limits);
    };
}
