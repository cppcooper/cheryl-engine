cheryl_add_test_aggregate(tests-console OWNER)
set(CHERYL_TEST_AGGREGATE tests-console)
cheryl_add_test_suite(tests-console-core
    SOURCES core.cpp lifetime.cpp
    LIBRARIES Cheryl::Console)
add_test(NAME console_core COMMAND tests-console-core
    "--gtest_filter=ConsoleCore.*:ConsoleLifetime.*")
add_executable(tests-console-allocation allocation.cpp)
target_link_libraries(tests-console-allocation PRIVATE Cheryl::Console)
add_test(NAME console_allocation COMMAND tests-console-allocation)
set_tests_properties(console_core console_allocation PROPERTIES TIMEOUT 30 LABELS "console;core")
