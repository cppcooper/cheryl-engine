# The profile matrix compiles only the logging cases against the real library.
cheryl_add_test_suite(logging-tests
    SOURCES src/logging.cpp src/logging-policy.cpp
    LIBRARIES Cheryl::Engine)
