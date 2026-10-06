cheryl_add_test_suite(${TARGET_TEST_ACCEPTANCE_ENGINE} ACCEPTANCE
    OUTPUT_NAME "${OUTPUT_TEST_ACCEPTANCE_ENGINE}"
    SOURCES
        ../all-tests/src/core/event-bus.cpp
        ../all-tests/src/core/runtime-adapter.cpp
        ../all-tests/src/core/worker-pool.cpp
        ../all-tests/src/diagnostics.cpp
        ../all-tests/src/extern/backward-cpp.cpp
        ../all-tests/src/extern/ctti.cpp
        ../all-tests/src/extern/glm.cpp
        ../all-tests/src/extern/spdlog.cpp
        ../all-tests/src/resources/asset-cache.cpp
        ../all-tests/src/resources/fonts.cpp
        ../all-tests/src/resources/memory.cpp
        ../all-tests/src/resources/reservations.cpp
        ../all-tests/src/templates/block.cpp
        ../all-tests/src/templates/singleton.cpp
    LIBRARIES ${LINKAGE_PUBLIC_TEST_ACCEPTANCE_ENGINE}
    PRIVATE_INCLUDES "${CHERYL_ENGINE_PRIVATE_SOURCE_DIR}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(${TARGET_TEST_ACCEPTANCE_ENGINE} PRIVATE -rdynamic)
endif()
