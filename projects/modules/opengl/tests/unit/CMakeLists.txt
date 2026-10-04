cheryl_add_test_suite(cheryl-opengl-tests
    SOURCES
        src/opengl-debug.cpp
        src/opengl-lifetime.cpp
        src/opengl-pipeline.cpp
        src/opengl-program-builder.cpp
    LIBRARIES Cheryl::OpenGL
    PRIVATE_INCLUDES "${CHERYL_OPENGL_PRIVATE_SOURCE_DIR}")
