#pragma once

#include <story/scene/scene.h>

#include <utility>

namespace CE::LivingWorld {
    class PairedCut final {
        struct Data {
            std::uint32_t schema = 1;
            std::shared_ptr<const void> world_cut, scene_cut, lineage;
            WorldScope scene_scope;
            std::uint64_t baseline = 0, issued = 0;
            Detail::Image image;
            ::Magic::Catalog catalog;
            ::Magic::PlanHandle plan;
            Dialogue::DefinitionHandle initial, helped, refused;
            Dialogue::Snapshot navigation;
            EntityRef principal;
            Action action;
            SceneState state = SceneState::Offered;
        };
        std::unique_ptr<Data> data_;
        explicit PairedCut(std::unique_ptr<Data> data) noexcept : data_(std::move(data)) {}
        friend class Recovery;
        friend struct RecoveryTestAccess;
    public:
        PairedCut(PairedCut&&) noexcept = default;
        PairedCut& operator=(PairedCut&&) noexcept = default;
        PairedCut(const PairedCut&) = delete;
        PairedCut& operator=(const PairedCut&) = delete;
    };

    class RecoveredPair final {
        std::unique_ptr<Host> host_;
        std::unique_ptr<Scene> scene_;
        RecoveredPair(std::unique_ptr<Host> host, std::unique_ptr<Scene> scene) noexcept
        : host_(std::move(host)), scene_(std::move(scene)) {}
        friend class Recovery;
    public:
        RecoveredPair(RecoveredPair&&) noexcept = default;
        RecoveredPair& operator=(RecoveredPair&&) = delete;
        RecoveredPair(const RecoveredPair&) = delete;
        RecoveredPair& operator=(const RecoveredPair&) = delete;
        Host& host() noexcept { return *host_; }
        Scene& scene() noexcept { return *scene_; }
    };

    class Recovery final {
        static std::expected<void, Error> validate(const PairedCut::Data&) noexcept;
    public:
        static std::expected<PairedCut, Error> capture(Host&, Scene&) noexcept;
        static std::expected<RecoveredPair, Error> restore(
            const PairedCut&, const Host& current_authority, const Host::Access&) noexcept;
    };
}
