#include "level-transition.h"

#include <cheryl/core/engine/engine-context.h>
#include <cheryl/text/layout.h>

#include <atomic>
#include <chrono>
#include <future>
#include <limits>
#include <stdexcept>
#include <utility>

namespace FirstGame::Levels {
    RoomSpec room_spec(const LevelId room) {
        switch (room) {
        case LevelId::Courtyard: return {"Courtyard", {0.5f, 0.5f}, 0};
        case LevelId::Study: return {"Study", {0.75f, 0.5f}, 1};
        }
        throw std::invalid_argument("Unknown First Game room");
    }

    namespace {
        struct Control {
            std::atomic<bool> cancelled{false};
            std::atomic<unsigned> flights{0};
        };

        // Future readiness can precede callback-capture destruction. Destroy the
        // owned callable before publishing settlement, even on rejected submission.
        struct Lease {
            std::shared_ptr<Control> control;
            explicit Lease(std::shared_ptr<Control> value)
            : control(std::move(value)) { control->flights.fetch_add(1, std::memory_order_relaxed); }
            ~Lease() { control->flights.fetch_sub(1, std::memory_order_release); }
        };
        template <typename Work> struct Flight {
            Lease lease;
            Work work;
            Flight(std::shared_ptr<Control> control, Work work)
            : lease(std::move(control)), work(std::move(work)) {}
        };
        template <typename Work> auto own_work(const std::shared_ptr<Control>& control, Work work) {
            return std::make_shared<Flight<Work>>(control, std::move(work));
        }
        template <typename T> bool ready(const std::future<T>& future) {
            return future.valid() && future.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
        }

        std::shared_ptr<const ReadyLevel> upload_level(
            const LevelId id,
            CE::Text::Rendering::Prepared prepared,
            CE::Rendering::Resources::Provider& resources,
            std::shared_ptr<const CE::Assets::Sprites::Asset> sprite,
            std::shared_ptr<const CE::Rendering::Resources::Material> images
        ) {
            const auto spec = room_spec(id);
            auto label = CE::Text::Rendering::upload(std::move(prepared), resources);
            return std::make_shared<const ReadyLevel>(ReadyLevel{id, std::move(label), std::move(sprite), std::move(images), spec.spawn, spec.cell});
        }
    }

    struct Controller::State {
        CE::Workers::Group workers;
        CE::Runtime::PlatformDispatcher::Submission platform;
        CE::Text::Font font;
        std::shared_ptr<const CE::Assets::Sprites::Asset> sprite;
        std::shared_ptr<const CE::Rendering::Resources::Material> images;
        std::shared_ptr<const ReadyLevel> current;
        std::shared_ptr<Control> active;
        std::future<std::optional<CE::Text::Rendering::Prepared>> preparing;
        std::future<std::shared_ptr<const ReadyLevel>> uploading;
        std::shared_ptr<const ReadyLevel> candidate;
        Observation observed;
        RequestId revision = 0;

        State(
            CE::Runtime::Context& engine,
            std::shared_ptr<const CE::Assets::Sprites::Asset> actor,
            std::shared_ptr<const CE::Rendering::Resources::Material> material
        )
        : workers(engine.make_worker_group({.max_concurrency = 1})),
          platform(engine.platform_dispatcher().submission()), font(engine.fonts().default_font()),
          sprite(std::move(actor)), images(std::move(material)) {
            if (!sprite || !images || sprite->definition().grid.cell_count() < 2)
                throw std::invalid_argument("First Game rooms require the two-cell actor and material");
        }

        void fail(const FailureStage stage) noexcept {
            observed.failure = Failure{observed.id, stage, std::current_exception()};
            if (!observed.closed && !(active && active->cancelled.load(std::memory_order_acquire)))
                observed.phase = Phase::Failed;
        }
    };

    Controller::Controller(
        CE::Runtime::Context& engine,
        std::shared_ptr<const CE::Assets::Sprites::Asset> sprite,
        std::shared_ptr<const CE::Rendering::Resources::Material> images,
        const std::optional<LevelId> initial
    ) : state_(std::make_unique<State>(engine, std::move(sprite), std::move(images))) {
        if (initial) {
            const auto spec = room_spec(*initial);
            state_->current = upload_level(*initial, CE::Text::Rendering::prepare(CE::Text::layout_text(
                state_->font, spec.label, {.pixel_height = 24}
            )), engine.resources(), state_->sprite, state_->images);
            state_->observed.phase = Phase::Ready;
        }
    }

    Controller::~Controller() { close(); }

    RequestResult Controller::request(const LevelId room) {
        auto& state = *state_;
        if (state.observed.closed)
            return {Disposition::Closed};
        const auto spec = room_spec(room); // Invalid identity changes nothing.
        if (state.observed.pending)
            return {Disposition::Busy};
        if (state.current && state.current->id == room)
            return {Disposition::AlreadyCurrent};
        if (state.revision == std::numeric_limits<RequestId>::max())
            throw std::overflow_error("First Game request identity exhausted");
        const auto id = ++state.revision;
        state.observed = {id, room, Phase::Preparing, true, false, {}};
        auto stage = FailureStage::Allocation;
        try {
            state.active = std::make_shared<Control>();
            auto flight = own_work(state.active, [control = state.active, font = state.font, spec]() -> std::optional<CE::Text::Rendering::Prepared> {
                if (control->cancelled.load(std::memory_order_acquire))
                    return {};
                return CE::Text::Rendering::prepare(CE::Text::layout_text(font, spec.label, {.pixel_height = 24}));
            });
            stage = FailureStage::PreparationSubmission;
            state.preparing = state.workers.submit([flight = std::move(flight)] { return flight->work(); });
        } catch (...) {
            state.fail(stage);
            state.observed.pending = false;
            state.active.reset();
        }
        return {Disposition::Accepted, id};
    }

    bool Controller::advance() {
        auto& state = *state_;
        if (!state.observed.pending)
            return false;
        try {
            if (ready(state.uploading))
                state.candidate = state.uploading.get();
        } catch (...) {
            state.fail(FailureStage::Upload);
        }
        std::optional<CE::Text::Rendering::Prepared> prepared;
        try {
            if (ready(state.preparing))
                prepared = state.preparing.get();
        } catch (...) {
            state.fail(FailureStage::Preparation);
        }
        if (prepared && !state.active->cancelled.load(std::memory_order_acquire)) {
            try {
                auto flight = own_work(state.active, [control = state.active, id = *state.observed.requested,
                    prepared = std::move(*prepared), sprite = state.sprite, images = state.images](CE::Runtime::Context& owner) mutable -> std::shared_ptr<const ReadyLevel> {
                    if (control->cancelled.load(std::memory_order_acquire))
                        return {};
                    return upload_level(id, std::move(prepared), owner.resources(), std::move(sprite), std::move(images));
                });
                state.uploading = state.platform.submit([flight = std::move(flight)](CE::Runtime::Context& owner) { return flight->work(owner); });
                state.observed.phase = Phase::Uploading;
            } catch (...) {
                state.fail(FailureStage::PlatformSubmission);
            }
        }
        if (state.preparing.valid() || state.uploading.valid() || state.active->flights.load(std::memory_order_acquire) != 0)
            return false;

        const bool adopt = !state.observed.closed && !state.active->cancelled.load(std::memory_order_acquire) &&
                           !state.observed.failure && static_cast<bool>(state.candidate);
        if (adopt) {
            state.current = std::move(state.candidate);
            state.observed.phase = Phase::Ready;
        }
        state.candidate.reset();
        state.active.reset();
        state.observed.pending = false;
        return adopt;
    }

    CancelResult Controller::cancel(const RequestId id) noexcept {
        auto& state = *state_;
        if (!state.observed.pending || state.observed.id != id || !state.active || state.active->cancelled.exchange(true, std::memory_order_acq_rel))
            return CancelResult::NotPending;
        if (!state.observed.closed)
            state.observed.phase = Phase::Cancelled;
        return CancelResult::Cancelled;
    }

    void Controller::unload() noexcept {
        static_cast<void>(cancel(state_->observed.id));
        state_->current.reset();
    }

    void Controller::close() noexcept {
        static_cast<void>(cancel(state_->observed.id));
        state_->observed.closed = true;
        state_->observed.phase = Phase::Closed;
    }

    std::shared_ptr<const ReadyLevel> Controller::current() const noexcept { return state_->current; }
    Observation Controller::observation() const noexcept { return state_->observed; }
}
