# Isolated timeout/fault/static-teardown cases are driven by logging.py.
if(CHERYL_BUILD_ACCEPTANCE_TESTS)
    add_executable(cheryl-logging-acceptance src/main.cpp)
else()
    add_executable(cheryl-logging-acceptance EXCLUDE_FROM_ALL src/main.cpp)
endif()
set_target_properties(cheryl-logging-acceptance PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(cheryl-logging-acceptance PRIVATE Cheryl::Engine)
