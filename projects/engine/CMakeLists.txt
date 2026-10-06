project(cheryl_engine VERSION ${VERSION_PROJECT_ENGINE} LANGUAGES C CXX)

# Native and graphics implementations belong to their selected modules.
file(GLOB_RECURSE CHERYL_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_subdirectory(support/logging-config)
add_subdirectory(support/signal-handlers)

add_library(${TARGET_LIB_ENGINE} STATIC ${CHERYL_ENGINE_SOURCES})
add_library(Cheryl::Engine ALIAS ${TARGET_LIB_ENGINE})
set_target_properties(${TARGET_LIB_ENGINE} PROPERTIES
    VERSION "${VERSION_PROJECT_ENGINE}"
    OUTPUT_NAME "${OUTPUT_LIB_ENGINE}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
    CHERYL_REPOSITORY_ROOT "${CHERYL_REPOSITORY_ROOT}")
target_compile_features(${TARGET_LIB_ENGINE} PUBLIC cxx_std_23)
target_include_directories(${TARGET_LIB_ENGINE} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>"
    # Preserve the existing public glm.hpp spelling alongside glm/glm.hpp.
    "$<BUILD_INTERFACE:${CHERYL_REPOSITORY_ROOT}/extern/glm/glm>")
target_include_directories(${TARGET_LIB_ENGINE} PRIVATE
    "${CHERYL_REPOSITORY_ROOT}/extern/stb"
    "${CHERYL_REPOSITORY_ROOT}/extern/json/single_include")
target_link_libraries(${TARGET_LIB_ENGINE}
    PUBLIC ${LINKAGE_PUBLIC_LIB_ENGINE})
# Forward the bootstrap object to final consumers; an unreferenced archive member
# would not reliably install the original Debug crash handlers.
target_link_libraries(${TARGET_LIB_ENGINE} INTERFACE ${LINKAGE_INTERFACE_LIB_ENGINE})
add_dependencies(${TARGET_LIB_ENGINE} ${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS})
if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Root Debug compilation is instrumented; standalone consumers also need
    # the sanitizer runtime even when their own directory is outside that root.
    target_link_options(${TARGET_LIB_ENGINE} INTERFACE -fsanitize=address,undefined)
endif()

if(CHERYL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
