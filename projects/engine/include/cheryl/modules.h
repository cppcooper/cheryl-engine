#pragma once

/** @file
 * @brief Public module APIs selected by linked Cheryl targets.
 */

#if defined(CHERYL_HAS_DEBUG_TERMINAL_LINUX) && CHERYL_HAS_DEBUG_TERMINAL_LINUX
#include <cheryl/modules/debug-terminal-linux.h>
#endif

#if defined(CHERYL_HAS_NATIVE_GLFW) && CHERYL_HAS_NATIVE_GLFW
#include <cheryl/modules/native-glfw.h>
#endif

#if defined(CHERYL_HAS_OPENGL) && CHERYL_HAS_OPENGL
#include <cheryl/modules/opengl.h>
#endif

#if defined(CHERYL_HAS_UI_TGUI) && CHERYL_HAS_UI_TGUI
#include <cheryl/modules/tgui.h>
#endif

#if defined(CHERYL_HAS_UI_RMLUI) && CHERYL_HAS_UI_RMLUI
#include <cheryl/modules/rmlui.h>
#endif

#if defined(CHERYL_HAS_AUDIO_MINIAUDIO) && CHERYL_HAS_AUDIO_MINIAUDIO
#include <cheryl/modules/miniaudio.h>
#endif
