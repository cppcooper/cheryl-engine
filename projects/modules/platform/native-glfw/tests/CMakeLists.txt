get_filename_component(CHERYL_NATIVE_PRIVATE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src" ABSOLUTE)
set(CHERYL_TEST_AGGREGATE native-glfw-all)
add_subdirectory(all-tests)
add_subdirectory(unit)
