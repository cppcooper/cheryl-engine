#pragma once
#include "device-binding.h"
#include <gainput/gainput.h>
#include <unordered_map>
#include <functional>

namespace CE::Input {
    class InputMapper final : public gainput::InputListener {
    public:
        InputMapper();
        ~InputMapper() override;
        // handles axis input mappings
        bool OnDeviceButtonFloat(gainput::DeviceId device, gainput::DeviceButtonId input, float old_value, float new_value) override;
        // handles button input mappings
        bool OnDeviceButtonBool(gainput::DeviceId device, gainput::DeviceButtonId input, bool old_value, bool new_value) override;
        [[nodiscard]] int GetPriority() const override { return 0; };

    private:
        gainput::ListenerId id;
        std::unordered_map<DeviceBind, std::function<void(bool,bool)>> button_callbacks;
        std::unordered_map<DeviceBind, std::function<void(float,float)>> axis_callbacks;

    public:
        void bind_axis(DeviceBind binding, std::function<void(float,float)> callback);
        void bind_button(DeviceBind binding, std::function<void(bool,bool)> callback);
    };
}
