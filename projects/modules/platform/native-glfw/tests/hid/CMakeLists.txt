# Compile the real tracker/parsers against isolated transport fixtures. The owned
# HID-OFF library supplies Gainput state machinery without a second HID runtime.
set(cheryl_hid_source_dir "${CHERYL_REPOSITORY_ROOT}/extern/gainput/lib/source/gainput/hid")
add_executable(${TARGET_TEST_NATIVE_HID}
    src/hid.cpp
    "${cheryl_hid_source_dir}/GainputHID.cpp"
    "${cheryl_hid_source_dir}/GainputHIDWhitelist.cpp"
    "${cheryl_hid_source_dir}/hidparsers/HIDParserPS4Controller.cpp"
    "${cheryl_hid_source_dir}/hidparsers/HIDParserPS5Controller.cpp")
set_target_properties(${TARGET_TEST_NATIVE_HID} PROPERTIES
    OUTPUT_NAME "${OUTPUT_TEST_NATIVE_HID}"
    RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_include_directories(${TARGET_TEST_NATIVE_HID} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures" "${cheryl_hid_source_dir}")
target_link_libraries(${TARGET_TEST_NATIVE_HID} PRIVATE
    ${LINKAGE_PRIVATE_TEST_RUNNER} ${cheryl_gainput_target})
target_link_options(${TARGET_TEST_NATIVE_HID} PRIVATE -Wl,--wrap=poll)
gtest_discover_tests(${TARGET_TEST_NATIVE_HID}
    TEST_PREFIX "${TARGET_TEST_NATIVE_HID}."
    WORKING_DIRECTORY "${CHERYL_BUILD_ROOT}")
