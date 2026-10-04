# Selected owners register their reusable case targets before this assembly.
cheryl_add_test_aggregate(all-tests EXPORT_SYMBOLS)
get_property(project_aggregates GLOBAL PROPERTY CHERYL_TEST_AGGREGATES)
foreach(project_aggregate IN LISTS project_aggregates)
    get_target_property(case_targets ${project_aggregate} CHERYL_TEST_CASE_TARGETS)
    if(case_targets)
        target_link_libraries(all-tests PRIVATE ${case_targets})
    endif()
endforeach()
