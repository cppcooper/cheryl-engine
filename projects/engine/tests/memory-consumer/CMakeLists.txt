# Reuse focused resource cases without enabling the broad acceptance campaign.
include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylConsumers.cmake")
add_executable(tests-memory-containers main.cpp
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
