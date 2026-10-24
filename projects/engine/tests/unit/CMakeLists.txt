file(GLOB_RECURSE engine_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(APPEND engine_test_sources ${CHERYL_ENGINE_SHARED_UNIT_SOURCES})
# Exercise the real private app state without enabling demo/native modules or
# exporting application headers through Engine's public include requirements.
set(tilemap_demo_private "${CMAKE_CURRENT_SOURCE_DIR}/../../../apps/demo/src")
list(APPEND engine_test_sources "${tilemap_demo_private}/tilemap-editor.cpp")
list(APPEND engine_test_sources "${tilemap_demo_private}/world-document.cpp")
list(APPEND engine_test_sources "${tilemap_demo_private}/tilemap-demo.cpp")
cheryl_add_test_suite(${TARGET_TEST_ENGINE}
    OUTPUT_NAME "${OUTPUT_TEST_ENGINE}"
    SOURCES ${engine_test_sources}
    LIBRARIES ${LINKAGE_PUBLIC_TEST_ENGINE}
    PRIVATE_INCLUDES "${CHERYL_ENGINE_PRIVATE_SOURCE_DIR}" "${tilemap_demo_private}")
