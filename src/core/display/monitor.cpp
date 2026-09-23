#include <core/display/monitor.h>

namespace CE {
    Monitor::Monitor(const std::uint64_t id, const int width, const int height) :
        ViewPort(width, height), id_(id) {
    }
}
