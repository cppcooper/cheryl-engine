include("${CHERYL_REPOSITORY_ROOT}/projects/modules/ui/rmlui/cmake/TestFont.cmake")
cheryl_rmlui_test_font(test_font)
file(GLOB_RECURSE ui_coexist_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
cheryl_add_test_suite(${TARGET_TEST_UI_COEXIST}
    OUTPUT_NAME "${OUTPUT_TEST_UI_COEXIST}"
    SOURCES ${ui_coexist_sources}
    LIBRARIES ${LINKAGE_PUBLIC_TEST_UI_COEXIST}
    DEFINITIONS CHERYL_RMLUI_TEST_FONT="${test_font}")
