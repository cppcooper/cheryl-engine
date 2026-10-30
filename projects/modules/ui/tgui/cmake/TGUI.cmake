# An explicitly selected source is configured only for Cheryl's custom backend.
# A supplied target bypasses this function and keeps all host dependency settings.
function(cheryl_add_tgui_source)
    if(NOT EXISTS "${CHERYL_TGUI_SOURCE}/CMakeLists.txt")
        message(FATAL_ERROR "CHERYL_TGUI_SOURCE must point to a TGUI 1.13.0 source checkout")
    endif()
    if(DEFINED TGUI_BACKEND AND NOT TGUI_BACKEND STREQUAL "Custom")
        message(FATAL_ERROR "Cheryl UI needs TGUI_BACKEND=Custom; use a dedicated dependency configuration")
    endif()
    if(DEFINED TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE AND TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE)
        message(FATAL_ERROR "Cheryl UI needs TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE=OFF; Engine owns font rasterization")
    endif()
    set(TGUI_BACKEND Custom)
    set(TGUI_CUSTOM_BACKEND_HAS_FONT_FREETYPE OFF)
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
    if(DEFINED TGUI_SHARED_LIBS AND NOT TGUI_SHARED_LIBS)
        message(FATAL_ERROR "Cheryl UI requires a shared TGUI runtime; remove TGUI_SHARED_LIBS=OFF")
    endif()
    set(TGUI_SHARED_LIBS ON)
    add_subdirectory("${CHERYL_TGUI_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/tgui" EXCLUDE_FROM_ALL)
    # Engine provides image decoding; do not compile TGUI's bundled stb decoder.
    get_target_property(tgui_sources tgui SOURCES)
    list(FILTER tgui_sources EXCLUDE REGEX "(^|/)Loading/ImageLoader\\.cpp$")
    set_property(TARGET tgui PROPERTY SOURCES "${tgui_sources}")
    target_sources(tgui PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../private/image-loader.cpp")
    set_target_properties(tgui PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
        LIBRARY_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
        ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
    set_property(TARGET tgui PROPERTY CHERYL_TGUI_ENGINE_SERVICES TRUE)
    set_property(TARGET tgui PROPERTY CHERYL_TGUI_IMAGE_DECODER_ABI 1)
    set_property(TARGET tgui APPEND PROPERTY EXPORT_PROPERTIES
        CHERYL_TGUI_ENGINE_SERVICES CHERYL_TGUI_IMAGE_DECODER_ABI)
    set_property(TARGET tgui PROPERTY CHERYL_OWNED_UI_DEPENDENCY TRUE)
    get_directory_property(tgui_version DIRECTORY "${CHERYL_TGUI_SOURCE}" DEFINITION TGUI_VERSION)
    if(NOT tgui_version STREQUAL "1.13.0")
        message(FATAL_ERROR "Cheryl UI requires TGUI 1.13.0; selected source reports ${tgui_version}")
    endif()
endfunction()
