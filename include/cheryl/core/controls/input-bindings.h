#pragma once

#include "device-binding.h"

#include <functional>
#include <unordered_map>

namespace CE::Input {
    class InputBindings {
    public:
        void bind_axis(DeviceBind binding, std::function<void(float, float)> callback);
        void bind_button(DeviceBind binding, std::function<void(bool, bool)> callback);
        void clear();

        void on_axis(DeviceBind binding, float old_value, float new_value) const;
        void on_button(DeviceBind binding, bool old_value, bool new_value) const;

    private:
        std::unordered_map<DeviceBind, std::function<void(float, float)>> axis_callbacks_;
        std::unordered_map<DeviceBind, std::function<void(bool, bool)>> button_callbacks_;
    };
}
