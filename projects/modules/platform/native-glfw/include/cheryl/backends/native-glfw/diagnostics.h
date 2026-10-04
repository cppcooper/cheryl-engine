#pragma once

namespace CE::DisplayDetail {
    // Report captured GLFW failures after returning from native callbacks. The
    // native module owns callback installation, bounded capture and restoration.
    void report_glfw_diagnostics(const char* operation, bool emergency = false) noexcept;
}
