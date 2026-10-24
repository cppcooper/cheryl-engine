get_target_property(world_engine_kind Cheryl::Engine TYPE)
if(NOT world_engine_kind STREQUAL "SHARED_LIBRARY" OR TARGET Cheryl::MapStorageCPU)
    message(FATAL_ERROR "World proof requires one normal shared Engine, without the standalone storage producer")
endif()

# The procedural prototype deliberately cannot be add_subdirectory'd. Compile
# its committed private sources here, without a copied algorithm or public API.
set(world_pg "${CHERYL_REPOSITORY_ROOT}/projects/tests/procedural-generation")
add_library(world-cave-private STATIC "${world_pg}/maze.cpp" "${world_pg}/random.cpp" "${world_pg}/cave.cpp")
target_compile_features(world-cave-private PRIVATE cxx_std_23)
target_include_directories(world-cave-private PUBLIC "${world_pg}")
set(world_demo_private "${CHERYL_REPOSITORY_ROOT}/projects/apps/demo/src")
add_executable(tests-world-consumer main.cpp "${world_demo_private}/world-document.cpp" "${world_demo_private}/tilemap-editor.cpp")
target_include_directories(tests-world-consumer PRIVATE "${world_demo_private}")
# The editor exposes the existing pinned GLM headers to this stricter caller.
# Keep conversion errors enabled for our code without diagnosing vendor headers.
target_include_directories(tests-world-consumer SYSTEM PRIVATE
    "${CHERYL_REPOSITORY_ROOT}/extern/glm" "${CHERYL_REPOSITORY_ROOT}/extern/glm/glm")
target_link_libraries(tests-world-consumer PRIVATE Cheryl::Engine world-cave-private)
set_target_properties(world-cave-private tests-world-consumer PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(world-cave-private PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
    target_compile_options(tests-world-consumer PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()
cheryl_add_header_probes(tests-world-header Cheryl::Engine NEUTRAL HEADERS
    cheryl/core/resources/map-storage/snapshot.h cheryl/core/resources/map-storage/codec.h
    cheryl/core/resources/map-storage/file-storage.h cheryl/core/resources/map-storage/cell-patch.h
    cheryl/maps/tilemap.h cheryl/maps/terrain-map.h)
add_dependencies(tests-world-consumer tests-world-header)
foreach(check IN ITEMS literal generated refusals editor_documents)
    add_test(NAME "world.${check}" COMMAND "${CMAKE_COMMAND}"
        "-DCLI=$<TARGET_FILE:tests-world-consumer>" "-DMODE=${check}"
        "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/world-checks" -P "${CMAKE_CURRENT_SOURCE_DIR}/check.cmake")
    set_tests_properties("world.${check}" PROPERTIES TIMEOUT 60 LABELS world)
endforeach()
