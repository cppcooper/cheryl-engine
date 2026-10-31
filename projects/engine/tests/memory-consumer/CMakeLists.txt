# Reuse focused resource cases without enabling the broad acceptance campaign.
include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylConsumers.cmake")
add_executable(tests-memory-containers main.cpp release.cpp
    ../all-tests/src/resources/memory.cpp
    ../all-tests/src/resources/reservations.cpp
    ../all-tests/src/templates/block.cpp)
target_link_libraries(tests-memory-containers PRIVATE Cheryl::Engine ${TARGET_LIB_TEST_MAIN})
cheryl_discover_tests(tests-memory-containers)
cheryl_add_header_probes(tests-allocator-header Cheryl::Engine NEUTRAL HEADERS
    cheryl/core/resources/memory/allocators/default-allocator.hpp
    cheryl/core/resources/memory/allocators/object-allocator.hpp
    cheryl/core/resources/memory/allocators/object-pool-allocator.hpp)
add_dependencies(tests-memory-containers tests-allocator-header)

# Real standard-container trace; timings use the unwrapped public allocators.
add_executable(tests-allocator-trace trace.cpp)
target_link_libraries(tests-allocator-trace PRIVATE Cheryl::Engine)
target_include_directories(tests-allocator-trace SYSTEM PRIVATE
    "${CHERYL_REPOSITORY_ROOT}/extern/glm" "${CHERYL_REPOSITORY_ROOT}/extern/glm/glm")
set_target_properties(tests-allocator-trace PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tests-allocator-trace PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()
add_test(NAME allocator_trace.matched COMMAND tests-allocator-trace --verify)
set_tests_properties(allocator_trace.matched PROPERTIES TIMEOUT 60 LABELS memory)
