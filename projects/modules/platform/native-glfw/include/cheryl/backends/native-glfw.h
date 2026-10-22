#pragma once

/** @file
 * @brief GLFW display/window and conditionally enabled native input; link Cheryl::NativeGLFW.
 * InputSystem declarations require CHERYL_NATIVE_INPUT; graphics resources are supplied by a separate backend.
 * @see [Native module setup](cheryl-guide:modules/native-glfw)
 */
#include <core/display/window.h>
#include <core/display/display-system.h>
#include <core/controls/input-system.h>
