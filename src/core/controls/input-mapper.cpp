#include <core/controls/input-mapper.h>
#include <core/controls/input-system.h>

namespace CE::Input {
    InputMapper::InputMapper() : InputMapper(InputSystem::get().manager()) {
    }

    InputMapper::InputMapper(gainput::InputManager& manager) : manager_(manager), id_(manager_.AddListener(this)) {
    }

    InputMapper::~InputMapper() {
        manager_.RemoveListener(id_);
    }

    bool InputMapper::OnDeviceButtonFloat(const gainput::DeviceId device, const gainput::DeviceButtonId input,
                                          const float old_value, const float new_value) {
        on_axis({device, input}, old_value, new_value);
        return true;
    }

    bool InputMapper::OnDeviceButtonBool(const gainput::DeviceId device, const gainput::DeviceButtonId input,
                                         const bool old_value, const bool new_value) {
        on_button({device, input}, old_value, new_value);
        return true;
    }
}
