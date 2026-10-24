# Private offline proof; no JSON/provider/UI/runtime module or SDK export.
add_library(story-offline-model STATIC src/validation.cpp src/encounter.cpp src/fixtures.cpp src/demo.cpp
        src/continuation.cpp src/continuation-fixtures.cpp src/continuation-demo.cpp
        src/bargain.cpp src/bargain-fixtures.cpp src/bargain-demo.cpp
        src/selection.cpp src/selection-demo.cpp src/payoff.cpp src/payoff-demo.cpp src/play.cpp)
target_compile_features(story-offline-model PRIVATE cxx_std_23)
target_link_libraries(story-offline-model PRIVATE Cheryl::Engine)
add_executable(story-offline src/main.cpp)
target_link_libraries(story-offline PRIVATE story-offline-model Cheryl::Engine)
set_target_properties(story-offline PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
if(CHERYL_BUILD_TESTS AND TARGET "${TARGET_TEST_ALL_TESTS}")
    set(CHERYL_TEST_AGGREGATE ${TARGET_TEST_ALL_TESTS})
    cheryl_add_test_suite(tests-story-offline SOURCES tests/story.cpp tests/continuation.cpp tests/bargain.cpp tests/selection.cpp tests/payoff.cpp tests/play.cpp LIBRARIES story-offline-model Cheryl::Engine)
    if(UNIX)
        target_sources(tests-story-offline-cases PRIVATE tests/transcript.cpp)
        target_compile_definitions(tests-story-offline-cases PRIVATE STORY_OFFLINE_EXECUTABLE="$<TARGET_FILE:story-offline>")
        add_dependencies(tests-story-offline-cases story-offline)
    endif()
endif()
