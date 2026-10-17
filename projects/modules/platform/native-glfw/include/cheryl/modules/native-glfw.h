#pragma once

/** @file
 * @brief Native GLFW window, display, diagnostics and enabled input APIs.
 */

#include <cheryl/backends/native-glfw.h>
#include <cheryl/backends/native-glfw/diagnostics.h>

#if defined(CHERYL_NATIVE_INPUT) && CHERYL_NATIVE_INPUT
#include <cheryl/core/controls/input-system.h>
#endif
