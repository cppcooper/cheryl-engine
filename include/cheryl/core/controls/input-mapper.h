#pragma once

#ifndef CHERYL_SANDBOX_BUILD
#include <gainput/gainput.h>
#include <unordered_set>
#include "input-bindings.h"

namespace CE::Input {
    class InputMapper final : public gainput::InputListener, public InputBindings {
        gainput::InputManager& manager_;
        gainput::ListenerId id_;
        std::unordered_set<DeviceId> externally_driven_;

    public:
        explicit InputMapper(gainput::InputManager& manager);
        ~InputMapper() override;
        InputMapper(const InputMapper&) = delete;
        InputMapper& operator=(const InputMapper&) = delete;
        // These devices feed State in callback order. Their later Gainput delta
        // notifications must not replay/reorder the already mapped transitions.
        void use_external_state(DeviceId device) { externally_driven_.insert(device); }

        bool OnDeviceButtonFloat(gainput::DeviceId device, gainput::DeviceButtonId input, float old_value, float new_value) override;
        bool OnDeviceButtonBool(gainput::DeviceId device, gainput::DeviceButtonId input, bool old_value, bool new_value) override;
        [[nodiscard]] int GetPriority() const override { return 0; }
    };
}
#endif
