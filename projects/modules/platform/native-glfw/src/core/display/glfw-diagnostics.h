#pragma once

#include <cheryl/export.h>
#include <core/diagnostics.h>
#include <backends/native-glfw/diagnostics.h>

namespace CE::Platform::GLFW::Detail {
    class NativeCallbackScope final {
        static thread_local unsigned int depth_;

    public:
        CE_NATIVE_GLFW_API NativeCallbackScope() noexcept;
        CE_NATIVE_GLFW_API ~NativeCallbackScope();
        NativeCallbackScope(const NativeCallbackScope&) = delete;
        NativeCallbackScope& operator=(const NativeCallbackScope&) = delete;
        [[nodiscard]] CE_NATIVE_GLFW_API static bool active() noexcept;
    };
    struct GlfwDiagnostics {
        ::CE::Diagnostics::DomainId domain = 0;
        std::uint64_t errors = 0;
        std::uint64_t host_failures = 0;
        int last_code = 0;
    };
    // Lifecycle/main thread installs before init and restores after termination.
    // Callback captures bounded numeric metadata and chains the borrowed host.
    CE_NATIVE_GLFW_API void install_glfw_diagnostics() noexcept;
    CE_NATIVE_GLFW_API void restore_glfw_diagnostics() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API GlfwDiagnostics glfw_diagnostics() noexcept;
}
