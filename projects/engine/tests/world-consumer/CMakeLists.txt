get_target_property(world_engine_kind Cheryl::Engine TYPE)
if(NOT world_engine_kind STREQUAL "SHARED_LIBRARY" OR TARGET Cheryl::MapStorageCPU)
    message(FATAL_ERROR "World proof requires one normal shared Engine, without the standalone storage producer")
endif()

include("${CHERYL_REPOSITORY_ROOT}/projects/apps/demo/private-cave.cmake")
set(world_demo_private "${CHERYL_REPOSITORY_ROOT}/projects/apps/demo/src")
add_executable(tests-world-consumer main.cpp "${world_demo_private}/world-document.cpp" "${world_demo_private}/tilemap-editor.cpp" "${world_demo_private}/generated-document.cpp")
target_include_directories(tests-world-consumer PRIVATE "${world_demo_private}")
# The editor exposes the existing pinned GLM headers to this stricter caller.
# Keep conversion errors enabled for our code without diagnosing vendor headers.
target_include_directories(tests-world-consumer SYSTEM PRIVATE
    "${CHERYL_REPOSITORY_ROOT}/extern/glm" "${CHERYL_REPOSITORY_ROOT}/extern/glm/glm")
target_link_libraries(tests-world-consumer PRIVATE Cheryl::Engine world-cave-private)
set_target_properties(tests-world-consumer PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tests-world-consumer PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()
cheryl_add_header_probes(tests-world-header Cheryl::Engine NEUTRAL HEADERS
    cheryl/core/resources/map-storage/snapshot.h cheryl/core/resources/map-storage/codec.h
    cheryl/core/resources/map-storage/file-storage.h cheryl/core/resources/map-storage/cell-patch.h
    cheryl/maps/tilemap.h cheryl/maps/terrain-map.h)
add_dependencies(tests-world-consumer tests-world-header)
foreach(check IN ITEMS literal generated refusals editor_documents semantic_documents document_history variable_documents layered_documents)
    add_test(NAME "world.${check}" COMMAND "${CMAKE_COMMAND}"
        "-DCLI=$<TARGET_FILE:tests-world-consumer>" "-DMODE=${check}"
        "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/world-checks" -P "${CMAKE_CURRENT_SOURCE_DIR}/check.cmake")
    set_tests_properties("world.${check}" PROPERTIES TIMEOUT 60 LABELS world)
endforeach()

set_tests_properties(world.variable_documents world.layered_documents PROPERTIES TIMEOUT 150)
