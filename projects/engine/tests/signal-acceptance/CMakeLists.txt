# The driver compares Engine-linked consumers in builds with and without NDEBUG.
if(CHERYL_BUILD_ACCEPTANCE_TESTS)
    add_executable(cheryl-signal-acceptance src/main.cpp)
else()
    add_executable(cheryl-signal-acceptance EXCLUDE_FROM_ALL src/main.cpp)
endif()
set_target_properties(cheryl-signal-acceptance PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(cheryl-signal-acceptance PRIVATE Cheryl::Engine)
