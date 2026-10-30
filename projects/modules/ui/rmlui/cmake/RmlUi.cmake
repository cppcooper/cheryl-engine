function(cheryl_fix_rmlui_placeholder)
    get_target_property(core_source rmlui_core SOURCE_DIR)
    set(widget_source "${core_source}/Elements/WidgetTextInput.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${widget_source}")
    file(READ "${widget_source}" widget)
    string(REPLACE "\r\n" "\n" widget "${widget}")
    set(entry [=[int WidgetTextInput::CalculateCharacterIndex(int line_index, float position)
{]=])
    set(guarded_entry [=[int WidgetTextInput::CalculateCharacterIndex(int line_index, float position)
{
	if (GetValue().empty())
		return 0;
]=])
    string(FIND "${widget}" "${guarded_entry}" guard_position)
    if(NOT guard_position EQUAL -1)
        return()
    endif()
    string(FIND "${widget}" "${entry}" entry_position)
    if(entry_position EQUAL -1)
        message(FATAL_ERROR "Cannot apply the RmlUi 6.3 placeholder hit-testing fix to ${widget_source}")
    endif()
    string(REPLACE "${entry}" "${guarded_entry}" widget "${widget}")

    # RmlUi 6.3 hit-tests placeholder-length lines against an empty value.
    # Compile a guarded copy without changing the dependency checkout.
    set(guarded_widget "${CMAKE_CURRENT_BINARY_DIR}/rmlui-fixes/WidgetTextInput.cpp")
    file(CONFIGURE OUTPUT "${guarded_widget}" CONTENT "${widget}" @ONLY)
    get_target_property(core_sources rmlui_core SOURCES)
    list(FIND core_sources "${widget_source}" source_index)
    if(source_index EQUAL -1)
        message(FATAL_ERROR "Cannot locate the RmlUi text input source in rmlui_core")
    endif()
    set_source_files_properties("${widget_source}" TARGET_DIRECTORY rmlui_core PROPERTIES
        HEADER_FILE_ONLY TRUE)
    target_sources(rmlui_core PRIVATE "${guarded_widget}")
    set_source_files_properties("${guarded_widget}" TARGET_DIRECTORY rmlui_core PROPERTIES
        INCLUDE_DIRECTORIES "${core_source}/Elements")
endfunction()

function(cheryl_add_rmlui_source)
    if(NOT EXISTS "${CHERYL_RMLUI_SOURCE}/CMakeLists.txt")
        message(FATAL_ERROR "CHERYL_RMLUI_SOURCE must point to a RmlUi 6.3 source checkout")
    endif()
    if(DEFINED RMLUI_FONT_ENGINE AND NOT RMLUI_FONT_ENGINE STREQUAL "none")
        message(FATAL_ERROR "Cheryl UI needs RMLUI_FONT_ENGINE=none; Engine owns font rasterization")
    endif()
    # Upstream creates this cache entry with a freetype default even when a
    # normal variable selects none. Seed the cache without replacing user input.
    set(RMLUI_FONT_ENGINE none CACHE STRING "Font engine to build RmlUi with")
    set(RMLUI_FONT_ENGINE none)
    set(optional_features RMLUI_SAMPLES RMLUI_TESTS RMLUI_SHELL RMLUI_LUA_BINDINGS
        RMLUI_LOTTIE_PLUGIN RMLUI_SVG_PLUGIN RMLUI_HARFBUZZ_SAMPLE
        RMLUI_IME_SAMPLE_USE_NOTO_FONTS RMLUI_TRACY_PROFILING RMLUI_CUSTOM_CONFIGURATION)
    foreach(feature IN LISTS optional_features)
        if(DEFINED ${feature} AND ${feature})
            message(FATAL_ERROR "Cheryl's owned RmlUi source requires ${feature}=OFF; supplied dependency settings are not rewritten")
        endif()
        set(${feature} OFF)
    endforeach()
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS ON)
    add_subdirectory("${CHERYL_RMLUI_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/rmlui" EXCLUDE_FROM_ALL)
    set(font_source "${CHERYL_RMLUI_SOURCE}/Source/Core/FontEngineDefault")
    foreach(source IN ITEMS FontEngineInterfaceDefault FontFace FontFaceHandleDefault FontFaceLayer FontFamily FontProvider)
        target_sources(rmlui_core PRIVATE "${font_source}/${source}.cpp")
    endforeach()
    target_sources(rmlui_core PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../private/font-provider.cpp")
    target_include_directories(rmlui_core PRIVATE "${font_source}")
    target_compile_features(rmlui_core PRIVATE cxx_std_17)
    set_target_properties(rmlui_core PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
        RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
        LIBRARY_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
        ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
    set_property(TARGET rmlui_core PROPERTY CHERYL_RMLUI_ENGINE_SERVICES TRUE)
    set_property(TARGET rmlui_core PROPERTY CHERYL_RMLUI_RASTERIZER_ABI 1)
    set_property(TARGET rmlui_core APPEND PROPERTY EXPORT_PROPERTIES
        CHERYL_RMLUI_ENGINE_SERVICES CHERYL_RMLUI_RASTERIZER_ABI CHERYL_RMLUI_PLACEHOLDER_FIX)
    set_property(TARGET rmlui_core PROPERTY CHERYL_OWNED_UI_DEPENDENCY TRUE)
    get_directory_property(rmlui_version DIRECTORY "${CHERYL_RMLUI_SOURCE}" DEFINITION PROJECT_VERSION)
    if(NOT rmlui_version STREQUAL "6.3")
        message(FATAL_ERROR "Cheryl UI requires RmlUi 6.3; selected source reports ${rmlui_version}")
    endif()
    cheryl_fix_rmlui_placeholder()
    set_property(TARGET rmlui_core PROPERTY CHERYL_RMLUI_PLACEHOLDER_FIX TRUE)
    set(RMLUI_FONT_ENGINE none PARENT_SCOPE)
endfunction()
