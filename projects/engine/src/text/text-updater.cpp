#include <text/text-updater.h>

#include <core/engine/engine-context.h>
#include <internals/exceptions.h>

#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <utility>

namespace CE::Text::Rendering {
    namespace {
        bool same_width(const std::optional<float> left, const std::optional<float> right) {
            if (left.has_value() != right.has_value())
                return false;
            return !left || *left == *right || (std::isnan(*left) && std::isnan(*right));
        }

        bool same_request(const Request& left, const Request& right) {
            return left.font == right.font && left.text == right.text && left.material == right.material &&
                   left.layout.pixel_height == right.layout.pixel_height && left.layout.direction == right.layout.direction &&
                   same_width(left.layout.maximum_width, right.layout.maximum_width) && left.layout.language == right.layout.language &&
                   left.layout.font_style.weight == right.layout.font_style.weight &&
                   left.layout.font_style.italic == right.layout.font_style.italic &&
                   left.raster.maximum_page_extent == right.raster.maximum_page_extent &&
                   left.raster.positioned_runs == right.raster.positioned_runs;
        }

        template <typename T> bool ready(const std::future<T>& future) {
            return future.valid() && future.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
        }
    }

    struct Updater::State {
        CE::Workers::Group workers;
        CE::Runtime::PlatformDispatcher::Submission platform;
        Font default_font;
        std::optional<Request> desired;
        std::uint64_t desired_revision = 0;
        std::uint64_t settled_revision = 0;
        std::uint64_t active_revision = 0;
        std::shared_ptr<const Request> active;
        std::shared_ptr<const Request> successful;
        std::future<Prepared> preparation;
        std::future<std::shared_ptr<const Generation>> uploading;
        std::shared_ptr<const Generation> current;
        std::optional<Failure> failure;
        bool last_failed = false;

        explicit State(CE::Runtime::Context& engine)
        : workers(engine.make_worker_group({.max_concurrency = 1})),
          platform(engine.platform_dispatcher().submission()),
          default_font(engine.fonts().default_font()) {}

        void advance_revision() {
            if (desired_revision == std::numeric_limits<std::uint64_t>::max())
                throw Exceptions::failed_operation(CE_HERE, "Text request revision limit reached");
            ++desired_revision;
        }

        void fail(const FailureStage stage) noexcept {
            failure = Failure{stage, std::current_exception(), active_revision};
            settled_revision = active && desired && same_request(*active, *desired) ? desired_revision : active_revision;
            preparation = {};
            uploading = {};
            active.reset();
            last_failed = true;
        }

        [[nodiscard]] bool pending() const noexcept {
            return preparation.valid() || uploading.valid() || desired_revision != settled_revision;
        }
    };

    Updater::Updater(CE::Runtime::Context& engine)
    : state_(std::make_unique<State>(engine)) {}

    Updater::Updater(CE::Runtime::Context& engine, Request initial)
    : Updater(engine) {
        request(std::move(initial));
        const auto& selected = *state_->desired;
        state_->current = std::make_shared<const Generation>(upload(
            prepare(layout_text(selected.font, selected.text, selected.layout), selected.raster), engine.resources(), selected.material
        ));
        state_->successful = std::make_shared<const Request>(selected);
        state_->settled_revision = state_->desired_revision;
    }

    Updater::~Updater() = default;

    void Updater::request(Request next) {
        if (!next.font)
            next.font = state_->default_font;
        if (state_->desired && same_request(*state_->desired, next))
            return;
        state_->advance_revision();
        state_->desired = std::move(next);
        state_->failure.reset();
        if (!state_->preparation.valid() && !state_->uploading.valid() && state_->successful &&
            same_request(*state_->successful, *state_->desired)) {
            state_->settled_revision = state_->desired_revision;
            state_->last_failed = false;
        }
    }

    bool Updater::update() {
        bool adopted = false;
        try {
            if (ready(state_->uploading)) {
                auto replacement = state_->uploading.get();
                state_->current = std::move(replacement);
                state_->settled_revision =
                    same_request(*state_->active, *state_->desired) ? state_->desired_revision : state_->active_revision;
                state_->successful = state_->active;
                state_->active.reset();
                state_->last_failed = false;
                adopted = true;
            }
        } catch (...) {
            state_->fail(FailureStage::Upload);
        }

        std::optional<Prepared> prepared;
        try {
            if (ready(state_->preparation))
                prepared = state_->preparation.get();
        } catch (...) {
            state_->fail(FailureStage::Preparation);
        }
        if (prepared) {
            try {
                state_->uploading = state_->platform.submit([prepared = std::move(*prepared),
                                                             material = state_->active->material](CE::Runtime::Context& owner) mutable {
                    return std::make_shared<const Generation>(upload(std::move(prepared), owner.resources(), std::move(material)));
                });
            } catch (...) {
                state_->fail(FailureStage::Upload);
            }
        }

        if (!state_->preparation.valid() && !state_->uploading.valid() && state_->desired_revision != state_->settled_revision) {
            state_->active_revision = state_->desired_revision;
            try {
                state_->active = std::make_shared<const Request>(*state_->desired);
                state_->preparation = state_->workers.submit([request = state_->active] {
                    return prepare(layout_text(request->font, request->text, request->layout), request->raster);
                });
            } catch (...) {
                state_->fail(FailureStage::Preparation);
            }
        }
        return adopted;
    }

    void Updater::retry() {
        if (state_->last_failed && !state_->pending()) {
            state_->advance_revision();
            state_->failure.reset();
            state_->last_failed = false;
        }
    }

    bool Updater::pending() const noexcept {
        return state_->pending();
    }

    std::shared_ptr<const Generation> Updater::current() const noexcept {
        return state_->current;
    }

    std::optional<Failure> Updater::failure() const noexcept {
        return state_->failure;
    }
}
