cheryl_add_test_aggregate(tests-demo-console OWNER)
set(CHERYL_TEST_AGGREGATE tests-demo-console)
include("${CHERYL_REPOSITORY_ROOT}/projects/modules/ui/rmlui/cmake/TestFont.cmake")
cheryl_rmlui_test_font(demo_console_test_font)
set(demo_console_sources developer-console.cpp developer-console-input.cpp
    ../src/developer-console.cpp ../src/developer-console-routing.cpp)
set(demo_console_libraries cheryl_demo_console_model Cheryl::UI::RmlUi)
set(demo_console_definitions
    DEMO_CONSOLE_SCRIPT_ROOT="${CHERYL_REPOSITORY_ROOT}/projects/tests/living-world/tests/scenarios/fixtures"
    DEMO_CONSOLE_MAGIC_ROOT="${CHERYL_REPOSITORY_ROOT}/projects/magic/fixtures"
    DEMO_CONSOLE_TEST_FONT="${demo_console_test_font}")
if(TARGET Cheryl::UI::TGUI)
    list(APPEND demo_console_sources developer-console-coexistence.cpp ../src/tgui-controls.cpp)
    list(APPEND demo_console_libraries Cheryl::UI::TGUI)
    list(APPEND demo_console_definitions CHERYL_DEMO_TGUI=1)
endif()
cheryl_add_test_suite(tests-demo-console-controller
    SOURCES ${demo_console_sources}
    LIBRARIES ${demo_console_libraries}
    DEFINITIONS ${demo_console_definitions})
add_test(NAME demo_console COMMAND tests-demo-console-controller "--gtest_filter=DemoConsole.*:DemoConsoleInput.*")
set_tests_properties(demo_console PROPERTIES TIMEOUT 30 LABELS "demo;console;contact;rmlui")

if(TARGET Cheryl::UI::TGUI)
    add_test(NAME demo_console_coexistence COMMAND tests-demo-console-controller
        "--gtest_filter=DemoConsoleCoexistence.*")
    set_tests_properties(demo_console_coexistence PROPERTIES TIMEOUT 30 LABELS "demo;console;contact;rmlui;tgui;coexistence")
endif()
