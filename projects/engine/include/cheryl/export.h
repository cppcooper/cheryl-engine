#pragma once

/** @file
 * @brief Visibility decorators owned by each Cheryl binary's CMake target.
 * Windows shared producers export and consumers import; GNU/Clang shared symbols
 * use default visibility. A component's static selection makes its decorator empty.
 * Use the exported CMake targets; application code does not set BUILDING_* flags
 * or guess which other binaries a module exports. A module imports Engine while
 * exporting only its own API.
 * @see [Module linkage and exports](cheryl-guide:development/modules)
 */
#if defined(_WIN32)
#define CE_DETAIL_EXPORT __declspec(dllexport)
#define CE_DETAIL_IMPORT __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#define CE_DETAIL_EXPORT __attribute__((visibility("default")))
#define CE_DETAIL_IMPORT __attribute__((visibility("default")))
#else
#define CE_DETAIL_EXPORT
#define CE_DETAIL_IMPORT
#endif

/** Gives public types default ELF visibility so cross-library RTTI remains usable.
 * Empty on Windows; member decorators still select the owning binary's import/export.
 * Module authors use this for public types whose selected methods export separately.
 */
#if !defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
#define CE_TYPE_API __attribute__((visibility("default")))
#else
#define CE_TYPE_API
#endif

#if defined(CE_ENGINE_STATIC)
#define CE_API
#elif defined(CE_BUILDING_ENGINE)
#define CE_API CE_DETAIL_EXPORT
#else
#define CE_API CE_DETAIL_IMPORT
#endif

#if defined(CE_NATIVE_GLFW_STATIC)
#define CE_NATIVE_GLFW_API
#elif defined(CE_BUILDING_NATIVE_GLFW)
#define CE_NATIVE_GLFW_API CE_DETAIL_EXPORT
#else
#define CE_NATIVE_GLFW_API CE_DETAIL_IMPORT
#endif

#if defined(CE_OPENGL_STATIC)
#define CE_OPENGL_API
#elif defined(CE_BUILDING_OPENGL)
#define CE_OPENGL_API CE_DETAIL_EXPORT
#else
#define CE_OPENGL_API CE_DETAIL_IMPORT
#endif

#if defined(CE_UI_TGUI_STATIC)
#define CE_UI_TGUI_API
#elif defined(CE_BUILDING_UI_TGUI)
#define CE_UI_TGUI_API CE_DETAIL_EXPORT
#else
#define CE_UI_TGUI_API CE_DETAIL_IMPORT
#endif

#if defined(CE_UI_RMLUI_STATIC)
#define CE_UI_RMLUI_API
#elif defined(CE_BUILDING_UI_RMLUI)
#define CE_UI_RMLUI_API CE_DETAIL_EXPORT
#else
#define CE_UI_RMLUI_API CE_DETAIL_IMPORT
#endif

#if defined(CE_AUDIO_MINIAUDIO_STATIC)
#define CE_AUDIO_MINIAUDIO_API
#elif defined(CE_BUILDING_AUDIO_MINIAUDIO)
#define CE_AUDIO_MINIAUDIO_API CE_DETAIL_EXPORT
#else
#define CE_AUDIO_MINIAUDIO_API CE_DETAIL_IMPORT
#endif

#if defined(CE_STARTUP_STATIC)
#define CE_STARTUP_API
#elif defined(CE_BUILDING_STARTUP)
#define CE_STARTUP_API CE_DETAIL_EXPORT
#else
#define CE_STARTUP_API CE_DETAIL_IMPORT
#endif

#if defined(CE_OPENGL_STARTUP_STATIC)
#define CE_OPENGL_STARTUP_API
#elif defined(CE_BUILDING_OPENGL_STARTUP)
#define CE_OPENGL_STARTUP_API CE_DETAIL_EXPORT
#else
#define CE_OPENGL_STARTUP_API CE_DETAIL_IMPORT
#endif

#if defined(CE_DEBUG_TERMINAL_STATIC)
#define CE_DEBUG_TERMINAL_API
#elif defined(CE_BUILDING_DEBUG_TERMINAL)
#define CE_DEBUG_TERMINAL_API CE_DETAIL_EXPORT
#else
#define CE_DEBUG_TERMINAL_API CE_DETAIL_IMPORT
#endif

#if defined(CE_WORKERS_STATIC)
#define CE_WORKERS_API
#elif defined(CE_BUILDING_WORKERS)
#define CE_WORKERS_API CE_DETAIL_EXPORT
#else
#define CE_WORKERS_API CE_DETAIL_IMPORT
#endif

#define CE_WORKER_TOPOLOGY_HWLOC_API CE_WORKERS_API
#define CE_WORKER_TOPOLOGY_JSON_API CE_WORKERS_API

/** @def CE_API
 * @brief Declares symbols owned by Cheryl::Engine.
 * CE_ENGINE_STATIC selects no decoration; CE_BUILDING_ENGINE exports a shared producer;
 * otherwise a shared consumer imports it. The owning linked CMake target supplies
 * these definitions consistently; applications do not set them per include.
 * @see [Module linkage](cheryl-guide:development/modules)
 */

/** @def CE_NATIVE_GLFW_API
 * @brief Declares symbols owned by Cheryl::NativeGLFW.
 * CE_NATIVE_GLFW_STATIC selects no decoration; CE_BUILDING_NATIVE_GLFW exports a shared producer;
 * otherwise a shared consumer imports it. The owning linked CMake target supplies
 * these definitions consistently; applications do not set them per include.
 * @see [Module linkage](cheryl-guide:development/modules)
 */

/** @def CE_OPENGL_API
 * @brief Declares symbols owned by Cheryl::OpenGL.
 * CE_OPENGL_STATIC selects no decoration; CE_BUILDING_OPENGL exports a shared producer;
 * otherwise a shared consumer imports it. The owning linked CMake target supplies
 * these definitions consistently; applications do not set them per include.
 * @see [Module linkage](cheryl-guide:development/modules)
 */

/** @def CE_UI_TGUI_API
 * @brief Declares symbols owned by Cheryl::UI::TGUI.
 * CE_UI_TGUI_STATIC selects no decoration; CE_BUILDING_UI_TGUI exports a shared producer;
 * otherwise a shared consumer imports it. The owning linked CMake target supplies
 * these definitions consistently; applications do not set them per include.
 * @see [Module linkage](cheryl-guide:development/modules)
 */

/** @def CE_UI_RMLUI_API
 * @brief Declares symbols owned by Cheryl::UI::RmlUi.
 * CE_UI_RMLUI_STATIC selects no decoration; CE_BUILDING_UI_RMLUI exports a shared producer;
 * otherwise a shared consumer imports it. The owning linked CMake target supplies
 * these definitions consistently; applications do not set them per include.
 * @see [Module linkage](cheryl-guide:development/modules)
 */

/** @def CE_AUDIO_MINIAUDIO_API
 * @brief Declares symbols owned by Cheryl::Audio::Miniaudio.
 * CE_AUDIO_MINIAUDIO_STATIC selects no decoration; CE_BUILDING_AUDIO_MINIAUDIO exports a shared producer;
 * otherwise a shared consumer imports it. The owning linked CMake target supplies
 * these definitions consistently; applications do not set them per include.
 * @see [Module linkage](cheryl-guide:development/modules)
 */

/** @def CE_STARTUP_API
 * @brief Declares symbols owned by Cheryl::Startup.
 * CE_STARTUP_STATIC selects no decoration; CE_BUILDING_STARTUP exports a shared producer;
 * otherwise a shared consumer imports it. The owning linked CMake target supplies
 * these definitions consistently; applications do not set them per include.
 * @see [Module linkage](cheryl-guide:development/modules)
 */

/** @def CE_OPENGL_STARTUP_API
 * @brief Compatibility visibility decorator for the former OpenGL Startup owner.
 * CE_OPENGL_STARTUP_STATIC makes it empty; CE_BUILDING_OPENGL_STARTUP exports;
 * other uses import. Canonical OpenGL startup now belongs to Cheryl::Startup;
 * this retained decorator does not select or create a separate startup binary.
 * @see [Startup compatibility](cheryl-guide:modules/startup#compatibility-and-further-reading)
 */

/** @def CE_DEBUG_TERMINAL_API
 * @brief Declares symbols owned by Cheryl::DebugTerminal.
 * CE_DEBUG_TERMINAL_STATIC selects no decoration; CE_BUILDING_DEBUG_TERMINAL exports a shared producer;
 * otherwise a shared consumer imports it. The owning linked CMake target supplies
 * these definitions consistently; applications do not set them per include.
 * @see [Module linkage](cheryl-guide:development/modules)
 */

/** @def CE_WORKERS_API
 * @brief Declares symbols owned by Cheryl::Workers.
 * CE_WORKERS_STATIC selects no decoration; CE_BUILDING_WORKERS exports a shared producer;
 * otherwise a shared consumer imports it. The owning linked CMake target supplies
 * these definitions consistently; applications do not set them per include.
 * @see [Module linkage](cheryl-guide:development/modules)
 */

/** @def CE_WORKER_TOPOLOGY_HWLOC_API
 * @brief Workers-owned visibility for the hwloc topology provider interface.
 * Aliases CE_WORKERS_API: these APIs share the Workers binary rather than
 * introducing separate provider/configuration import libraries.
 * @see [Workers module](cheryl-guide:modules/workers)
 */

/** @def CE_WORKER_TOPOLOGY_JSON_API
 * @brief Workers-owned visibility for the JSON worker configuration interface.
 * Aliases CE_WORKERS_API: these APIs share the Workers binary rather than
 * introducing separate provider/configuration import libraries.
 * @see [Workers module](cheryl-guide:modules/workers)
 */
