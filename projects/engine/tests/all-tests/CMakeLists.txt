set(all_test_sources
    src/assets/draw-contract.cpp
    src/assets/manifest.cpp
    src/assets/pipeline.cpp
    src/assets/render-frame.cpp
    src/assets/submission.cpp
    src/assets/vertex.cpp
    src/backends/native-opengl.cpp
    src/backends/opengl-debug.cpp
    src/backends/opengl-lifetime.cpp
    src/backends/opengl-pipeline.cpp
    src/backends/opengl-program-builder.cpp
    src/core/camera.cpp
    src/core/display-contract.cpp
    src/core/event-bus.cpp
    src/core/glfw-diagnostics.cpp
    src/core/input-capture.cpp
    src/core/input-contract.cpp
    src/core/input-routing.cpp
    src/core/input.cpp
    src/core/polling-backlog.cpp
    src/core/runtime-adapter.cpp
    src/core/simulation-scheduler.cpp
    src/core/tick-context.cpp
    src/core/worker-pool.cpp
    src/diagnostics.cpp
    src/extern/backward-cpp.cpp
    src/extern/ctti.cpp
    src/extern/glm.cpp
    src/extern/spdlog.cpp
    src/math/math.cpp
    src/math/numbers-link.cpp
    src/math/numbers.cpp
    src/resources/asset-cache.cpp
    src/resources/fonts.cpp
    src/resources/memory.cpp
    src/resources/reservations.cpp
    src/templates/block.cpp
    src/templates/ovar.cpp
    src/templates/singleton.cpp
    src/templates/state.cpp)

add_executable(all-tests
    "${CHERYL_ENGINE_TEST_ROOT}/support/gtest-main.cpp"
    ${all_test_sources}
    "${CHERYL_ENGINE_TEST_ROOT}/logging-tests/src/logging.cpp"
    "${CHERYL_ENGINE_TEST_ROOT}/logging-tests/src/logging-policy.cpp")
set_target_properties(all-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(all-tests PRIVATE Cheryl::Engine gtest glfw)
target_include_directories(all-tests PRIVATE
    "${CHERYL_ENGINE_TEST_ROOT}/include"
    "${CHERYL_ENGINE_PRIVATE_SOURCE_DIR}")
target_compile_definitions(all-tests PRIVATE
    CHERYL_SOURCE_DIR="${CHERYL_REPOSITORY_ROOT}"
    CHERYL_TEST_FIXTURE_DIR="${CHERYL_ENGINE_TEST_ROOT}/fixtures")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GLFW_BUILD_X11 AND NOT CHERYL_SANDBOX_BUILD)
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/backends/native-opengl.cpp"
        APPEND PROPERTY COMPILE_DEFINITIONS CHERYL_NATIVE_X11_TESTS=1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # backward-cpp's fallback resolver reads exported symbols in Release builds.
    target_link_options(all-tests PRIVATE -rdynamic)
endif()
if(UNIX AND NOT APPLE AND NOT CHERYL_SANDBOX_BUILD)
    target_link_libraries(all-tests PRIVATE X11::X11)
endif()
gtest_discover_tests(all-tests WORKING_DIRECTORY "${CHERYL_BUILD_ROOT}")
