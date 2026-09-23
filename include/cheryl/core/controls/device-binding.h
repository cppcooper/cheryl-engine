#pragma once

#include "input-types.h"

#include <functional>

namespace CE::Input {
    struct DeviceBind {
        DeviceId id;
        DeviceButtonId btn;
        bool operator==(const DeviceBind& o) const {
            return id == o.id && btn == o.btn;
        }

    };
}

namespace std {
    template <>
    struct hash<CE::Input::DeviceBind> {
        std::size_t operator()(const CE::Input::DeviceBind& k) const noexcept {
            std::hash<CE::Input::DeviceButtonId> hash_button;
            std::hash<CE::Input::DeviceId> hash_device;
            const std::size_t hash1 = hash_button(k.btn);
            return hash1 ^ (hash_device(k.id) + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
        }
    };
}
