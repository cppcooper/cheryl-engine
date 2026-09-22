#include <core/controls/input-bindings.h>

#include <utility>

namespace CE::Input {
    void InputBindings::bind_axis(const DeviceBind binding, std::function<void(float, float)> callback) {
        if (callback)
            axis_callbacks_.insert_or_assign(binding, std::move(callback));
        else
            axis_callbacks_.erase(binding);
    }

    void InputBindings::bind_button(const DeviceBind binding, std::function<void(bool, bool)> callback) {
        if (callback)
            button_callbacks_.insert_or_assign(binding, std::move(callback));
        else
            button_callbacks_.erase(binding);
    }

    void InputBindings::clear() {
        axis_callbacks_.clear();
        button_callbacks_.clear();
    }

    void InputBindings::on_axis(const DeviceBind binding, const float old_value, const float new_value) const {
        if (const auto it = axis_callbacks_.find(binding); it != axis_callbacks_.end()) {
            const auto callback = it->second;
            callback(old_value, new_value);
        }
    }

    void InputBindings::on_button(const DeviceBind binding, const bool old_value, const bool new_value) const {
        if (const auto it = button_callbacks_.find(binding); it != button_callbacks_.end()) {
            const auto callback = it->second;
            callback(old_value, new_value);
        }
    }
}
