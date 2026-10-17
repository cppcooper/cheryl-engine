#pragma once

// Every binary owns its exports; a module imports Engine while exporting itself.
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

// ELF RTTI needs public type visibility even when only selected methods export.
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
