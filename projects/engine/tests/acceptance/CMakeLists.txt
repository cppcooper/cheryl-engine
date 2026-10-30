file(GLOB_RECURSE engine_acceptance_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(APPEND engine_acceptance_sources ${CHERYL_ENGINE_SHARED_ACCEPTANCE_SOURCES})
cheryl_add_test_suite(${TARGET_TEST_ACCEPTANCE_ENGINE} ACCEPTANCE
    OUTPUT_NAME "${OUTPUT_TEST_ACCEPTANCE_ENGINE}"
    SOURCES ${engine_acceptance_sources}
    LIBRARIES ${LINKAGE_PUBLIC_TEST_ACCEPTANCE_ENGINE}
    PRIVATE_INCLUDES "${CHERYL_ENGINE_PRIVATE_SOURCE_DIR}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(${TARGET_TEST_ACCEPTANCE_ENGINE} PRIVATE -rdynamic)
endif()
if(MSVC)
    # Stack-frame names need symbol ranges covering the captured return addresses.
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../all-tests/src/extern/backward-cpp.cpp"
        APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release,MinSizeRel>:/Z7>")
    foreach(runner IN ITEMS ${TARGET_TEST_ACCEPTANCE_ENGINE} ${CHERYL_TEST_AGGREGATE})
        target_link_options(${runner} PRIVATE "$<$<CONFIG:Release,MinSizeRel>:/DEBUG:FULL>")
    endforeach()
endif()
