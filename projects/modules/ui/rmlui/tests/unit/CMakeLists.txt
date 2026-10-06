include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/TestFont.cmake")
cheryl_rmlui_test_font(test_font)
file(GLOB_RECURSE rmlui_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
cheryl_add_test_suite(${TARGET_TEST_MODULE_UI_RMLUI}
    OUTPUT_NAME "${OUTPUT_TEST_MODULE_UI_RMLUI}"
    SOURCES ${rmlui_test_sources}
    LIBRARIES ${LINKAGE_PUBLIC_TEST_MODULE_UI_RMLUI}
    DEFINITIONS CHERYL_RMLUI_TEST_IMAGE="${CMAKE_CURRENT_LIST_DIR}/../fixtures/rgba.png"
        CHERYL_RMLUI_TEST_BLUE_IMAGE="${CMAKE_CURRENT_LIST_DIR}/../fixtures/blue.png"
        CHERYL_RMLUI_TEST_FONT="${test_font}")
