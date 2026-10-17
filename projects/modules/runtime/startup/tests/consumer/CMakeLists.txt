add_executable(consumer-module-startup src/main.cpp)
set_target_properties(consumer-module-startup PROPERTIES
    OUTPUT_NAME cheryl-startup-consumer
    RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(consumer-module-startup PRIVATE Cheryl::Startup)
add_test(NAME consumer-module-startup COMMAND consumer-module-startup)
