add_executable(backward-cpp src/main.cpp)
set_target_properties(backward-cpp PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(backward-cpp PUBLIC Backward::Backward)
