cmake_minimum_required(VERSION 3.28)
project(cheryl_consumer LANGUAGES CXX)

set(CHERYL_ENGINE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Cheryl source checkout")
set(CHERYL_BUILD_TESTS OFF CACHE BOOL "Build Cheryl's tests and acceptance tools")
set(CHERYL_BUILD_DEMO OFF CACHE BOOL "Build the windowed demonstration application")
add_subdirectory("${CHERYL_ENGINE_SOURCE}" engine EXCLUDE_FROM_ALL)

# No dependency list, include directories, language standard or engine sources.
add_executable(cheryl-consumer main.cpp resources.cpp)
target_link_libraries(cheryl-consumer PRIVATE Cheryl::Engine)

# Each translation unit includes its header first, with no umbrella/prefix header.
set(consumer_headers
    cheryl/core/engine/engine-context.h
    cheryl/core/engine/worker-pool.h
    cheryl/core/controls/input-interface.h
    cheryl/core/display/display-system-interface.h
    cheryl/core/rendering/render-frame.h
    cheryl/core/game-framework/game-runtime.h
    cheryl/core/logging/logger.h
    cheryl/core/resources/asset-management/asset-loader.h
    cheryl/assets/resources/resource-provider.h
    cheryl/backends/opengl/glslprogram.h)
if(NOT CHERYL_SANDBOX_BUILD)
    list(APPEND consumer_headers cheryl/core/controls/input-system.h)
endif()
set(header_sources)
foreach(header IN LISTS consumer_headers)
    string(MAKE_C_IDENTIFIER "${header}" identifier)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/headers/${identifier}.cpp")
    file(WRITE "${source}" "#include <${header}>\n")
    list(APPEND header_sources "${source}")
endforeach()
add_library(cheryl-consumer-headers OBJECT ${header_sources})
target_link_libraries(cheryl-consumer-headers PRIVATE Cheryl::Engine)
add_dependencies(cheryl-consumer cheryl-consumer-headers)
