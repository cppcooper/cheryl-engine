# The profile matrix compiles only the logging cases against the real library.
cheryl_add_test_suite(${TARGET_TEST_ENGINE_LOGGING}
    OUTPUT_NAME "${OUTPUT_TEST_ENGINE_LOGGING}"
    SOURCES src/logging.cpp src/logging-policy.cpp
    LIBRARIES ${LINKAGE_PUBLIC_TEST_ENGINE_LOGGING})
