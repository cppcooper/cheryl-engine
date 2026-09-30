#include <core/controls/input-mapper.h>

namespace CE::Input {
    InputMapper::InputMapper(gainput::InputManager& manager)
    : manager_(manager), id_(manager_.AddListener(this)) {}

    InputMapper::~InputMapper() { manager_.RemoveListener(id_); }

    bool InputMapper::OnDeviceButtonFloat(const gainput::DeviceId device,
                                          const gainput::DeviceButtonId input,
                                          const float,
                                          const float new_value
        ) {
        if (!externally_driven_.contains(device))
            on_axis({device, input}, new_value);
        return true;
    }

    bool
    InputMapper::OnDeviceButtonBool(const gainput::DeviceId device, const gainput::DeviceButtonId input, const bool, const bool new_value) {
        if (!externally_driven_.contains(device))
            on_button({device, input}, new_value);
        return true;
    }
}
