#include "glfw-diagnostics.h"
#include <internals/compile-time-logging.hpp>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <atomic>
#include <chrono>

namespace CE::DisplayDetail {
    thread_local unsigned int NativeCallbackScope::depth_ = 0;
    namespace {
        std::atomic<GLFWerrorfun> previous_callback{nullptr};
        std::atomic<Diagnostics::DomainId> domain{0};
        std::atomic<std::uint64_t> errors{0};
        std::atomic<std::uint64_t> host_failures{0};
        std::atomic<int> last_code{0};
        std::atomic<std::uint64_t> reported_errors{0};
        std::atomic<std::uint64_t> reported_host_failures{0};
        std::atomic<std::int64_t> next_report{0};

        void on_error(const int code, const char* description) noexcept {
            NativeCallbackScope callback_scope;
            last_code.store(code, std::memory_order_relaxed);
            errors.fetch_add(1, std::memory_order_relaxed);
            // The description stays borrowed for this call only; no engine copy.
            if (const auto previous = previous_callback.load(); previous && previous != on_error) {
                try {
                    previous(code, description);
                } catch (...) {
                    host_failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    }

    void install_glfw_diagnostics() noexcept {
        domain.store(Diagnostics::next_domain_id());
        errors.store(0);
        host_failures.store(0);
        last_code.store(0);
        reported_errors.store(0);
        reported_host_failures.store(0);
        next_report.store(0);
        previous_callback.store(glfwSetErrorCallback(on_error));
    }

    void restore_glfw_diagnostics() noexcept {
        const auto previous = previous_callback.load();
        const auto current = glfwSetErrorCallback(previous);
        if (current != on_error)
            glfwSetErrorCallback(current); // Preserve a later host replacement.
        previous_callback.store(nullptr);
    }

    GlfwDiagnostics glfw_diagnostics() noexcept {
        return {domain.load(), errors.load(), host_failures.load(), last_code.load()};
    }

    void report_glfw_diagnostics(const char* operation, const bool emergency) noexcept {
        if (NativeCallbackScope::active())
            return;
        const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (!emergency) {
            auto deadline = next_report.load();
            if (now < deadline || !next_report.compare_exchange_strong(deadline, now + 2000000000LL))
                return;
        }
        const auto snapshot = glfw_diagnostics();
        const auto prior = reported_errors.exchange(snapshot.errors);
        const auto prior_host = reported_host_failures.exchange(snapshot.host_failures);
        if (snapshot.errors > prior) {
            if (emergency)
                Diagnostics::report_outcome("glfw", snapshot.domain, operation, "native_error", snapshot.last_code);
            else
                CE_LOG_ERROR(
                    CE::enginelog, "subsystem=glfw domain={} operation={} outcome=native_error code={} errors={} new={}", snapshot.domain,
                    operation, snapshot.last_code, snapshot.errors, snapshot.errors - prior
                );
        }
        if (snapshot.host_failures > prior_host)
            Diagnostics::report_outcome("glfw", snapshot.domain, operation, "host_callback_failed", snapshot.host_failures - prior_host);
    }
}
