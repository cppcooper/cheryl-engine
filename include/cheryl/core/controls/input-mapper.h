#pragma once

#ifndef CHERYL_SANDBOX_BUILD
#include "input-bindings.h"
#include <gainput/gainput.h>

namespace CE::Input {
    class InputMapper final : public gainput::InputListener, public InputBindings {
    public:
        InputMapper();
        explicit InputMapper(gainput::InputManager& manager);
        ~InputMapper() override;
        InputMapper(const InputMapper&) = delete;
        InputMapper& operator=(const InputMapper&) = delete;

        bool OnDeviceButtonFloat(gainput::DeviceId device, gainput::DeviceButtonId input, float old_value, float new_value) override;
        bool OnDeviceButtonBool(gainput::DeviceId device, gainput::DeviceButtonId input, bool old_value, bool new_value) override;
        [[nodiscard]] int GetPriority() const override { return 0; }

    private:
        gainput::InputManager& manager_;
        gainput::ListenerId id_;
    };
}
#endif
