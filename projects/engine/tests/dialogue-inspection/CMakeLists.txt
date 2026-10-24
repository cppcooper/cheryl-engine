# Independent header consumer: no Engine private/app/JSON include or source copy.
cheryl_add_test_suite(tests-dialogue-inspection
    SOURCES consumer.cpp lookup.cpp
    LIBRARIES Cheryl::Engine)
if(CHERYL_BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(tests-dialogue-inspection-cases PRIVATE
        DIALOGUE_INSPECTION_ENGINE="$<TARGET_FILE:Cheryl::Engine>")
    target_link_libraries(tests-dialogue-inspection-cases PRIVATE ${CMAKE_DL_LIBS})
endif()
