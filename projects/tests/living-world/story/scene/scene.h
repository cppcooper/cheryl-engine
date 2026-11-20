#pragma once

#include <host/host.h>
#include <cheryl/core/game-framework/dialogue.h>

#include <functional>
#include <string>
#include <string_view>

namespace CE::LivingWorld {
    enum class SceneState { Offered, Waiting, Helped, Refused };
    class Offer {
        std::shared_ptr<const void> incarnation_;
        friend class Scene;
    public:
        Offer() = default;
    };
    struct Display {
        SceneState state = SceneState::Offered;
        std::string text;
        std::string choice;
        std::optional<Offer> offer;
    };
    class Scene final {
        struct Presentation {
            Dialogue::Session session;
            std::string text;
        };
        Host& host_;
        Host::Access access_;
        OwnerFence fence_;
        Dialogue::DefinitionHandle initial_, helped_, refused_;
        std::unique_ptr<Presentation> presentation_;
        Dialogue::Selection selection_;
        std::shared_ptr<const void> incarnation_;
        Action action_;
        SceneState state_ = SceneState::Offered;
        bool presenting_ = false;

    public:
        Scene(Host& host, Host::Access access, EntityRef recipient);
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Display display() const;
        std::expected<Receipt, Error> request(const Offer& offer);
        std::expected<Receipt, Error> process() noexcept;
        std::expected<void, Error> refresh(const std::function<void(std::string_view)>& present = {});
        static bool notify(const std::function<void()>& wake) noexcept;
        SceneState state() const noexcept;
    };
}
