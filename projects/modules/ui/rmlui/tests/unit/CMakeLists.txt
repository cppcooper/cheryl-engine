include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/TestFont.cmake")
cheryl_rmlui_test_font(test_font)
cheryl_add_test_suite(ui-rmlui-tests
    SOURCES src/input.cpp src/rendering.cpp src/scene.cpp src/session.cpp src/runtime.cpp
    LIBRARIES Cheryl::UI::RmlUi
    DEFINITIONS CHERYL_RMLUI_TEST_IMAGE="${CMAKE_CURRENT_LIST_DIR}/../fixtures/rgba.png"
        CHERYL_RMLUI_TEST_BLUE_IMAGE="${CMAKE_CURRENT_LIST_DIR}/../fixtures/blue.png"
        CHERYL_RMLUI_TEST_FONT="${test_font}")
