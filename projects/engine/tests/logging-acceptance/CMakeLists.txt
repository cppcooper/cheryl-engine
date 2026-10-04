# Isolated timeout/fault/static-teardown cases are driven by logging.py.
add_executable(cheryl-logging-acceptance src/main.cpp)
set_target_properties(cheryl-logging-acceptance PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(cheryl-logging-acceptance PRIVATE Cheryl::Engine)
