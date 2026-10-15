cmake_minimum_required(VERSION 3.28)
project(cheryl_sdk_consumer LANGUAGES CXX)

find_package(Cheryl CONFIG REQUIRED)
if(NOT TARGET Cheryl::Cheryl)
    message(FATAL_ERROR "The installed SDK must provide the Cheryl::Cheryl aggregate target")
endif()
include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylConsumers.cmake")
enable_testing()

function(sdk_consumer name path library headers_target)
    file(GLOB sources "${CHERYL_REPOSITORY_ROOT}/${path}/src/*.cpp")
    add_executable(${name} ${sources})
    target_link_libraries(${name} PRIVATE ${library})
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    add_subdirectory("${CHERYL_REPOSITORY_ROOT}/${path}/headers" "headers/${name}")
    add_dependencies(${name} ${headers_target})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

sdk_consumer(sdk-engine projects/engine/tests/consumer Cheryl::Engine ${TARGET_TEST_CONSUMER_ENGINE_HEADERS})
if(TARGET Cheryl::NativeGLFW)
    get_target_property(CHERYL_NATIVE_INPUT Cheryl::NativeGLFW CHERYL_NATIVE_INPUT)
    sdk_consumer(sdk-native projects/modules/platform/native-glfw/tests/consumer Cheryl::NativeGLFW
        ${TARGET_TEST_CONSUMER_MODULE_NATIVE_GLFW_HEADERS})
endif()
if(TARGET Cheryl::OpenGL)
    sdk_consumer(sdk-opengl projects/modules/graphics/opengl/tests/consumer Cheryl::OpenGL
        ${TARGET_TEST_CONSUMER_MODULE_OPENGL_HEADERS})
endif()
if(TARGET Cheryl::UI::TGUI)
    sdk_consumer(sdk-tgui projects/modules/ui/tgui/tests/consumer Cheryl::UI::TGUI
        ${TARGET_TEST_CONSUMER_MODULE_UI_TGUI_HEADERS})
endif()
if(TARGET Cheryl::UI::RmlUi)
    sdk_consumer(sdk-rmlui projects/modules/ui/rmlui/tests/consumer Cheryl::UI::RmlUi
        ${TARGET_TEST_CONSUMER_MODULE_UI_RMLUI_HEADERS})
    target_compile_definitions(sdk-rmlui PRIVATE
        CHERYL_RMLUI_TEST_FONT="${CHERYL_REPOSITORY_ROOT}/assets/fonts/DejaVuSans.ttf")
endif()
if(TARGET Cheryl::Audio::Miniaudio)
    sdk_consumer(sdk-audio projects/modules/audio/miniaudio/tests/consumer Cheryl::Audio::Miniaudio
        ${TARGET_TEST_CONSUMER_MODULE_AUDIO_MINIAUDIO_HEADERS})
    target_include_directories(sdk-audio PRIVATE
        "${CHERYL_REPOSITORY_ROOT}/projects/modules/audio/miniaudio/tests/support")
endif()

cheryl_add_header_probes(sdk-startup Cheryl::Startup NEUTRAL
    HEADERS cheryl/core/engine/startup.h cheryl/support/startup.h)
if(TARGET Cheryl::OpenGL::Startup)
    cheryl_add_header_probes(sdk-opengl-startup Cheryl::OpenGL::Startup
        HEADERS cheryl/backends/opengl/startup.h cheryl/support/opengl-startup.h)
endif()
add_executable(sdk-worker "${CHERYL_REPOSITORY_ROOT}/projects/engine/support/worker-topology/tests/consumer.cpp")
target_link_libraries(sdk-worker PRIVATE Cheryl::Engine)
if(TARGET Cheryl::WorkerTopology::Hwloc)
    cheryl_add_header_probes(sdk-worker-hwloc Cheryl::WorkerTopology::Hwloc NEUTRAL
        HEADERS cheryl/support/worker-topology-hwloc.h)
    target_link_libraries(sdk-worker PRIVATE Cheryl::WorkerTopology::Hwloc)
    target_compile_definitions(sdk-worker PRIVATE CHERYL_CONSUMER_WORKER_HWLOC=1)
endif()
if(TARGET Cheryl::WorkerTopology::Json)
    cheryl_add_header_probes(sdk-worker-json Cheryl::WorkerTopology::Json NEUTRAL
        HEADERS cheryl/support/worker-topology-json.h)
    target_link_libraries(sdk-worker PRIVATE Cheryl::WorkerTopology::Json)
    target_compile_definitions(sdk-worker PRIVATE CHERYL_CONSUMER_WORKER_JSON=1)
endif()
add_test(NAME sdk-worker COMMAND sdk-worker)
