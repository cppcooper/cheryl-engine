# vcpkg's hwloc port provides headers/libraries, not a CMake config package.
# Supply the explicit SDK target already supported by Cheryl's provider contract.
if(PROJECT_NAME STREQUAL "cheryl" AND CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC)
    find_path(cheryl_ci_hwloc_include NAMES hwloc.h REQUIRED)
    find_library(cheryl_ci_hwloc_library NAMES hwloc libhwloc REQUIRED)
    add_library(cheryl_ci_hwloc UNKNOWN IMPORTED GLOBAL)
    set_target_properties(cheryl_ci_hwloc PROPERTIES
        IMPORTED_LOCATION "${cheryl_ci_hwloc_library}"
        INTERFACE_INCLUDE_DIRECTORIES "${cheryl_ci_hwloc_include}")
    set(CHERYL_HWLOC_TARGET cheryl_ci_hwloc)
endif()
if(PROJECT_NAME STREQUAL "cheryl" AND CHERYL_BUILD_NATIVE_GLFW)
    # The pinned Gainput Windows adapter links hidapi even with its HID option off.
    find_package(hidapi CONFIG REQUIRED)
endif()
