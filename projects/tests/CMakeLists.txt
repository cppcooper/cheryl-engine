# Selected owners register their reusable case targets before this assembly.
cheryl_add_test_aggregate(${TARGET_TEST_ALL_TESTS}
    OUTPUT_NAME "${OUTPUT_TEST_ALL_TESTS}" EXPORT_SYMBOLS)
if(TARGET Cheryl::UI::TGUI AND TARGET Cheryl::UI::RmlUi)
    set(CHERYL_TEST_AGGREGATE ${TARGET_TEST_ALL_TESTS})
    add_subdirectory(ui-coexist)
endif()
get_property(project_aggregates GLOBAL PROPERTY CHERYL_TEST_AGGREGATES)
foreach(project_aggregate IN LISTS project_aggregates)
    get_target_property(case_targets ${project_aggregate} CHERYL_TEST_CASE_TARGETS)
    if(case_targets)
        target_link_libraries(${TARGET_TEST_ALL_TESTS} PRIVATE ${case_targets})
    endif()
endforeach()
