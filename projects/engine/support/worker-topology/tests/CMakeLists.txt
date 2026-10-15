if(TARGET Cheryl::WorkerTopology::Json)
    cheryl_add_test_suite(${TARGET_TEST_WORKER_TOPOLOGY_JSON}
        OUTPUT_NAME "${OUTPUT_TEST_WORKER_TOPOLOGY_JSON}"
        SOURCES src/configuration.cpp
        LIBRARIES ${LINKAGE_PUBLIC_TEST_WORKER_TOPOLOGY_JSON})
endif()

if(TARGET Cheryl::WorkerTopology::Hwloc)
    get_directory_property(worker_hwloc_sdk
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.." DEFINITION cheryl_hwloc_target)
    cheryl_add_test_suite(${TARGET_TEST_ACCEPTANCE_WORKER_TOPOLOGY_HWLOC} ACCEPTANCE
        OUTPUT_NAME "${OUTPUT_TEST_ACCEPTANCE_WORKER_TOPOLOGY_HWLOC}"
        SOURCES src/hwloc.cpp
        LIBRARIES ${LINKAGE_PUBLIC_TEST_ACCEPTANCE_WORKER_TOPOLOGY_HWLOC} ${worker_hwloc_sdk})
endif()

if(TARGET ${TARGET_LIB_ENGINE_S_WORKER_TOPOLOGY_EXAMPLE})
    cheryl_add_test_suite(${TARGET_TEST_WORKER_TOPOLOGY_COMPOSITION}
        OUTPUT_NAME "${OUTPUT_TEST_WORKER_TOPOLOGY_COMPOSITION}"
        SOURCES src/composition.cpp
        LIBRARIES ${LINKAGE_PUBLIC_TEST_WORKER_TOPOLOGY_COMPOSITION})
endif()

if(CHERYL_BUILD_CONSUMER_TESTS)
    include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylConsumers.cmake")
    add_executable(${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY} consumer.cpp)
    set_target_properties(${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY} PROPERTIES
        OUTPUT_NAME "${OUTPUT_TEST_CONSUMER_WORKER_TOPOLOGY}"
        RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
    set(worker_headers)
    set(worker_libraries)
    if(TARGET Cheryl::WorkerTopology::Json)
        list(APPEND worker_libraries Cheryl::WorkerTopology::Json)
        list(APPEND worker_headers cheryl/core/engine/worker-topology/configuration.h)
        target_compile_definitions(${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY} PRIVATE CHERYL_CONSUMER_WORKER_JSON=1)
    endif()
    if(TARGET Cheryl::WorkerTopology::Hwloc)
        list(APPEND worker_libraries Cheryl::WorkerTopology::Hwloc)
        list(APPEND worker_headers cheryl/core/engine/worker-topology/hwloc.h)
        target_compile_definitions(${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY} PRIVATE CHERYL_CONSUMER_WORKER_HWLOC=1)
    endif()
    target_link_libraries(${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY} PRIVATE ${worker_libraries})
    cheryl_add_header_probes(${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY_HEADERS}
        "${worker_libraries}" NEUTRAL HEADERS ${worker_headers})
    add_dependencies(${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY} ${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY_HEADERS})
    add_test(NAME ${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY} COMMAND ${TARGET_TEST_CONSUMER_WORKER_TOPOLOGY})
endif()
