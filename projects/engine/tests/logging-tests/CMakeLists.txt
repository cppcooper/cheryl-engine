# The profile matrix compiles only the logging cases against the real library.
file(GLOB_RECURSE logging_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
cheryl_add_test_suite(${TARGET_TEST_ENGINE_LOGGING}
    OUTPUT_NAME "${OUTPUT_TEST_ENGINE_LOGGING}"
    SOURCES ${logging_test_sources}
    LIBRARIES ${LINKAGE_PUBLIC_TEST_ENGINE_LOGGING})
