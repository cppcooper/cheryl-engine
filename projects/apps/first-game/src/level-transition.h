#pragma once

#include <cheryl/assets/types/2d/sprite.h>
#include <cheryl/assets/types/2d/unicode-text.h>

#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string_view>

namespace CE::Runtime {
    class Context;
}

namespace FirstGame::Levels {
    enum class LevelId { Courtyard, Study };
    using RequestId = std::uint64_t;

    struct RoomSpec {
        std::string_view label;
        glm::vec2 spawn;
        CE::Assets::Grids::CellIndex cell;
    };
    [[nodiscard]] RoomSpec room_spec(LevelId room);

    struct ReadyLevel {
        LevelId id;
        CE::Text::Rendering::Generation label;
        std::shared_ptr<const CE::Assets::Sprites::Asset> sprite;
        std::shared_ptr<const CE::Rendering::Resources::Material> images;
        glm::vec2 spawn;
        CE::Assets::Grids::CellIndex cell;
    };

    enum class Disposition { Accepted, AlreadyCurrent, Busy, Closed };
    struct RequestResult {
        Disposition disposition;
        RequestId id = 0;
    };
    enum class CancelResult { Cancelled, NotPending };
    enum class Phase { Idle, Preparing, Uploading, Ready, Cancelled, Failed, Closed };
    enum class FailureStage { Allocation, PreparationSubmission, Preparation, PlatformSubmission, Upload };
    struct Failure {
        RequestId id;
        FailureStage stage;
        std::exception_ptr error;
    };
    struct Observation {
        RequestId id = 0;
        std::optional<LevelId> requested;
        Phase phase = Phase::Idle;
        bool pending = false;
        bool closed = false;
        std::optional<Failure> failure;
    };

    // Application-private, serialized simulation owner. Construct on platform before
    // simulation; destroy there after runtime settlement. Callbacks own inputs only.
    class Controller final {
        struct State;
        std::unique_ptr<State> state_;

    public:
        Controller(
            CE::Runtime::Context& engine,
            std::shared_ptr<const CE::Assets::Sprites::Asset> sprite,
            std::shared_ptr<const CE::Rendering::Resources::Material> images,
            std::optional<LevelId> initial = LevelId::Courtyard
        );
        ~Controller();
        Controller(const Controller&) = delete;
        Controller& operator=(const Controller&) = delete;

        [[nodiscard]] RequestResult request(LevelId room);
        [[nodiscard]] bool advance(); // Poll only; adoption occurs at this update boundary.
        [[nodiscard]] CancelResult cancel(RequestId id) noexcept;
        void unload() noexcept;
        void close() noexcept; // Invalidate, never wait; dependencies survive until final release.
        [[nodiscard]] std::shared_ptr<const ReadyLevel> current() const noexcept;
        [[nodiscard]] Observation observation() const noexcept;
    };
}
