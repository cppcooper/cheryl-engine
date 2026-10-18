#pragma once

/** @file
 * @brief Native GLFW display/window and enabled input APIs; link Cheryl::NativeGLFW.
 * CHERYL_NATIVE_INPUT gates input declarations; diagnostic Detail helpers are internal.
 * @see [Native GLFW setup](cheryl-guide:modules/native-glfw)
 */

#include <cheryl/backends/native-glfw.h>
#include <cheryl/backends/native-glfw/diagnostics.h>

#if defined(CHERYL_NATIVE_INPUT) && CHERYL_NATIVE_INPUT
#include <cheryl/core/controls/input-system.h>
#endif
