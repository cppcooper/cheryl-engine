#include <core/controls/input-mapper.h>
#include <internals/compile-time-logging.hpp>

namespace CE::Input {
    InputMapper::InputMapper(gainput::InputManager& manager)
    : manager_(manager), id_(manager_.AddListener(this)) {}

    InputMapper::~InputMapper() {
        manager_.RemoveListener(id_);
    }

    bool InputMapper::OnDeviceButtonFloat(
        const float /*delta_time*/, // Current axis state is independent of elapsed time.
        const gainput::DeviceId device,
        const gainput::DeviceButtonId input,
        const float old_value,
        const float new_value
    ) {
        if (!externally_driven_.contains(device)) {
            if (gamepad_diagnostics_)
                Logger<platformlog>::write_lazy<ctlog::TRACE_>([&](auto& log) {
                    const auto* source = manager_.GetDevice(device);
                    if (!source || source->GetType() == gainput::InputDevice::DT_PAD)
                        log.trace(
                            "subsystem=input domain={} operation=gainput_delta kind=axis listener={} device={} device_known={} "
                            "available={} control={} old={} new={}",
                            diagnostic_domain_, id_, device, source != nullptr, source && source->IsAvailable(), input, old_value,
                            new_value
                        );
                });
            on_axis({device, input}, new_value);
        }
        return true;
    }

    bool InputMapper::OnDeviceButtonBool(
        const gainput::DeviceId device,
        const gainput::DeviceButtonId input,
        const bool old_value,
        const bool new_value
    ) {
        if (!externally_driven_.contains(device)) {
            if (gamepad_diagnostics_)
                Logger<platformlog>::write_lazy<ctlog::TRACE_>([&](auto& log) {
                    const auto* source = manager_.GetDevice(device);
                    if (!source || source->GetType() == gainput::InputDevice::DT_PAD)
                        log.trace(
                            "subsystem=input domain={} operation=gainput_delta kind=button listener={} device={} device_known={} "
                            "available={} control={} old={} new={}",
                            diagnostic_domain_, id_, device, source != nullptr, source && source->IsAvailable(), input, old_value,
                            new_value
                        );
                });
            on_button({device, input}, new_value);
        }
        return true;
    }
}
