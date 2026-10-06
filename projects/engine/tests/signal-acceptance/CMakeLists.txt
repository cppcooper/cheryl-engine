# The driver compares Engine-linked consumers in builds with and without NDEBUG.
file(GLOB_RECURSE signal_acceptance_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(CHERYL_BUILD_ACCEPTANCE_TESTS)
    add_executable(${TARGET_TEST_ACCEPTANCE_ENGINE_SIGNAL} ${signal_acceptance_sources})
else()
    add_executable(${TARGET_TEST_ACCEPTANCE_ENGINE_SIGNAL} EXCLUDE_FROM_ALL ${signal_acceptance_sources})
endif()
set_target_properties(${TARGET_TEST_ACCEPTANCE_ENGINE_SIGNAL} PROPERTIES
    OUTPUT_NAME "${OUTPUT_TEST_ACCEPTANCE_ENGINE_SIGNAL}"
    RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_link_libraries(${TARGET_TEST_ACCEPTANCE_ENGINE_SIGNAL} PRIVATE ${LINKAGE_PRIVATE_TEST_ACCEPTANCE_ENGINE_SIGNAL})
