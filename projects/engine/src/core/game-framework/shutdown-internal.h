#pragma once

#include <core/game-framework/shutdown.h>
#include <core/display/window-interface.h>
#include <stop_token>

namespace CE::Runtime::Detail {
    class ShutdownController final {
        std::shared_ptr<ShutdownState> state_;
        struct Bridge;
        std::shared_ptr<Bridge> bridge_;
        CE::Events::EventBus::Registration bridge_registration_;
        bool processing_ = false;

    public:
        CE_API ShutdownController(std::stop_source, std::function<void()> wake);
        CE_API ~ShutdownController();
        [[nodiscard]] CE_API ShutdownEndpoint endpoint() const;
        [[nodiscard]] CE_API ShutdownPolicy policy(ShutdownPolicyCallback, ShutdownPolicyOptions, CE::Events::EventBus::Delivery);
        CE_API void begin(CE::Events::EventBus&);
        CE_API void process(CE::Events::EventBus&, RuntimeLifecycle&, InitializedCapabilities);
        CE_API void window_close(CE::Events::EventBus&, const CE::Platform::WindowCloseRequested&);
        CE_API void force(ShutdownCause) noexcept;
        CE_API void quiescing() noexcept;
        CE_API void close() noexcept;
        [[nodiscard]] CE_API bool has_pending() const noexcept;
        [[nodiscard]] CE_API ShutdownCause cause() const noexcept;
    };
}
