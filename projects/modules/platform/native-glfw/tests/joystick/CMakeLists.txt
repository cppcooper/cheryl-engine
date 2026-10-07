# Keep process-wide syscall wrappers out of ordinary owner/aggregate runners.
add_executable(${TARGET_TEST_NATIVE_JOYSTICK} src/joystick.cpp)
set_target_properties(${TARGET_TEST_NATIVE_JOYSTICK} PROPERTIES
    OUTPUT_NAME "${OUTPUT_TEST_NATIVE_JOYSTICK}"
    RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(${TARGET_TEST_NATIVE_JOYSTICK} PRIVATE
    ${LINKAGE_PRIVATE_TEST_RUNNER} ${LINKAGE_PUBLIC_TEST_MODULE_NATIVE_GLFW})
target_link_options(${TARGET_TEST_NATIVE_JOYSTICK} PRIVATE
    -Wl,--wrap=open -Wl,--wrap=read -Wl,--wrap=ioctl -Wl,--wrap=close)
gtest_discover_tests(${TARGET_TEST_NATIVE_JOYSTICK}
    TEST_PREFIX "${TARGET_TEST_NATIVE_JOYSTICK}."
    WORKING_DIRECTORY "${CHERYL_BUILD_ROOT}")
