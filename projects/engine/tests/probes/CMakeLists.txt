# Isolate replacement allocation operators from every GoogleTest runner. The
# aligned allocator/interposition probe is verified only on the native Linux runner.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(dialogue-lookup-allocation dialogue-lookup-allocation.cpp)
    target_link_libraries(dialogue-lookup-allocation PRIVATE Cheryl::Engine)
    set_target_properties(dialogue-lookup-allocation PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
    add_test(NAME tests-dialogue-lookup.allocation COMMAND dialogue-lookup-allocation)
    set_tests_properties(tests-dialogue-lookup.allocation PROPERTIES WORKING_DIRECTORY "${CHERYL_BUILD_ROOT}")
endif()
