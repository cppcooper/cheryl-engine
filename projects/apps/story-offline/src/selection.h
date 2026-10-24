#pragma once

#include "bargain.h"

namespace OfflineStory {
    // This finite manual family has two goals and one steward role at each actual location.
    enum class ReliefGoal { Access, Shelter };
    struct PropositionGoal {
        std::string proposition;
        ReliefGoal goal;
    };
    struct GoalPermission {
        std::string id;
        unsigned goals = 3;
    }; // bit 0 access, bit 1 shelter; host-owned.
    struct BargainLibrary {
        BargainSets sets;
        std::vector<PropositionGoal> goals;
        std::vector<ReliefBundle> bundles{ReliefBundle::OpenOffer, ReliefBundle::ExplainFirst};
        std::vector<std::size_t> flavours{0, 1};
    };
    struct BargainCatalogue {
        ReliefCatalogue host;
        std::vector<GoalPermission> roles, plans;
    };
    struct SelectionConstraints {
        unsigned required_initial = 0; // ReliefIntent bits, availability as well as presence.
        std::size_t minimum_initial = 2, nodes = 8, edges = 16;
        int max_material = 10, max_coins = 10, max_pending = 10;
    };
    struct SelectionLimits {
        std::size_t candidates = 128, draws = 1, text_bytes = 128 * 1024, storage_bytes = 1024 * 1024, work = 4 * 1024 * 1024;
        BargainLimits composition;
    };
    inline constexpr SelectionLimits maximum_selection_limits{};
    enum class SelectionCode {
        InvalidLimits,
        InvalidConstraints,
        InvalidLibrary,
        Composition,
        Empty,
        CandidateLimit,
        DrawLimit,
        TextLimit,
        StorageLimit,
        WorkLimit
    };
    struct SelectionError {
        SelectionCode code;
        std::string field;
        std::optional<Error> detail;
    };
    struct SelectionDescriptor {
        std::uint32_t algorithm;
        std::uint64_t seed, sets_revision, catalogue_revision;
        std::string sets, catalogue, request, proposition, actor, plan;
        ReliefGoal goal;
        ReliefBundle bundle;
        std::size_t flavour, examined, compatible;
        bool operator==(const SelectionDescriptor&) const = default;
    };
    // Ordinal beats are the eight fixed recipe beats; IDs/wording are deliberately excluded.
    struct MechanicalEdge {
        std::size_t from, to;
        int intent; // -1 is the fixed close edge; otherwise ReliefIntent.
        bool condition, effect;
        bool operator==(const MechanicalEdge&) const = default;
    };
    struct BargainMechanics {
        ReliefLocation location;
        int material_cost, coin_cost, ordinary_due, negotiated_due;
        // These typed consequences accompany the finite family's action/condition mapping.
        std::array<ReliefResolution, 7> resolutions;
        std::array<ReliefKnowledge, 7> knowledge;
        std::array<int, 7> attitudes;
        std::vector<MechanicalEdge> edges;
        bool operator==(const BargainMechanics&) const = default;
    };
    struct BargainComparison {
        BargainMechanics mechanics;
        std::vector<std::string> presentation; // Owns definition ID, node/choice IDs and all displayed text.
    };
    BargainComparison compare_bargain(const BargainArtifact&); // Fixed eight-beat family only; programming/allocation errors propagate.
    class SelectedBargain;
    using SelectedBargainHandle = std::shared_ptr<const SelectedBargain>;
    std::expected<SelectedBargainHandle, SelectionError> select_bargain(
        const BargainLibrary&,
        const BargainCatalogue&,
        const BargainContext&,
        std::uint64_t seed,
        std::uint32_t algorithm,
        const SelectionConstraints& = {},
        const SelectionLimits& = {}
    );
    class SelectedBargain final {
        BargainHandle content_;
        SelectionDescriptor descriptor_;
        Usage usage_;
        SelectedBargain(BargainHandle c, SelectionDescriptor d, Usage u)
        : content_(std::move(c)), descriptor_(std::move(d)), usage_(u) {}
        friend std::expected<SelectedBargainHandle, SelectionError> select_bargain(
            const BargainLibrary&,
            const BargainCatalogue&,
            const BargainContext&,
            std::uint64_t,
            std::uint32_t,
            const SelectionConstraints&,
            const SelectionLimits&
        );
    public:
        SelectedBargain(const SelectedBargain&) = delete;
        const BargainHandle& content() const noexcept { return content_; }
        const SelectionDescriptor& descriptor() const noexcept { return descriptor_; }
        Usage usage() const noexcept { return usage_; }
    };
    BargainLibrary selection_library();
    BargainCatalogue selection_catalogue();
    int run_selected(std::ostream&, std::uint64_t, std::string_view profile, std::string_view route);
}
