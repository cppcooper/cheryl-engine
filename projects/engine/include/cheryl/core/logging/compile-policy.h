#pragma once
#include <spdlog/common.h>

namespace ctlog {
    enum LogLevel { FATAL_ = 1 << 0, ERROR_ = 1 << 1, WARNING_ = 1 << 2, INFO_ = 1 << 3, DEBUG_ = 1 << 4, TRACE_ = 1 << 5 };
}

// These definitions must agree in every translation unit sharing Cheryl headers.
// Prefer the library target's public configuration; CTWriteMask remains an alias
// for legacy target-wide overrides, never an include-local switch.
#ifndef CHERYL_LOG_PROFILE
#ifdef NDEBUG
#define CHERYL_LOG_PROFILE 2
#else
#define CHERYL_LOG_PROFILE 0
#endif
#endif

#ifndef CHERYL_LOG_COMPILED_MASK
#ifdef CTWriteMask
#define CHERYL_LOG_COMPILED_MASK CTWriteMask
#else
#define CHERYL_LOG_COMPILED_MASK (CHERYL_LOG_PROFILE == 0 ? 0x3f : CHERYL_LOG_PROFILE == 1 ? 0x1f : 0x0f)
#endif
#endif

#ifndef CTWriteMask
#define CTWriteMask CHERYL_LOG_COMPILED_MASK
#endif

static_assert(CHERYL_LOG_PROFILE >= 0 && CHERYL_LOG_PROFILE <= 2, "Unknown Cheryl logging profile");
static_assert(CHERYL_LOG_COMPILED_MASK >= 0 && (CHERYL_LOG_COMPILED_MASK & ~0x3f) == 0, "Unknown Cheryl logging severity bits");
static_assert(CTWriteMask == CHERYL_LOG_COMPILED_MASK, "Conflicting Cheryl logging masks");

namespace ctlog {
    inline constexpr unsigned compiled_mask = CHERYL_LOG_COMPILED_MASK;
    inline constexpr int profile = CHERYL_LOG_PROFILE; // Developer, support, release.

    [[nodiscard]] constexpr bool enabled(const unsigned levels) noexcept { return (compiled_mask & levels) != 0; }

    [[nodiscard]] constexpr unsigned severity_bit(const spdlog::level level) noexcept {
        switch (level) {
            case spdlog::level::critical:
                return FATAL_;
            case spdlog::level::err:
                return ERROR_;
            case spdlog::level::warn:
                return WARNING_;
            case spdlog::level::info:
                return INFO_;
            case spdlog::level::debug:
                return DEBUG_;
            case spdlog::level::trace:
                return TRACE_;
            default:
                return 0;
        }
    }

    [[nodiscard]] constexpr spdlog::level runtime_level(const LogLevel level) noexcept {
        switch (level) {
            case FATAL_:
                return spdlog::level::critical;
            case ERROR_:
                return spdlog::level::err;
            case WARNING_:
                return spdlog::level::warn;
            case INFO_:
                return spdlog::level::info;
            case DEBUG_:
                return spdlog::level::debug;
            case TRACE_:
                return spdlog::level::trace;
        }
        return spdlog::level::off;
    }
}
