set(native_test_sources src/glfw-diagnostics.cpp)
if(CHERYL_NATIVE_INPUT)
    list(APPEND native_test_sources src/input.cpp)
endif()
cheryl_add_test_suite(${TARGET_TEST_MODULE_NATIVE_GLFW}
    OUTPUT_NAME "${OUTPUT_TEST_MODULE_NATIVE_GLFW}"
    SOURCES ${native_test_sources}
    LIBRARIES ${LINKAGE_PUBLIC_TEST_MODULE_NATIVE_GLFW}
    PRIVATE_INCLUDES "${CHERYL_NATIVE_PRIVATE_SOURCE_DIR}")
