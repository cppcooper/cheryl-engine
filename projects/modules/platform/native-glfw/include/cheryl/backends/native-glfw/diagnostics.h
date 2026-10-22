#pragma once

#include <cheryl/export.h>

namespace CE::Platform::GLFW::Detail {
    // Report captured GLFW failures after returning from native callbacks. The
    // native module owns callback installation, bounded capture and restoration.
    CE_NATIVE_GLFW_API void report_glfw_diagnostics(const char* operation, bool emergency = false) noexcept;
}
