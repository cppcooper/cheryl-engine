cheryl_add_test_suite(tests-living-console
    SOURCES tests.cpp authoring.cpp
    LIBRARIES Cheryl::LivingWorld::Console)
add_executable(living-console-consumer consumer.cpp)
target_link_libraries(living-console-consumer PRIVATE Cheryl::LivingWorld::Console)
add_test(NAME living_console COMMAND tests-living-console "--gtest_filter=ConsoleHost.*")
add_test(NAME living_console_consumer COMMAND living-console-consumer)
set_tests_properties(living_console living_console_consumer
    PROPERTIES TIMEOUT 30 LABELS "console;living-world;contact")
