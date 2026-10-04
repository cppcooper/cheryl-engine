set(native_test_sources src/glfw-diagnostics.cpp)
if(CHERYL_NATIVE_INPUT)
    list(APPEND native_test_sources src/input.cpp)
endif()
cheryl_add_test_suite(cheryl-native-glfw-tests
    SOURCES ${native_test_sources}
    LIBRARIES Cheryl::NativeGLFW glfw
    PRIVATE_INCLUDES "${CHERYL_NATIVE_PRIVATE_SOURCE_DIR}")
