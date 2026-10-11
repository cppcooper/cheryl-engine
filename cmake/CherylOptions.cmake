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
        set_property(GLOBAL APPEND PROPERTY CHERYL_CONFIGURED_OPTIONS "${name}")
    else()
        set_property(GLOBAL APPEND PROPERTY CHERYL_DEFAULT_OPTIONS "${name}")
    endif()
endfunction()

function(cheryl_print_options)
    set(color_enabled OFF)
    if(NOT "$ENV{NO_COLOR}" STREQUAL "" AND NOT "$ENV{NO_COLOR}" STREQUAL "0")
        set(color_enabled OFF)
    elseif(NOT "$ENV{CLICOLOR_FORCE}" STREQUAL "" AND NOT "$ENV{CLICOLOR_FORCE}" STREQUAL "0")
        set(color_enabled ON)
    elseif(NOT "$ENV{CLICOLOR}" STREQUAL "0" AND
            NOT "$ENV{TERM}" STREQUAL "" AND NOT "$ENV{TERM}" STREQUAL "dumb")
        set(color_enabled ON)
    endif()
    set(green "")
    set(red "")
    set(reset "")
    if(color_enabled)
        string(ASCII 27 escape)
        set(green "${escape}[32m")
        set(red "${escape}[31m")
        set(reset "${escape}[0m")
    endif()

    message(STATUS "Cheryl CMake options configuration")
    message(STATUS "----------------------------------")
    foreach(group IN ITEMS CONFIGURED DEFAULT)
        get_property(options GLOBAL PROPERTY "CHERYL_${group}_OPTIONS")
        list(SORT options)
        set(on_options)
        set(off_options)
        foreach(name IN LISTS options)
            get_property(value GLOBAL PROPERTY "CHERYL_OPTION_${name}_VALUE")
            if(value)
                list(APPEND on_options "${name}")
            else()
                list(APPEND off_options "${name}")
            endif()
        endforeach()
        string(TOLOWER "${group}" label)
        message(STATUS "${label}:")
        foreach(name IN LISTS on_options off_options)
            get_property(value GLOBAL PROPERTY "CHERYL_OPTION_${name}_VALUE")
            if(value)
                set(color "${green}")
            else()
                set(color "${red}")
            endif()
            message(STATUS "  ${color}[${value}]${reset}\t${name}")
        endforeach()
    endforeach()
    message(STATUS "----------------------------------")
endfunction()

# Include options declared by modules and standalone entry points in one summary.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL cheryl_print_options)

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
    set(cheryl_terminal_linux_default OFF)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(cheryl_terminal_linux_default ON)
    endif()
    cheryl_option(CHERYL_BUILD_DEBUG_TERMINAL_LINUX
            "Select the Linux Debug terminal module for applications and test runners" ${cheryl_terminal_linux_default})
    cheryl_option(CHERYL_SANDBOX_BUILD
            "Deprecated native GLFW null-platform selection without Gainput" OFF)
    cheryl_option(CHERYL_BUILD_OPENGL
            "Build the OpenGL backend" ON)
    cheryl_option(CHERYL_BUILD_UI_TGUI
            "Build the optional TGUI UI integration" ON)
    cheryl_option(CHERYL_BUILD_UI_RMLUI
            "Build the optional RmlUi UI integration" OFF)
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
        cheryl_print_options()
        message(FATAL_ERROR "Cheryl OpenGL requires Native GLFW; enable CHERYL_BUILD_NATIVE_GLFW or disable CHERYL_BUILD_OPENGL")
    endif()
endfunction()

function(cheryl_debug_terminal_configuration result)
    set(CHERYL_DEBUG_TERMINAL "AUTO" CACHE STRING "Native output terminal inclusion: AUTO (Debug only), ON, or OFF")
    set_property(CACHE CHERYL_DEBUG_TERMINAL PROPERTY STRINGS AUTO ON OFF)
    if(CHERYL_DEBUG_TERMINAL STREQUAL "AUTO")
        set(${result} "$<CONFIG:Debug>" PARENT_SCOPE)
    elseif(CHERYL_DEBUG_TERMINAL STREQUAL "ON")
        set(${result} "1" PARENT_SCOPE)
    elseif(CHERYL_DEBUG_TERMINAL STREQUAL "OFF")
        set(${result} "0" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown CHERYL_DEBUG_TERMINAL: ${CHERYL_DEBUG_TERMINAL}")
    endif()
endfunction()
