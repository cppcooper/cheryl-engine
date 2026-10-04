# The profile matrix compiles only the logging cases against the real library.
add_executable(cheryl-logging-tests
    "${CHERYL_ENGINE_TEST_ROOT}/support/gtest-main.cpp"
    src/logging.cpp
    src/logging-policy.cpp)
set_target_properties(cheryl-logging-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(cheryl-logging-tests PRIVATE Cheryl::Engine gtest)
