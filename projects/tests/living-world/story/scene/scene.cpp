#include <story/scene/scene.h>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace CE::LivingWorld {
    namespace {
        Dialogue::DefinitionHandle initial_definition() {
            return Dialogue::compile(
                {{"living-help"}, 1, {"offer"},
                 {{{"offer"}, "Marin asks for help.", Dialogue::NodeKind::Prompt,
                   {{{"help"}, "Help Marin", {"waiting"}, Dialogue::ConditionKey{"may-help"}, {}}}},
                  {{"waiting"}, "Waiting for the host's answer.", Dialogue::NodeKind::Terminal, {}}}},
                {{{"may-help"}}, {}}
            ).value();
        }
        Dialogue::DefinitionHandle continuation_definition(bool helped) {
            return Dialogue::compile(
                {{helped ? "living-helped" : "living-refused"}, 1, {"answer"},
                 {{{"answer"}, helped ? "The help was applied." : "The help was refused.",
                   Dialogue::NodeKind::Terminal, {}}}}, {}
            ).value();
        }
        static_assert(std::is_nothrow_move_constructible_v<Dialogue::Session>);
        static_assert(std::is_nothrow_destructible_v<Dialogue::Session>);
        static_assert(std::is_nothrow_copy_assignable_v<Action>);
        static_assert(std::is_nothrow_destructible_v<Dialogue::Selection>);
    }

    Scene::Scene(Host& host, Host::Access access, Restored)
    : host_(host), access_(std::move(access)), fence_(host.fence()),
      incarnation_(std::make_shared<const unsigned char>(0)) {}

    Scene::Scene(Host& host, Host::Access access, EntityRef recipient)
    : host_(host), access_(std::move(access)), fence_(host.fence()),
      initial_(initial_definition()), helped_(continuation_definition(true)),
      refused_(continuation_definition(false)), incarnation_(std::make_shared<const unsigned char>(0)) {
        auto session = Dialogue::Session::start(initial_, {"living-scene"}).value();
        const Dialogue::ConditionHandler condition = [](const Dialogue::ConditionKey& key) noexcept {
            return key.value == "may-help";
        };
        auto view = session.view(condition).value();
        if (view.choices.size() != 1 || view.text.size() + view.choices[0].text.size() > 4096)
            throw std::length_error("Scene display capacity");
        selection_ = std::move(view.choices[0].selection);
        presentation_ = std::make_unique<Presentation>(Presentation{std::move(session), std::move(view.text)});
        action_ = host_.action(access_, recipient).value();
    }

    Display Scene::display() const {
        if (!host_.authorized(access_))
            return {state_, "Access unavailable.", {}, {}};
        if (state_ == SceneState::Offered) {
            if (!host_.live(action_.body))
                return {state_, "Offer expired.", {}, {}};
            Offer offer;
            offer.incarnation_ = incarnation_;
            return {state_, presentation_->text, "Help Marin", offer};
        }
        if (state_ == SceneState::Waiting)
            return {state_, "Waiting for the host's answer.", {}, {}};
        return {state_, presentation_->text, {}, {}};
    }

    std::expected<Receipt, Error> Scene::request(const Offer& offer) {
        if (presenting_)
            return std::unexpected(Error::Busy);
        if (!host_.authorized(access_) || !offer.incarnation_ || offer.incarnation_ != incarnation_)
            return std::unexpected(Error::Unauthorized);
        const auto existing = host_.lookup(access_, action_);
        if (existing || existing.error() != Error::Missing)
            return existing;
        if (state_ != SceneState::Offered)
            return std::unexpected(Error::Stale);
        const Dialogue::ConditionHandler condition = [this](const Dialogue::ConditionKey& key) noexcept {
            return key.value == "may-help" && host_.authorized(access_) && host_.live(action_.body);
        };
        auto reservation = host_.reserve(access_, action_, fence_);
        if (!reservation)
            return std::unexpected(reservation.error());
        const auto chosen = presentation_->session.choose(selection_, condition, {});
        if (!chosen)
            return std::unexpected(Error::Navigation);
        reservation->finalize();
        state_ = SceneState::Waiting;
        return Receipt{};
    }

    std::expected<Receipt, Error> Scene::process() noexcept {
        if (presenting_)
            return std::unexpected(Error::Busy);
        if (state_ == SceneState::Offered)
            return std::unexpected(Error::Missing);
        return host_.settle(access_, action_);
    }

    std::expected<void, Error> Scene::refresh(const std::function<void(std::string_view)>& present) {
        if (presenting_ || host_.presenting_)
            return std::unexpected(Error::Busy);
        const auto receipt = host_.lookup(access_, action_);
        if (!receipt)
            return std::unexpected(receipt.error());
        if (receipt->state == Disposition::Queued)
            return std::unexpected(Error::Busy);
        presenting_ = true;
        host_.presenting_ = true;
        struct Guard {
            bool& active;
            bool& owner_active;
            ~Guard() { active = false; owner_active = false; }
        } guard{presenting_, host_.presenting_};
        try {
            const bool helped = receipt->state == Disposition::Applied;
            auto session = Dialogue::Session::start(helped ? helped_ : refused_, {"living-answer"}).value();
            auto view = session.view().value();
            if (view.text.size() > 4096)
                return std::unexpected(Error::Capacity);
            auto next = std::make_unique<Presentation>(Presentation{std::move(session), std::move(view.text)});
            if (present)
                present(next->text);
            if (!host_.authorized(access_))
                return std::unexpected(Error::Unauthorized);
            presentation_.swap(next);
            state_ = helped ? SceneState::Helped : SceneState::Refused;
            return {};
        } catch (...) {
            return std::unexpected(Error::Preparation);
        }
    }

    bool Scene::notify(const std::function<void()>& wake) noexcept {
        if (!wake)
            return false;
        try {
            wake();
            return true;
        } catch (...) {
            return false;
        }
    }
    SceneState Scene::state() const noexcept { return state_; }
}
