project(cheryl_engine VERSION ${VERSION_PROJECT_ENGINE} LANGUAGES C CXX)

# Native and graphics implementations belong to their selected modules.
file(GLOB_RECURSE CHERYL_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Logging.cmake")
add_subdirectory(private/diagnostics)

add_library(${TARGET_LIB_ENGINE} ${CHERYL_LIBRARY_TYPE} ${CHERYL_ENGINE_SOURCES})
add_library(Cheryl::Engine ALIAS ${TARGET_LIB_ENGINE})
cheryl_register_public_component(${TARGET_LIB_ENGINE})
set_target_properties(${TARGET_LIB_ENGINE} PROPERTIES
    VERSION "${VERSION_PROJECT_ENGINE}"
    OUTPUT_NAME "${OUTPUT_LIB_ENGINE}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
    CHERYL_REPOSITORY_ROOT "${CHERYL_REPOSITORY_ROOT}")
cheryl_configure_library(${TARGET_LIB_ENGINE} ENGINE)
if(CHERYL_BUILD_TESTS)
    target_compile_definitions(${TARGET_LIB_ENGINE} PRIVATE CHERYL_LOGGING_TEST_HOOKS=1)
endif()
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
    PUBLIC ${LINKAGE_PUBLIC_LIB_ENGINE}
    PRIVATE ${LINKAGE_PRIVATE_LIB_ENGINE})
get_target_property(cheryl_owned_spdlog spdlog::spdlog CHERYL_OWNED_DEPENDENCY)
if(cheryl_owned_spdlog)
    cheryl_private_dependency(spdlog::spdlog OWNER ${TARGET_LIB_ENGINE} OWNED)
else()
    cheryl_private_dependency(spdlog::spdlog OWNER ${TARGET_LIB_ENGINE})
endif()
foreach(dependency IN ITEMS Freetype::Freetype harfbuzz::harfbuzz)
    cheryl_shared_dependency(${dependency} OWNER ${TARGET_LIB_ENGINE})
endforeach()
if(WIN32)
    cheryl_shared_dependency(Cheryl::ICU OWNER ${TARGET_LIB_ENGINE} SYSTEM)
else()
    cheryl_shared_dependency(Cheryl::ICU OWNER ${TARGET_LIB_ENGINE})
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FontFallback.cmake")
# A shared Engine owns one bootstrap. Static archives forward it so an
# unreferenced archive member cannot discard the original Debug crash handlers.
if(CHERYL_BUILD_SHARED_LIBS)
    target_link_libraries(${TARGET_LIB_ENGINE} PRIVATE ${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS})
else()
    target_link_libraries(${TARGET_LIB_ENGINE} INTERFACE ${LINKAGE_INTERFACE_LIB_ENGINE})
endif()
add_dependencies(${TARGET_LIB_ENGINE} ${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS})
if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Root Debug compilation is instrumented; standalone consumers also need
    # the sanitizer runtime even when their own directory is outside that root.
    target_link_options(${TARGET_LIB_ENGINE} INTERFACE -fsanitize=address,undefined)
endif()
