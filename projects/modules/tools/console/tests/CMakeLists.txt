cheryl_add_test_aggregate(tests-console OWNER)
set(CHERYL_TEST_AGGREGATE tests-console)
cheryl_add_test_suite(tests-console-core
    SOURCES core.cpp lifetime.cpp setting.cpp
    LIBRARIES Cheryl::Console)
cheryl_add_test_suite(tests-console-scenarios
    SOURCES scenario.cpp scenario-authority.cpp
    LIBRARIES Cheryl::Console)
add_test(NAME console_scenarios COMMAND tests-console-scenarios "--gtest_filter=ConsoleScenario.*:ConsoleScenarioAuthority.*")
set_tests_properties(console_scenarios PROPERTIES TIMEOUT 30 LABELS "console;scenario")
add_test(NAME console_core COMMAND tests-console-core
    "--gtest_filter=ConsoleCore.*:ConsoleLifetime.*")
add_executable(tests-console-allocation allocation.cpp
    ../src/registry.cpp ../src/parser.cpp ../src/session.cpp
    ../src/scenario.cpp ../src/scenario-parser.cpp)
target_include_directories(tests-console-allocation PRIVATE ../include)
target_compile_features(tests-console-allocation PRIVATE cxx_std_23)
target_compile_definitions(tests-console-allocation PRIVATE CE_CONSOLE_STATIC)
target_link_libraries(tests-console-allocation PRIVATE Cheryl::Engine)
add_test(NAME console_allocation COMMAND tests-console-allocation)
set_tests_properties(console_core console_allocation PROPERTIES TIMEOUT 30 LABELS "console;core")
cheryl_add_test_suite(tests-console-runtime
    SOURCES runtime-report.cpp
    LIBRARIES Cheryl::Console
    DEFINITIONS CONSOLE_RUNTIME_TEST_ROOT="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME console_runtime_io COMMAND tests-console-runtime "--gtest_filter=ConsoleRuntimeIo.*")
set_tests_properties(console_runtime_io PROPERTIES TIMEOUT 30 LABELS "console;runtime;io")
