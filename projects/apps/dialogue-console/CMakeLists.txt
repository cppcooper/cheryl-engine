# Private headless consumer; no module, SDK export or public JSON dependency.
add_library(dialogue-console-state STATIC src/gate.cpp src/codec.cpp src/files.cpp src/definition-codec.cpp)
target_compile_features(dialogue-console-state PRIVATE cxx_std_23)
target_link_libraries(dialogue-console-state PRIVATE Cheryl::Engine)
target_include_directories(dialogue-console-state PRIVATE "${CHERYL_REPOSITORY_ROOT}/extern/json/single_include")

add_executable(dialogue-console src/main.cpp src/console.cpp)
target_compile_features(dialogue-console PRIVATE cxx_std_23)
target_link_libraries(dialogue-console PRIVATE dialogue-console-state Cheryl::Engine)
set_target_properties(dialogue-console PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")

if(CHERYL_BUILD_TESTS AND TARGET "${TARGET_TEST_ALL_TESTS}")
    set(CHERYL_TEST_AGGREGATE ${TARGET_TEST_ALL_TESTS})
    cheryl_add_test_suite(tests-dialogue-console
        SOURCES tests/state.cpp tests/codec.cpp tests/files.cpp tests/definition-codec.cpp
        LIBRARIES dialogue-console-state Cheryl::Engine
        PRIVATE_INCLUDES "${CHERYL_REPOSITORY_ROOT}/extern/json/single_include")
    if(UNIX)
        target_sources(tests-dialogue-console-cases PRIVATE tests/transcripts.cpp)
        target_compile_definitions(tests-dialogue-console-cases PRIVATE
            DIALOGUE_CONSOLE_EXECUTABLE="$<TARGET_FILE:dialogue-console>")
        add_dependencies(tests-dialogue-console-cases dialogue-console)
    endif()
    target_compile_definitions(tests-dialogue-console-cases PRIVATE
        DIALOGUE_CONSOLE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
endif()
