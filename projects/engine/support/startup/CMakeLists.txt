# Compatibility source entrypoint; Startup is a public runtime module.
if(NOT TARGET Cheryl::Startup)
    add_subdirectory("${CHERYL_REPOSITORY_ROOT}/projects/modules/runtime/startup"
        "${CMAKE_CURRENT_BINARY_DIR}/module" EXCLUDE_FROM_ALL)
endif()
