project(cheryl_demo VERSION ${VERSION_PROJECT_APP_DEMO} LANGUAGES CXX)

file(GLOB_RECURSE cheryl_demo_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(NOT TARGET Cheryl::UI::TGUI)
    list(FILTER cheryl_demo_sources EXCLUDE REGEX "/tgui-demo\\.cpp$")
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

if(TARGET Cheryl::Audio::Miniaudio)
    target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_DEMO_AUDIO=1)
    target_link_libraries(${TARGET_APP_DEMO} PRIVATE Cheryl::Audio::Miniaudio)
endif()

if(TARGET Cheryl::UI::TGUI)
    target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_DEMO_TGUI=1)
    target_link_libraries(${TARGET_APP_DEMO} PRIVATE ${LINKAGE_COMPOSITION_APP_DEMO_UI_TGUI})
endif()

if(TARGET Cheryl::UI::RmlUi)
    target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_DEMO_RMLUI=1)
    target_link_libraries(${TARGET_APP_DEMO} PRIVATE ${LINKAGE_COMPOSITION_APP_DEMO_UI_RMLUI})
endif()
