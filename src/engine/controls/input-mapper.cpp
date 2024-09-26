#include <engine/controls/input-mapper.h>
#include <templates/singleton.h>

namespace CE::Input {

    InputMapper::InputMapper() {
        id = Singleton_CTS<gainput::InputManager>::get().AddListener(this);
    }

    InputMapper::~InputMapper() {
        Singleton_CTS<gainput::InputManager>::get().RemoveListener(id);
    }

    bool InputMapper::OnDeviceButtonFloat(gainput::DeviceId device, gainput::DeviceButtonId input, float old_value, float new_value) {
        if(const DeviceBind binding{device,input}; axis_callbacks.contains(binding)) {
            axis_callbacks[binding](old_value, new_value);
        }
        return false;
    }

    bool InputMapper::OnDeviceButtonBool(gainput::DeviceId device, gainput::DeviceButtonId input, bool old_value, bool new_value) {
        if(const DeviceBind binding{device,input}; button_callbacks.contains(binding)) {
            button_callbacks[binding](old_value, new_value);
        }
        return false;
    }

    void InputMapper::bind_axis(DeviceBind binding, std::function<void(float, float)> callback) {
        axis_callbacks.emplace(binding, callback);
    }

    void InputMapper::bind_button(DeviceBind binding, std::function<void(bool, bool)> callback) {
        axis_callbacks.emplace(binding, callback);
    }

}