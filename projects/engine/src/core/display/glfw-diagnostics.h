#pragma once
#include <core/diagnostics.h>

namespace CE::DisplayDetail {
    class NativeCallbackScope final {
        static thread_local unsigned int depth_;

    public:
        NativeCallbackScope() noexcept { ++depth_; }
        ~NativeCallbackScope() { --depth_; }
        NativeCallbackScope(const NativeCallbackScope&) = delete;
        NativeCallbackScope& operator=(const NativeCallbackScope&) = delete;
        [[nodiscard]] static bool active() noexcept { return depth_ != 0; }
    };
    struct GlfwDiagnostics {
        Diagnostics::DomainId domain = 0;
        std::uint64_t errors = 0;
        std::uint64_t host_failures = 0;
        int last_code = 0;
    };
    // Lifecycle/main thread installs before init and restores after termination.
    // Callback captures bounded numeric metadata and chains the borrowed host.
    void install_glfw_diagnostics() noexcept;
    void restore_glfw_diagnostics() noexcept;
    [[nodiscard]] GlfwDiagnostics glfw_diagnostics() noexcept;
    void report_glfw_diagnostics(const char* operation, bool emergency = false) noexcept;
}
