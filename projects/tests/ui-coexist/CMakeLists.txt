include("${CHERYL_REPOSITORY_ROOT}/projects/modules/ui/rmlui/cmake/TestFont.cmake")
cheryl_rmlui_test_font(test_font)
cheryl_add_test_suite(ui-coexist-tests
    SOURCES src/coexist.cpp
    LIBRARIES Cheryl::UI::TGUI Cheryl::UI::RmlUi
    DEFINITIONS CHERYL_RMLUI_TEST_FONT="${test_font}")
