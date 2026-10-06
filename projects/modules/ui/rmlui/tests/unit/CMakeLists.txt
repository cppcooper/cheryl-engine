cheryl_add_test_suite(ui-rmlui-tests
    SOURCES src/input.cpp src/rendering.cpp src/scene.cpp
    LIBRARIES Cheryl::UI::RmlUi
    DEFINITIONS CHERYL_RMLUI_TEST_IMAGE="${CMAKE_CURRENT_LIST_DIR}/../fixtures/rgba.png")
