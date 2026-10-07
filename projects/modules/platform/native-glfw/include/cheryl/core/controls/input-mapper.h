#pragma once

#if CHERYL_NATIVE_INPUT
#include <gainput/gainput.h>
#include <unordered_set>
#include <core/controls/input-bindings.h>
#include <core/diagnostics.h>

namespace CE::Input {
    class InputMapper final : public gainput::InputListener,
                              public InputBindings {
        gainput::InputManager& manager_;
        gainput::ListenerId id_;
        std::unordered_set<DeviceId> externally_driven_;
        bool gamepad_diagnostics_ = false;
        const Diagnostics::DomainId diagnostic_domain_ = Diagnostics::next_domain_id();

    public:
        explicit InputMapper(gainput::InputManager& manager);
        ~InputMapper() override;
        InputMapper(const InputMapper&) = delete;
        InputMapper& operator=(const InputMapper&) = delete;
        // These devices feed State in callback order. Their later Gainput delta
        // notifications must not replay/reorder the already mapped transitions.
        void use_external_state(DeviceId device) { externally_driven_.insert(device); }
        // Configure on the input owner, outside a Gainput update.
        void set_gamepad_diagnostics(bool enabled) noexcept { gamepad_diagnostics_ = enabled; }

        bool OnDeviceButtonFloat(float deltaTime, DeviceId device, DeviceButtonId deviceButton, float oldValue, float newValue) override;
        bool OnDeviceButtonBool(gainput::DeviceId device, gainput::DeviceButtonId input, bool old_value, bool new_value) override;
        [[nodiscard]] int GetPriority() const override { return 0; }
    };
}
#endif
