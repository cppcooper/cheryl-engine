if(TARGET Cheryl::OpenGL::Startup)
    return()
endif()

if(NOT TARGET Cheryl::Startup)
    add_subdirectory("${CHERYL_REPOSITORY_ROOT}/projects/engine/support/startup" engine-startup EXCLUDE_FROM_ALL)
endif()

add_library(${TARGET_LIB_MODULE_OPENGL_S_STARTUP} STATIC src/startup.cpp)
add_library(Cheryl::OpenGL::Startup ALIAS ${TARGET_LIB_MODULE_OPENGL_S_STARTUP})
set_target_properties(${TARGET_LIB_MODULE_OPENGL_S_STARTUP} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_OPENGL}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_OPENGL_S_STARTUP}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_include_directories(${TARGET_LIB_MODULE_OPENGL_S_STARTUP} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_link_libraries(${TARGET_LIB_MODULE_OPENGL_S_STARTUP} PUBLIC ${LINKAGE_PUBLIC_LIB_MODULE_OPENGL_S_STARTUP})
