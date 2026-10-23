#pragma once

#include "traversal.h"

namespace ProcGen {
    inline constexpr std::uint32_t traversal_scale = 65536;
    inline constexpr Budgets configured_hard_limits{1'000'000, 999'999, 64 * 1024 * 1024, 100'000'000};
    inline constexpr Index configured_fixed_allowance = 8192;

    struct Endpoints {
        Index entrance;
        Index goal;
        bool operator==(const Endpoints&) const = default;
    };
    enum class Coverage { full };

    // Binary64 [0,1], normalized to ticks/65536, nearest with half upward.
    // For C>=2, route cells L=2+floor(ticks*(C-2)/65536); C=1 uses L=1.
    // All C cells are active; exactly C-L lie off the selected route. More
    // ticks never increase that count, but change auto goal/route length. This
    // profile does not promise monotonic branch count/depth or player difficulty.
    // nullopt selects auto endpoints; fixed accepts the seeded prefix pair in
    // either orientation. Coverage other than full is explicitly unsupported.
    struct Configuration {
        double traversability = 1.0;
        std::optional<Endpoints> endpoints = std::nullopt;
        Coverage coverage = Coverage::full;
        bool operator==(const Configuration&) const = default;
    };
    struct EffectiveConfiguration {
        std::uint32_t ticks;
        Index route_cells;
        bool operator==(const EffectiveConfiguration&) const = default;
    };
    enum class ConfiguredAlgorithm : std::uint32_t { serpentine_tree = 1 };
    struct ConfiguredReplay {
        Domain domain;
        StreamKey random;
        Budgets budgets;
        Configuration requested;
        ConfiguredAlgorithm algorithm = ConfiguredAlgorithm::serpentine_tree;
        std::uint32_t version = 1;
        bool operator==(const ConfiguredReplay&) const = default;
    };
    struct ConfiguredRequirements {
        EffectiveConfiguration effective;
        Requirements resources;
    };

    // Resource estimate and necessary endpoint checks, not proof that a fixed pair
    // matches the seeded construction (the key is only supplied at generation).
    // Preflight lower work bound; runtime rejection draws and direction scans
    // can require more. Peak includes all buffers and U2 metric scratch, not RSS.
    [[nodiscard]] Result<ConfiguredRequirements> configured_requirements(const Domain& domain, Configuration requested);

    // Complete owned result; const views expire on destruction/move/assignment.
    // Copies deep-copy both buffers; moved-from values only destroy/assign.
    class ConfiguredMaze {
        ConfiguredReplay replay_;
        EffectiveConfiguration effective_;
        Endpoints endpoints_;
        std::vector<std::uint32_t> passages_;
        std::vector<Index> route_;
        Measurements measured_;
        Usage usage_;

        ConfiguredMaze(
            ConfiguredReplay replay,
            EffectiveConfiguration effective,
            Endpoints endpoints,
            std::vector<std::uint32_t> passages,
            std::vector<Index> route,
            Measurements measured,
            Usage usage
        );
        friend Result<ConfiguredMaze> replay_configured(const ConfiguredReplay& descriptor);

    public:
        [[nodiscard]] const ConfiguredReplay& descriptor() const { return replay_; }
        [[nodiscard]] const Domain& domain() const { return replay_.domain; }
        [[nodiscard]] EffectiveConfiguration effective() const { return effective_; }
        [[nodiscard]] Endpoints endpoints() const { return endpoints_; }
        [[nodiscard]] std::span<const std::uint32_t> passages() const { return passages_; }
        [[nodiscard]] std::span<const Index> main_route() const { return route_; }
        [[nodiscard]] const Measurements& measurements() const { return measured_; }
        [[nodiscard]] Usage usage() const { return usage_; }
    };

    [[nodiscard]] MazeView maze_view(const ConfiguredMaze& maze);
    // V1 preserves integer ordering, normalization and stage stream semantics.
    // Budgets affect admission only. Errors return no topology/path; allocation
    // and programming exceptions propagate. No prior result is mutated.
    [[nodiscard]] Result<ConfiguredMaze> replay_configured(const ConfiguredReplay& descriptor);
    [[nodiscard]] Result<ConfiguredMaze>
    generate_configured(std::span<const Index> extents, StreamKey random, Configuration requested, Budgets budgets);
}
