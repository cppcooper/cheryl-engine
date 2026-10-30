# Use with CMAKE_PROJECT_INCLUDE after preparing the shared dependency prefix.
# Existing application-provided targets take precedence over this convenience integration.
if(NOT PROJECT_NAME STREQUAL "cheryl" AND NOT PROJECT_NAME STREQUAL "cheryl_workers_module")
    return()
endif()
set(cheryl_prepared_prefix "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
if(NOT IS_DIRECTORY "${cheryl_prepared_prefix}/lib")
    message(FATAL_ERROR "The prepared dependency helper requires VCPKG_INSTALLED_DIR and VCPKG_TARGET_TRIPLET")
endif()

if(PROJECT_NAME STREQUAL "cheryl" AND WIN32 AND
        NOT TARGET harfbuzz::harfbuzz AND NOT TARGET harfbuzz)
    # The pinned port describes only the .lib as UNKNOWN. Supply its DLL pair
    # explicitly so linking and runtime deployment share the same information.
    block(SCOPE_FOR VARIABLES)
        find_path(cheryl_prepared_harfbuzz_include NAMES hb.h
            PATHS "${cheryl_prepared_prefix}/include/harfbuzz" NO_DEFAULT_PATH NO_CACHE REQUIRED)
        find_library(cheryl_prepared_harfbuzz_library NAMES harfbuzz
            PATHS "${cheryl_prepared_prefix}/lib" NO_DEFAULT_PATH NO_CACHE REQUIRED)
        find_file(cheryl_prepared_harfbuzz_runtime NAMES harfbuzz.dll
            PATHS "${cheryl_prepared_prefix}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
        find_library(cheryl_prepared_harfbuzz_debug_library NAMES harfbuzz
            PATHS "${cheryl_prepared_prefix}/debug/lib" NO_DEFAULT_PATH NO_CACHE)
        find_file(cheryl_prepared_harfbuzz_debug_runtime NAMES harfbuzz.dll
            PATHS "${cheryl_prepared_prefix}/debug/bin" NO_DEFAULT_PATH NO_CACHE)
        add_library(cheryl_prepared_harfbuzz SHARED IMPORTED GLOBAL)
        add_library(harfbuzz::harfbuzz ALIAS cheryl_prepared_harfbuzz)
        set_target_properties(cheryl_prepared_harfbuzz PROPERTIES
            IMPORTED_CONFIGURATIONS RELEASE
            IMPORTED_LOCATION "${cheryl_prepared_harfbuzz_runtime}"
            IMPORTED_LOCATION_RELEASE "${cheryl_prepared_harfbuzz_runtime}"
            IMPORTED_IMPLIB "${cheryl_prepared_harfbuzz_library}"
            IMPORTED_IMPLIB_RELEASE "${cheryl_prepared_harfbuzz_library}"
            INTERFACE_INCLUDE_DIRECTORIES "${cheryl_prepared_harfbuzz_include}")
        if(cheryl_prepared_harfbuzz_debug_library AND cheryl_prepared_harfbuzz_debug_runtime)
            set_property(TARGET cheryl_prepared_harfbuzz APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(cheryl_prepared_harfbuzz PROPERTIES
                IMPORTED_LOCATION_DEBUG "${cheryl_prepared_harfbuzz_debug_runtime}"
                IMPORTED_IMPLIB_DEBUG "${cheryl_prepared_harfbuzz_debug_library}")
        endif()
    endblock()
endif()

if((NOT DEFINED CHERYL_BUILD_WORKERS OR CHERYL_BUILD_WORKERS) AND
        (NOT DEFINED CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC OR CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC) AND
        NOT CHERYL_HWLOC_TARGET AND NOT TARGET cheryl_prepared_hwloc)
    set(cheryl_prepared_existing_hwloc OFF)
    foreach(candidate IN ITEMS hwloc::hwloc HWLOC::hwloc unofficial::hwloc::hwloc hwloc)
        if(TARGET "${candidate}")
            set(cheryl_prepared_existing_hwloc ON)
        endif()
    endforeach()
    if(NOT cheryl_prepared_existing_hwloc)
        block(SCOPE_FOR VARIABLES)
            find_path(cheryl_prepared_hwloc_include NAMES hwloc.h
                PATHS "${cheryl_prepared_prefix}/include" NO_DEFAULT_PATH NO_CACHE REQUIRED)
            find_library(cheryl_prepared_hwloc_library NAMES hwloc libhwloc
                PATHS "${cheryl_prepared_prefix}/lib" NO_DEFAULT_PATH NO_CACHE REQUIRED)
            find_library(cheryl_prepared_hwloc_debug_library NAMES hwloc libhwloc
                PATHS "${cheryl_prepared_prefix}/debug/lib" NO_DEFAULT_PATH NO_CACHE)
            set(cheryl_prepared_hwloc_runtime "${cheryl_prepared_hwloc_library}")
            set(cheryl_prepared_hwloc_debug_runtime "${cheryl_prepared_hwloc_debug_library}")
            if(WIN32)
                unset(cheryl_prepared_hwloc_runtime)
                unset(cheryl_prepared_hwloc_debug_runtime)
                find_file(cheryl_prepared_hwloc_runtime NAMES hwloc.dll hwloc-15.dll libhwloc-15.dll libhwloc.dll
                    PATHS "${cheryl_prepared_prefix}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
                find_file(cheryl_prepared_hwloc_debug_runtime NAMES hwloc.dll hwloc-15.dll libhwloc-15.dll libhwloc.dll
                    PATHS "${cheryl_prepared_prefix}/debug/bin" NO_DEFAULT_PATH NO_CACHE)
            endif()
            add_library(cheryl_prepared_hwloc SHARED IMPORTED GLOBAL)
            set_target_properties(cheryl_prepared_hwloc PROPERTIES
                IMPORTED_CONFIGURATIONS RELEASE
                IMPORTED_LOCATION "${cheryl_prepared_hwloc_runtime}"
                IMPORTED_LOCATION_RELEASE "${cheryl_prepared_hwloc_runtime}"
                INTERFACE_INCLUDE_DIRECTORIES "${cheryl_prepared_hwloc_include}")
            if(WIN32)
                set_target_properties(cheryl_prepared_hwloc PROPERTIES
                    IMPORTED_IMPLIB "${cheryl_prepared_hwloc_library}"
                    IMPORTED_IMPLIB_RELEASE "${cheryl_prepared_hwloc_library}")
            endif()
            if(cheryl_prepared_hwloc_debug_library AND cheryl_prepared_hwloc_debug_runtime)
                set_property(TARGET cheryl_prepared_hwloc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
                set_property(TARGET cheryl_prepared_hwloc PROPERTY IMPORTED_LOCATION_DEBUG "${cheryl_prepared_hwloc_debug_runtime}")
                if(WIN32)
                    set_property(TARGET cheryl_prepared_hwloc PROPERTY IMPORTED_IMPLIB_DEBUG "${cheryl_prepared_hwloc_debug_library}")
                endif()
            endif()
        endblock()
        set(CHERYL_HWLOC_TARGET cheryl_prepared_hwloc)
    endif()
endif()

if(PROJECT_NAME STREQUAL "cheryl" AND WIN32 AND (NOT DEFINED CHERYL_BUILD_NATIVE_GLFW OR CHERYL_BUILD_NATIVE_GLFW) AND
        (NOT DEFINED CHERYL_NATIVE_INPUT OR CHERYL_NATIVE_INPUT) AND NOT TARGET hidapi::hidapi)
    # Gainput forces native HID on for Windows. Its later FetchContent request
    # reuses this declaration and the prepared package rather than downloading.
    include(FetchContent)
    FetchContent_Declare(hidapi FIND_PACKAGE_ARGS CONFIG REQUIRED)
    FetchContent_MakeAvailable(hidapi)
endif()
