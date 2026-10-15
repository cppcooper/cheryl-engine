# An explicitly selected source is configured only for Cheryl's custom backend.
# A supplied target bypasses this function and keeps all host dependency settings.
function(cheryl_add_tgui_source)
    if(NOT EXISTS "${CHERYL_TGUI_SOURCE}/CMakeLists.txt")
        message(FATAL_ERROR "CHERYL_TGUI_SOURCE must point to a TGUI 1.13.0 source checkout")
    endif()
    if(DEFINED TGUI_BACKEND AND NOT TGUI_BACKEND STREQUAL "Custom")
        message(FATAL_ERROR "Cheryl UI needs TGUI_BACKEND=Custom; use a dedicated dependency configuration")
    endif()
    if(DEFINED TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE AND NOT TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE)
        message(FATAL_ERROR "Cheryl UI needs TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE=ON")
    endif()
    set(TGUI_BACKEND Custom)
    set(TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE ON)
    set(native_features
        TGUI_HAS_BACKEND_SFML_GRAPHICS TGUI_HAS_BACKEND_SFML_OPENGL3
        TGUI_HAS_BACKEND_SDL_GPU TGUI_HAS_BACKEND_SDL_RENDERER
        TGUI_HAS_BACKEND_SDL_OPENGL3 TGUI_HAS_BACKEND_SDL_GLES2
        TGUI_HAS_BACKEND_SDL_TTF_OPENGL3 TGUI_HAS_BACKEND_SDL_TTF_GLES2
        TGUI_HAS_BACKEND_GLFW_OPENGL3 TGUI_HAS_BACKEND_GLFW_GLES2 TGUI_HAS_BACKEND_RAYLIB
        TGUI_CUSTOM_BACKEND_HAS_WINDOW_SFML TGUI_CUSTOM_BACKEND_HAS_WINDOW_SDL
        TGUI_CUSTOM_BACKEND_HAS_WINDOW_GLFW TGUI_CUSTOM_BACKEND_HAS_WINDOW_RAYLIB
        TGUI_CUSTOM_BACKEND_HAS_RENDERER_SFML_GRAPHICS TGUI_CUSTOM_BACKEND_HAS_RENDERER_SDL_GPU
        TGUI_CUSTOM_BACKEND_HAS_RENDERER_SDL_RENDERER TGUI_CUSTOM_BACKEND_HAS_RENDERER_OPENGL3
        TGUI_CUSTOM_BACKEND_HAS_RENDERER_GLES2 TGUI_CUSTOM_BACKEND_HAS_RENDERER_RAYLIB
        TGUI_CUSTOM_BACKEND_HAS_FONT_SFML_GRAPHICS TGUI_CUSTOM_BACKEND_HAS_FONT_SDL_TTF
        TGUI_CUSTOM_BACKEND_HAS_FONT_RAYLIB)
    foreach(feature IN LISTS native_features)
        if(DEFINED ${feature} AND ${feature})
            message(FATAL_ERROR "Cheryl UI requires ${feature}=OFF; supplied dependency settings are not rewritten")
        endif()
        set(${feature} OFF)
    endforeach()
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(TGUI_BUILD_GUI_BUILDER OFF)
    set(TGUI_BUILD_EXAMPLES OFF)
    set(TGUI_BUILD_TESTS OFF)
    set(TGUI_BUILD_DOC OFF)
    set(TGUI_INSTALL OFF)
    add_subdirectory("${CHERYL_TGUI_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/tgui" EXCLUDE_FROM_ALL)
    get_directory_property(tgui_version DIRECTORY "${CHERYL_TGUI_SOURCE}" DEFINITION TGUI_VERSION)
    if(NOT tgui_version STREQUAL "1.13.0")
        message(FATAL_ERROR "Cheryl UI requires TGUI 1.13.0; selected source reports ${tgui_version}")
    endif()
endfunction()
