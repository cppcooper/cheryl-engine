cheryl_add_test_suite(${TARGET_TEST_MODULE_OPENGL}
    OUTPUT_NAME "${OUTPUT_TEST_MODULE_OPENGL}"
    SOURCES
        src/opengl-debug.cpp
        src/opengl-lifetime.cpp
        src/opengl-pipeline.cpp
        src/opengl-program-builder.cpp
    LIBRARIES ${LINKAGE_PUBLIC_TEST_MODULE_OPENGL}
    PRIVATE_INCLUDES "${CHERYL_OPENGL_PRIVATE_SOURCE_DIR}")
