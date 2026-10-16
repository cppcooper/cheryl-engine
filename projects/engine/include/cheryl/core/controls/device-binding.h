#pragma once

#include "input-types.h"

#include <functional>

namespace CE::Input {
    struct Control {
        DeviceId device;
        ControlId control;
        bool operator==(const Control& o) const { return device == o.device && control == o.control; }
    };
}

namespace std {
    template <> struct hash<CE::Input::Control> {
        std::size_t operator()(const CE::Input::Control& k) const noexcept {
            std::hash<CE::Input::ControlId> hash_button;
            std::hash<CE::Input::DeviceId> hash_device;
            const std::size_t hash1 = hash_button(k.control);
            return hash1 ^ (hash_device(k.device) + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
        }
    };
}
