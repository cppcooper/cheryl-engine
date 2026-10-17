# Compatibility source entrypoint. Configured OpenGL integration is owned by Startup.
if(NOT TARGET Cheryl::Startup)
    add_subdirectory("${CHERYL_REPOSITORY_ROOT}/projects/modules/runtime/startup"
        "${CMAKE_CURRENT_BINARY_DIR}/startup" EXCLUDE_FROM_ALL)
endif()
