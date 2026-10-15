#pragma once

#include <TGUI/Config.hpp>

#if TGUI_VERSION_MAJOR != 1 || TGUI_VERSION_MINOR != 13 || TGUI_VERSION_PATCH != 0
#error Cheryl UI requires the reviewed TGUI 1.13.0 backend contract.
#endif

#if !TGUI_HAS_FONT_BACKEND_FREETYPE
#error Cheryl UI requires TGUI custom backend with the FreeType font backend.
#endif

#if TGUI_HAS_WINDOW_BACKEND_SFML || TGUI_HAS_WINDOW_BACKEND_SDL || TGUI_HAS_WINDOW_BACKEND_GLFW || TGUI_HAS_WINDOW_BACKEND_RAYLIB ||       \
    TGUI_HAS_RENDERER_BACKEND_SFML_GRAPHICS || TGUI_HAS_RENDERER_BACKEND_SDL_GPU || TGUI_HAS_RENDERER_BACKEND_SDL_RENDERER ||              \
    TGUI_HAS_RENDERER_BACKEND_OPENGL3 || TGUI_HAS_RENDERER_BACKEND_GLES2 || TGUI_HAS_RENDERER_BACKEND_RAYLIB ||                            \
    TGUI_HAS_FONT_BACKEND_SFML_GRAPHICS || TGUI_HAS_FONT_BACKEND_SDL_TTF || TGUI_HAS_FONT_BACKEND_RAYLIB
#error Cheryl UI requires TGUI custom backend without native window or renderer backends.
#endif
