#include <core/display/monitor.h>

namespace CE::Platform {
    Monitor::Monitor(const std::uint64_t id, const int width, const int height)
    : Dimensions(width, height), id_(id) {}
}
