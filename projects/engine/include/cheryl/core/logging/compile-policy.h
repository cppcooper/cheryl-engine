#pragma once
#include "types.h"

namespace CE::Logging::Compile {
    /** Compile-time severity mask bits; combine bits for enabled(), not runtime_level(). */
    enum LogLevel { FATAL_ = 1 << 0, ///< Critical writes and their trace behavior.
        ERROR_ = 1 << 1, ///< Error records.
        WARNING_ = 1 << 2, ///< Warning records.
        INFO_ = 1 << 3, ///< Informational records.
        DEBUG_ = 1 << 4, ///< Development diagnostics.
        TRACE_ = 1 << 5 ///< Detailed tracing.
    };
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

namespace CE::Logging::Compile {
    inline constexpr unsigned compiled_mask = CHERYL_LOG_COMPILED_MASK; ///< Target-wide enabled severity bits; identical across sharing translation units.
    inline constexpr int profile = CHERYL_LOG_PROFILE; ///< Target profile ID: Developer=0, Support=1, Release=2.

    [[nodiscard]] constexpr bool enabled(const unsigned levels) noexcept {
        return (compiled_mask & levels) != 0;
    }

    [[nodiscard]] constexpr unsigned severity_bit(const ::CE::Logging::Level level) noexcept {
        switch (level) {
            case ::CE::Logging::Level::critical:
                return FATAL_;
            case ::CE::Logging::Level::err:
                return ERROR_;
            case ::CE::Logging::Level::warn:
                return WARNING_;
            case ::CE::Logging::Level::info:
                return INFO_;
            case ::CE::Logging::Level::debug:
                return DEBUG_;
            case ::CE::Logging::Level::trace:
                return TRACE_;
            default:
                return 0;
        }
    }

    [[nodiscard]] constexpr ::CE::Logging::Level runtime_level(const LogLevel level) noexcept {
        switch (level) {
            case FATAL_:
                return ::CE::Logging::Level::critical;
            case ERROR_:
                return ::CE::Logging::Level::err;
            case WARNING_:
                return ::CE::Logging::Level::warn;
            case INFO_:
                return ::CE::Logging::Level::info;
            case DEBUG_:
                return ::CE::Logging::Level::debug;
            case TRACE_:
                return ::CE::Logging::Level::trace;
        }
        return ::CE::Logging::Level::off;
    }
}

/**
 * @fn constexpr bool CE::Logging::Compile::enabled(unsigned levels) noexcept
 * @brief Test whether any supplied severity bit is compiled.
 * @param levels LogLevel bit mask, possibly a combination.
 * @return True when compiled_mask intersects levels; this says nothing about runtime logger/sink gates.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn constexpr unsigned CE::Logging::Compile::severity_bit(CE::Logging::Level level) noexcept
 * @brief Convert one runtime severity to its compile-time bit.
 * @return Matching bit, or zero for off/unknown values. No logging or initialization occurs.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn constexpr CE::Logging::Level CE::Logging::Compile::runtime_level(LogLevel level) noexcept
 * @brief Convert one compile-time severity bit to a runtime level.
 * @param level A single recognized bit, not a mask combination.
 * @return Matching runtime level, or off for unrecognized/combined values.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/** @def CHERYL_LOG_PROFILE
 * Target-wide logging profile ID (0 Developer, 1 Support, 2 Release). Use the library target's public configuration in every sharing translation unit; fallback uses NDEBUG.
 */
/** @def CHERYL_LOG_COMPILED_MASK
 * Target-wide six-bit severity mask. A disabled branch is compiled out; changing runtime levels cannot restore it. Must agree across translation units.
 */
/** @def CTWriteMask
 * Legacy alias for CHERYL_LOG_COMPILED_MASK, supported only as a consistent target-wide override, never a per-include switch.
 */
