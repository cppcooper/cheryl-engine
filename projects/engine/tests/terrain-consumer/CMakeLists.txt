include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylConsumers.cmake")

add_executable(tests-terrain-consumer main.cpp)
target_link_libraries(tests-terrain-consumer PRIVATE Cheryl::Engine)
cheryl_add_header_probes(tests-terrain-header Cheryl::Engine NEUTRAL
    HEADERS cheryl/maps/terrain-map.h)
add_dependencies(tests-terrain-consumer tests-terrain-header)

foreach(check IN ITEMS values errors refusal)
    add_test(NAME tests-terrain-consumer.${check}
        COMMAND tests-terrain-consumer ${check})
endforeach()
