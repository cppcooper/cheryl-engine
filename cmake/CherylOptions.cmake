include_guard(GLOBAL)

function(cheryl_option name description default_value)
    cmake_parse_arguments(configured "" "" "REQUIRES" ${ARGN})
    option(${name} "${description}" "${default_value}")

    # Root and module entry points can declare the same option with different
    # defaults. Report it once, against the first declaration's default.
    get_property(declared_options GLOBAL PROPERTY CHERYL_DECLARED_OPTIONS)
    if(name IN_LIST declared_options)
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY CHERYL_DECLARED_OPTIONS "${name}")
    # Preserve the entry point's value through scoped Engine bootstrapping.
    set_property(GLOBAL PROPERTY "CHERYL_OPTION_${name}_VALUE" "${${name}}")
    set_property(GLOBAL PROPERTY "CHERYL_OPTION_${name}_REQUIRES" "${configured_REQUIRES}")

    if((${name} AND NOT default_value) OR (NOT ${name} AND default_value))
        message(STATUS "Configured Cheryl option: ${name}")
    endif()
endfunction()

function(cheryl_check_option_requirements)
    get_property(declared_options GLOBAL PROPERTY CHERYL_DECLARED_OPTIONS)
    foreach(name IN LISTS declared_options)
        get_property(value GLOBAL PROPERTY "CHERYL_OPTION_${name}_VALUE")
        if(NOT value)
            continue()
        endif()

        get_property(required_options GLOBAL PROPERTY "CHERYL_OPTION_${name}_REQUIRES")
        set(missing_options)
        foreach(required_option IN LISTS required_options)
            if(required_option IN_LIST declared_options)
                get_property(required_value GLOBAL PROPERTY "CHERYL_OPTION_${required_option}_VALUE")
            else()
                set(required_value "${${required_option}}")
            endif()
            if(NOT required_value)
                list(APPEND missing_options "${required_option}")
            endif()
        endforeach()
        if(missing_options)
            list(JOIN missing_options ", " missing_options_text)
            message(WARNING
                    "${name} is enabled, but requires these options to be enabled: ${missing_options_text}. "
                    "Enable them or set ${name}=OFF.")
        endif()
    endforeach()
endfunction()

function(cheryl_configure_root_options)
    cheryl_option(WARN
            "Enable compiler warnings [-Wall]" OFF)

    cheryl_option(CHERYL_BUILD_NATIVE_GLFW
            "Build the native GLFW window/input integration" ON)
    cheryl_option(CHERYL_SANDBOX_BUILD
            "Deprecated native GLFW null-platform selection without Gainput" OFF)
    cheryl_option(CHERYL_BUILD_OPENGL
            "Build the OpenGL backend" ON)
    cheryl_option(CHERYL_BUILD_UI_TGUI
            "Build the optional TGUI UI integration" ON)
    cheryl_option(CHERYL_BUILD_UI_RMLUI
            "Build the optional RmlUi UI integration" ON)
    cheryl_option(CHERYL_BUILD_AUDIO_MINIAUDIO
            "Build the optional miniaudio playback integration" OFF)

    cheryl_option(CHERYL_BUILD_DEMO
            "Build the windowed demonstration application" ON
            REQUIRES CHERYL_BUILD_NATIVE_GLFW CHERYL_BUILD_OPENGL CHERYL_NATIVE_INPUT)

    cheryl_option(CHERYL_BUILD_TESTS
            "Build Cheryl's tests and acceptance tools" ON)
    cheryl_option(CHERYL_BUILD_ALL_TESTS
            "Include owner and cross-project aggregate test runners in the default build" OFF
            REQUIRES CHERYL_BUILD_TESTS)
    cheryl_option(CHERYL_BUILD_ACCEPTANCE_TESTS
            "Include broader acceptance runners in the default build" OFF
            REQUIRES CHERYL_BUILD_TESTS)
    cheryl_option(CHERYL_BUILD_CONSUMER_TESTS
            "Build the consumer and public-header checks" OFF)

    if(CHERYL_BUILD_OPENGL AND NOT CHERYL_BUILD_NATIVE_GLFW)
        message(FATAL_ERROR "Cheryl OpenGL requires Native GLFW; enable CHERYL_BUILD_NATIVE_GLFW or disable CHERYL_BUILD_OPENGL")
    endif()
endfunction()
