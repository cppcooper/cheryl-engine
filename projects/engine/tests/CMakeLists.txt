set(CHERYL_ENGINE_TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(CHERYL_ENGINE_PRIVATE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src" ABSOLUTE)

# Preserve the focused/acceptance split of cases still stored under all-tests.
# New cases belong in the selected suite's own src directory.
file(GLOB_RECURSE cheryl_engine_shared_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/all-tests/src/*.cpp")
set(cheryl_engine_acceptance_pattern
    "/all-tests/src/(diagnostics\\.cpp|(extern|resources)/.*|core/(event-bus|runtime-adapter|worker-pool)\\.cpp|templates/(block|singleton)\\.cpp)$")
set(CHERYL_ENGINE_SHARED_UNIT_SOURCES ${cheryl_engine_shared_test_sources})
list(FILTER CHERYL_ENGINE_SHARED_UNIT_SOURCES EXCLUDE REGEX "${cheryl_engine_acceptance_pattern}")
set(CHERYL_ENGINE_SHARED_ACCEPTANCE_SOURCES ${cheryl_engine_shared_test_sources})
list(FILTER CHERYL_ENGINE_SHARED_ACCEPTANCE_SOURCES INCLUDE REGEX "${cheryl_engine_acceptance_pattern}")

include(GoogleTest)
include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylTests.cmake")
if(NOT TARGET ${TARGET_LIB_TEST_SUPPORT})
    add_subdirectory(support)
endif()
set(CHERYL_TEST_AGGREGATE ${TARGET_TEST_ALL_ENGINE})
add_subdirectory(all-tests)
add_subdirectory(unit)
# Narrow public terrain boundary check; independent of the optional broad
# CHERYL_BUILD_CONSUMER_TESTS family and unit-private include requirements.
if(CHERYL_BUILD_SHARED_LIBS)
    add_subdirectory(terrain-consumer)
endif()
add_subdirectory(acceptance)
if(TARGET Cheryl::Workers)
    add_subdirectory("${CHERYL_REPOSITORY_ROOT}/projects/modules/runtime/workers/tests" worker-topology)
endif()
add_subdirectory(logging-tests)
add_subdirectory(logging-acceptance)
add_subdirectory(signal-acceptance)
