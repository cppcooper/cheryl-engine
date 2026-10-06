cheryl_add_test_suite(${TARGET_TEST_MODULE_UI_TGUI}
    OUTPUT_NAME "${OUTPUT_TEST_MODULE_UI_TGUI}"
    SOURCES src/input.cpp src/layout.cpp src/rendering.cpp src/scene.cpp src/session.cpp src/runtime.cpp
    LIBRARIES ${LINKAGE_PUBLIC_TEST_MODULE_UI_TGUI})
