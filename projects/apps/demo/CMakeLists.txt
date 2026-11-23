project(cheryl_demo VERSION ${VERSION_PROJECT_APP_DEMO} LANGUAGES CXX)

file(GLOB_RECURSE cheryl_demo_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER cheryl_demo_sources EXCLUDE REGEX "/developer-console-model\\.cpp$")
if(NOT CHERYL_DEMO_CONSOLE)
    list(FILTER cheryl_demo_sources EXCLUDE REGEX "/developer-console\\.cpp$")
    list(FILTER cheryl_demo_sources EXCLUDE REGEX "/developer-console-routing\\.cpp$")
endif()
if(NOT TARGET Cheryl::UI::TGUI)
    list(FILTER cheryl_demo_sources EXCLUDE REGEX "/tgui-demo\\.cpp$")
    list(FILTER cheryl_demo_sources EXCLUDE REGEX "/tgui-controls\\.cpp$")
endif()
if(NOT TARGET Cheryl::UI::RmlUi)
    list(FILTER cheryl_demo_sources EXCLUDE REGEX "/rmlui-demo\\.cpp$")
endif()

add_executable(${TARGET_APP_DEMO} ${cheryl_demo_sources})
set_target_properties(${TARGET_APP_DEMO} PROPERTIES
    OUTPUT_NAME "${OUTPUT_APP_DEMO}"
    RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_SOURCE_DIR="${CHERYL_REPOSITORY_ROOT}")
target_link_libraries(${TARGET_APP_DEMO} PRIVATE ${LINKAGE_COMPOSITION_APP_DEMO_BASE})

if(CHERYL_DEMO_CONSOLE)
    foreach(required_target IN ITEMS Cheryl::Console
            Cheryl::LivingWorld::Host Cheryl::LivingWorld::Console Cheryl::LivingWorld::Scenarios)
        if(NOT TARGET ${required_target})
            message(FATAL_ERROR "CHERYL_DEMO_CONSOLE requires the actual target ${required_target}; select its explicit build options")
        endif()
    endforeach()
    add_library(cheryl_demo_console_model STATIC src/developer-console-model.cpp)
    target_include_directories(cheryl_demo_console_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(cheryl_demo_console_model PUBLIC Cheryl::LivingWorld::Scenarios Cheryl::Console)
    target_link_libraries(${TARGET_APP_DEMO} PRIVATE cheryl_demo_console_model)
    target_compile_definitions(${TARGET_APP_DEMO} PRIVATE
        CHERYL_DEMO_CONSOLE=1
        CHERYL_DEMO_SCRIPT_ROOT="${CHERYL_REPOSITORY_ROOT}/projects/tests/living-world/tests/scenarios/fixtures"
        CHERYL_DEMO_MAGIC_ROOT="${CHERYL_REPOSITORY_ROOT}/projects/magic/fixtures"
        CHERYL_DEMO_OUTPUT_ROOT="${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(TARGET Cheryl::Audio::Miniaudio)
    target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_DEMO_AUDIO=1)
    target_link_libraries(${TARGET_APP_DEMO} PRIVATE ${LINKAGE_COMPOSITION_APP_DEMO_AUDIO})
endif()

if(TARGET Cheryl::UI::TGUI)
    target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_DEMO_TGUI=1)
    target_link_libraries(${TARGET_APP_DEMO} PRIVATE ${LINKAGE_COMPOSITION_APP_DEMO_UI_TGUI})
endif()

if(TARGET Cheryl::UI::RmlUi)
    target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_DEMO_RMLUI=1)
    target_link_libraries(${TARGET_APP_DEMO} PRIVATE ${LINKAGE_COMPOSITION_APP_DEMO_UI_RMLUI})
endif()

if(CHERYL_DEMO_CONSOLE)
    add_subdirectory(tests)
endif()
