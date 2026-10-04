add_executable(demo src/main.cpp)
set_target_properties(demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_compile_definitions(demo PRIVATE CHERYL_SOURCE_DIR="${CHERYL_REPOSITORY_ROOT}")
target_link_libraries(demo PRIVATE Cheryl::Engine Cheryl::NativeGLFW Cheryl::OpenGL)
