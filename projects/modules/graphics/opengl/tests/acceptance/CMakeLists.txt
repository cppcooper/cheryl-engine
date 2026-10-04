set(native_test_definitions CHERYL_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../fixtures")
set(native_test_libraries Cheryl::OpenGL glfw)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GLFW_BUILD_X11)
    if(NOT TARGET X11::X11)
        find_package(X11 REQUIRED)
    endif()
    list(APPEND native_test_definitions CHERYL_NATIVE_X11_TESTS=1)
    list(APPEND native_test_libraries X11::X11)
endif()
cheryl_add_test_suite(cheryl-opengl-acceptance ACCEPTANCE
    SOURCES src/native-opengl.cpp
    LIBRARIES ${native_test_libraries}
    PRIVATE_INCLUDES "${CHERYL_OPENGL_PRIVATE_SOURCE_DIR}"
    DEFINITIONS ${native_test_definitions})
