# Isolated timeout/fault/static-teardown cases are driven by logging.py.
file(GLOB_RECURSE logging_acceptance_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(CHERYL_BUILD_ACCEPTANCE_TESTS)
    add_executable(${TARGET_TEST_ACCEPTANCE_ENGINE_LOGGING} ${logging_acceptance_sources})
else()
    add_executable(${TARGET_TEST_ACCEPTANCE_ENGINE_LOGGING} EXCLUDE_FROM_ALL ${logging_acceptance_sources})
endif()
set_target_properties(${TARGET_TEST_ACCEPTANCE_ENGINE_LOGGING} PROPERTIES
    OUTPUT_NAME "${OUTPUT_TEST_ACCEPTANCE_ENGINE_LOGGING}"
    RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(${TARGET_TEST_ACCEPTANCE_ENGINE_LOGGING} PRIVATE ${LINKAGE_PRIVATE_TEST_ACCEPTANCE_ENGINE_LOGGING})
