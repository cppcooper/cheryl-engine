set(CHERYL_TEST_AGGREGATE all-startup)
cheryl_add_test_aggregate(${CHERYL_TEST_AGGREGATE} OUTPUT_NAME tests-all-startup OWNER)
cheryl_add_test_suite(tests-startup
    OUTPUT_NAME tests-startup
    SOURCES src/startup.cpp
    LIBRARIES Cheryl::Startup)
