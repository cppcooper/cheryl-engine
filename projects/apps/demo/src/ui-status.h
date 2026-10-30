#pragma once

#include <cstdint>

struct DemoUiStatus {
    std::uint64_t updates;
    std::uint64_t clicks;
    std::uint64_t gamepad_presses;
    double wheel;
    float pan_x;
    float pan_y;
};
