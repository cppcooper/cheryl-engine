cheryl_add_test_suite(tests-living-scenarios
    SOURCES tests.cpp io.cpp
    LIBRARIES Cheryl::LivingWorld::Scenarios
    DEFINITIONS SCENARIO_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        SCENARIO_ASSETS="${magic_source}/fixtures")
add_test(NAME living_scenarios COMMAND tests-living-scenarios "--gtest_filter=ConsoleScenarios.*")
set_tests_properties(living_scenarios PROPERTIES TIMEOUT 30 LABELS "console;living-world;scenarios;contact")

add_executable(tests-living-scenarios-allocation allocation.cpp)
target_link_libraries(tests-living-scenarios-allocation PRIVATE Cheryl::LivingWorld::Scenarios)
add_test(NAME living_scenarios_allocation COMMAND tests-living-scenarios-allocation
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures" "${magic_source}/fixtures")
set_tests_properties(living_scenarios_allocation PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 LABELS "console;living-world;scenarios;allocation")

if(UNIX)
    add_dependencies(tests-living-scenarios living-scenarios)
    add_dependencies(${CHERYL_TEST_AGGREGATE} living-scenarios)
    foreach(scenario IN ITEMS heal refused wrong-assertion authoring)
        add_test(NAME living_scenarios_batch_${scenario}
            COMMAND "${CMAKE_COMMAND}"
                "-DSCENARIO_EXECUTABLE=$<TARGET_FILE:living-scenarios>"
                "-DSCENARIO_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
                "-DSCENARIO_ASSETS=${magic_source}/fixtures"
                "-DSCENARIO_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/batch-output"
                "-DSCENARIO_CASE=${scenario}"
                "-DSCENARIO_BUILD=ctest-source-witness-$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/batch.cmake")
        set_tests_properties(living_scenarios_batch_${scenario} PROPERTIES
            TIMEOUT 30 LABELS "console;living-world;scenarios;contact;batch")
    endforeach()
endif()
