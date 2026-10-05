# Native and graphics implementations belong to their selected modules.
set(CHERYL_ENGINE_SOURCES
    src/assets/definitions/grid.cpp
    src/assets/definitions/manifest.cpp
    src/assets/definitions/sprite.cpp
    src/assets/definitions/tileset.cpp
    src/assets/geometry/grid-geometry.cpp
    src/assets/resources/decoded-image.cpp
    src/assets/resources/parameters.cpp
    src/assets/resources/pipeline.cpp
    src/assets/resources/resource-provider.cpp
    src/assets/submission/draw2d.cpp
    src/assets/types/2d/ffont.cpp
    src/assets/types/2d/graphic.cpp
    src/assets/types/2d/sprite.cpp
    src/assets/types/2d/stbfont.cpp
    src/assets/types/2d/tileset.cpp
    src/core/controls/input-accumulator.cpp
    src/core/controls/input-bindings.cpp
    src/core/controls/input-capture.cpp
    src/core/controls/input-interface.cpp
    src/core/controls/input-routing.cpp
    src/core/controls/polling-backlog.cpp
    src/core/controls/tick-input.cpp
    src/core/display/monitor.cpp
    src/core/engine/engine-context.cpp
    src/core/engine/event-delivery.cpp
    src/core/engine/platform-dispatcher.cpp
    src/core/engine/simulation-dispatcher.cpp
    src/core/engine/worker-affinity.cpp
    src/core/engine/worker-pool.cpp
    src/core/game-framework/game-runtime.cpp
    src/core/game-framework/simulation-scheduler.cpp
    src/core/game-framework/tick-context.cpp
    src/core/logging/backend-guard.cpp
    src/core/logging/log.cpp
    src/core/rendering/camera.cpp
    src/core/rendering/clip-region.cpp
    src/core/rendering/draw-packet.cpp
    src/core/resources/asset-management/asset-loader.cpp
    src/core/resources/asset-management/font-mgr.cpp
    src/core/resources/asset-management/manifest-loader.cpp
    src/core/resources/asset-management/material-mgr.cpp
    src/core/resources/asset-management/shader-mgr.cpp
    src/core/resources/asset-management/sprite-mgr.cpp
    src/core/resources/asset-management/texture-mgr.cpp
    src/core/resources/asset-management/tileset-mgr.cpp
    src/core/resources/fileio/file-mgr.cpp
    src/core/resources/fileio/fonts-list.cpp
    src/core/subsystems/event-bus.cpp
    src/core/subsystems/event-system.cpp
    src/internal/diagnostics.cpp
    src/internal/exceptions.cpp
    src/internal/internal-logs.cpp
    src/math/anchor.cpp)
add_subdirectory(support/logging-config)
add_subdirectory(support/signal-handlers)

add_library(cherylGL STATIC ${CHERYL_ENGINE_SOURCES})
add_library(Cheryl::Engine ALIAS cherylGL)
set_target_properties(cherylGL PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
    CHERYL_REPOSITORY_ROOT "${CHERYL_REPOSITORY_ROOT}")
target_compile_features(cherylGL PUBLIC cxx_std_23)
target_include_directories(cherylGL PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>"
    # Preserve the existing public glm.hpp spelling alongside glm/glm.hpp.
    "$<BUILD_INTERFACE:${CHERYL_REPOSITORY_ROOT}/extern/glm/glm>")
target_include_directories(cherylGL PRIVATE
    "${CHERYL_REPOSITORY_ROOT}/extern/stb"
    "${CHERYL_REPOSITORY_ROOT}/extern/json/single_include")
target_link_libraries(cherylGL
    PUBLIC Threads::Threads cheryl_logging_config ctti spdlog::spdlog glm::glm-header-only Backward::Interface)
# Forward the bootstrap object to final consumers; an unreferenced archive member
# would not reliably install the original Debug crash handlers.
target_link_libraries(cherylGL INTERFACE "$<TARGET_OBJECTS:cheryl_signal_handlers>")
add_dependencies(cherylGL cheryl_signal_handlers)
if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Root Debug compilation is instrumented; standalone consumers also need
    # the sanitizer runtime even when their own directory is outside that root.
    target_link_options(cherylGL INTERFACE -fsanitize=address,undefined)
endif()

if(CHERYL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
