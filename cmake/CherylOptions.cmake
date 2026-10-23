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
    get_property(summary_scheduled GLOBAL PROPERTY CHERYL_OPTIONS_SUMMARY_SCHEDULED)
    if(NOT summary_scheduled)
        cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL cheryl_print_options)
        set_property(GLOBAL PROPERTY CHERYL_OPTIONS_SUMMARY_SCHEDULED TRUE)
    endif()
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

# Entry points declare only the groups they consume; all defaults live here.
function(cheryl_configure_module_options)
    cheryl_option(CHERYL_BUILD_TESTS "Build module-local checks" OFF)
endfunction()

function(cheryl_configure_native_options)
    cheryl_option(CHERYL_NATIVE_INPUT "Include the Gainput-backed native input adapter" ON)
    cheryl_option(CHERYL_NATIVE_NULL_PLATFORM "Use GLFW's null platform without X11 or Wayland" OFF)
endfunction()

function(cheryl_configure_opengl_options)
    set(CHERYL_NATIVE_GLFW_SOURCE "" CACHE PATH "Native GLFW module source directory when its target is not supplied")
endfunction()

function(cheryl_configure_startup_options)
    set(opengl_default "${CHERYL_BUILD_OPENGL}")
    if(TARGET Cheryl::OpenGL)
        set(opengl_default ON)
    endif()
    cheryl_option(CHERYL_STARTUP_OPENGL "Include private GLFW/OpenGL integration in Startup" "${opengl_default}")
    set(CHERYL_OPENGL_SOURCE "" CACHE PATH "OpenGL module source directory when its target is not supplied")
endfunction()

function(cheryl_configure_tgui_options)
    set(CHERYL_TGUI_SOURCE "" CACHE PATH "TGUI 1.13.0 source directory; empty selects the pinned source or a supplied package")
endfunction()

function(cheryl_configure_rmlui_options)
    set(CHERYL_RMLUI_SOURCE "" CACHE PATH "RmlUi 6.3 source directory; empty selects the pinned source or a supplied package")
endfunction()

function(cheryl_configure_rmlui_test_options)
    set(CHERYL_RMLUI_TEST_FONT "" CACHE FILEPATH "RmlUi check font fixture; empty discovers an available TTF/OTF file")
endfunction()

function(cheryl_configure_miniaudio_options)
    set(CHERYL_MINIAUDIO_SOURCE "" CACHE PATH "Miniaudio 0.11.25 source directory, overriding the pinned submodule")
endfunction()

function(cheryl_configure_debug_terminal_options)
    set(CHERYL_DEBUG_TERMINAL_EMULATOR "auto" CACHE STRING "Linux native terminal emulator: auto, konsole, or xterm")
    set_property(CACHE CHERYL_DEBUG_TERMINAL_EMULATOR PROPERTY STRINGS auto konsole xterm)
    if(NOT CHERYL_DEBUG_TERMINAL_EMULATOR MATCHES "^(auto|konsole|xterm)$")
        message(FATAL_ERROR "Unknown CHERYL_DEBUG_TERMINAL_EMULATOR: ${CHERYL_DEBUG_TERMINAL_EMULATOR}")
    endif()
endfunction()

function(cheryl_configure_logging_options)
    set(CHERYL_LOG_PROFILE "auto" CACHE STRING "Logging defaults: auto, developer, support, or release")
    set_property(CACHE CHERYL_LOG_PROFILE PROPERTY STRINGS auto developer support release)
    set(CHERYL_LOG_COMPILED_MASK "" CACHE STRING "Optional severity-bit override (0x3f trace, 0x1f debug, 0x0f info, 0x07 warn, 0 off)")
endfunction()

function(cheryl_configure_hwloc_options)
    set(CHERYL_HWLOC_TARGET "" CACHE STRING "Supplied hwloc 2.5+ (2.x) SDK target")
endfunction()

function(cheryl_configure_documentation_options interpreter)
    set(CHERYL_DOCS_PYTHON "${interpreter}" CACHE FILEPATH "Python interpreter for documentation tool selection")
    set(CHERYL_DOCS_CACHE_DIR "" CACHE PATH "Shared documentation tool cache; empty uses the user cache")
    cheryl_option(CHERYL_DOCS_FORCE_PINNED "Use locked documentation tools instead of compatible installed tools" OFF)
endfunction()

function(cheryl_configure_first_game_options)
    set(CHERYL_FIRST_GAME_ENGINE_SOURCE "" CACHE PATH "Cheryl checkout for source consumption; empty uses an installed SDK")
endfunction()

function(cheryl_configure_library_options)
    cheryl_option(CHERYL_BUILD_SHARED_LIBS
            "Build Cheryl Engine, modules and support as shared libraries" ON)
endfunction()

function(cheryl_configure_root_options)
    cheryl_configure_library_options()
    cheryl_option(CHERYL_BUILD_DOCS
            "Add documentation tooling and generation targets" ON)
    cheryl_option(CHERYL_BUILD_SDK
            "Enable installation of the selected libraries, headers and dependencies" ON)
    cheryl_option(WARN
            "Enable compiler warnings [-Wall]" OFF)

    cheryl_option(CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC
            "Build optional hwloc worker topology and thread placement support" ON)
    cheryl_option(CHERYL_BUILD_WORKER_TOPOLOGY_JSON
            "Build optional worker configuration JSON loading support" ON)
    cheryl_option(CHERYL_BUILD_WORKERS
            "Build the optional worker configuration/topology module" ON)
    cheryl_option(CHERYL_BUILD_STARTUP
            "Build the application bootstrap module" ON)

    cheryl_option(CHERYL_BUILD_NATIVE_GLFW
            "Build the native GLFW window/input integration" ON)
    cheryl_option(CHERYL_BUILD_NATIVE_SDL3
            "Add the Native SDL3 scaffold (runtime adapter is not implemented)" OFF)
    set(cheryl_terminal_linux_default OFF)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND
            NOT CMAKE_CONFIGURATION_TYPES AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(cheryl_terminal_linux_default ON)
    endif()
    cheryl_option(CHERYL_BUILD_DEBUG_TERMINAL_LINUX
            "Include the Linux output terminal in applications and test runners (defaults ON in supported Debug builds)" ${cheryl_terminal_linux_default})
    cheryl_option(CHERYL_BUILD_OPENGL
            "Build the OpenGL backend" ON)
    cheryl_configure_startup_options()
    cheryl_option(CHERYL_BUILD_UI_TGUI
            "Build the optional TGUI UI integration" ON)
    cheryl_option(CHERYL_BUILD_UI_RMLUI
            "Build the optional RmlUi UI integration" ON)
    cheryl_option(CHERYL_BUILD_AUDIO_MINIAUDIO
            "Build the optional miniaudio playback integration" ON)

    cheryl_option(CHERYL_BUILD_DEMO
            "Build the windowed demonstration application" OFF
            REQUIRES CHERYL_BUILD_NATIVE_GLFW CHERYL_BUILD_OPENGL CHERYL_NATIVE_INPUT)
    cheryl_option(CHERYL_BUILD_FIRST_GAME
            "Build the maintained getting-started game" OFF
            REQUIRES CHERYL_BUILD_NATIVE_GLFW CHERYL_BUILD_OPENGL CHERYL_NATIVE_INPUT)
    cheryl_option(CHERYL_BUILD_TUTORIAL_GAME
            "Build the Player/Enemy/HUD and How-to teaching game" OFF
            REQUIRES CHERYL_BUILD_STARTUP CHERYL_STARTUP_OPENGL CHERYL_BUILD_UI_TGUI CHERYL_NATIVE_INPUT)
    cheryl_option(CHERYL_BUILD_DIALOGUE_CONSOLE
            "Build the headless dialogue encounter and paired save consumer" OFF)

    cheryl_option(CHERYL_BUILD_TESTS
            "Build Cheryl's tests and acceptance tools" OFF)
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
