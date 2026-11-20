#pragma once

#include "sites.h"

namespace CE::LivingWorld::Habitat {
    using ProcGen::Index;
    enum class Error { budget, evidence, capacity, producer, owner, stale, foreign, revision };
    struct Failure {
        Error reason;
        std::optional<ProcGen::SettlementFailure> producer;
        std::optional<Sites::Failure> owner;
    };
    template <class Value> using Result = std::expected<Value, Failure>;
    inline constexpr Sites::Budget hard_limits{8 * 1024 * 1024, 20'000'000};
    struct Seed {
        Sites::Text name_space;
        Index value = 0, version = 1;
        bool operator==(const Seed&) const = default;
    };
    class Eligibility {
        std::shared_ptr<Sites::Owner> owner_;
        Sites::Image read_;
        ProcGen::EligibilityImage image_;
        Index revision_, literal_schema_;
        Seed seed_;
        std::optional<ProcGen::ElevationReplay> replay_;
        Eligibility(std::shared_ptr<Sites::Owner> owner, Sites::Image read, ProcGen::EligibilityImage image, Index revision,
            Index schema, Seed seed, std::optional<ProcGen::ElevationReplay> replay);
        [[nodiscard]] static Result<void> validate(const Sites::Stamp& stamp,
            const ProcGen::EligibilityImage& image, Index revision, Seed seed, Sites::Budget budget);
    public:
        [[nodiscard]] static Sites::Accounting requirements(Index cells);
        [[nodiscard]] static Result<std::shared_ptr<const Eligibility>> literal(std::shared_ptr<Sites::Owner> owner,
            const Sites::Owner::Access& access,
            Index schema, const ProcGen::EligibilityImage& image, Index revision, Seed seed, Sites::Budget budget = hard_limits);
        [[nodiscard]] static Result<std::shared_ptr<const Eligibility>> generated(std::shared_ptr<Sites::Owner> owner,
            const Sites::Owner::Access& access,
            const ProcGen::Elevation& elevation, const ProcGen::EligibilityImage& image,
            Index revision, Seed seed, Sites::Budget budget = hard_limits);
        [[nodiscard]] const Sites::Stamp& stamp() const { return read_.water.stamp(); }
        [[nodiscard]] const Sites::Image& read_set() const { return read_; }
        [[nodiscard]] bool belongs_to(const std::shared_ptr<Sites::Owner>& owner) const { return owner_ == owner; }
        [[nodiscard]] const ProcGen::EligibilityImage& image() const { return image_; }
        [[nodiscard]] Index revision() const { return revision_; }
        [[nodiscard]] Index literal_schema() const { return literal_schema_; }
        [[nodiscard]] const Seed& seed() const { return seed_; }
        [[nodiscard]] const std::optional<ProcGen::ElevationReplay>& replay() const { return replay_; }
    };
}
