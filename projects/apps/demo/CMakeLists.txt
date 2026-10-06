project(cheryl_demo VERSION ${VERSION_PROJECT_APP_DEMO} LANGUAGES CXX)

add_executable(${TARGET_APP_DEMO} src/main.cpp)
set_target_properties(${TARGET_APP_DEMO} PROPERTIES
    OUTPUT_NAME "${OUTPUT_APP_DEMO}"
    RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_SOURCE_DIR="${CHERYL_REPOSITORY_ROOT}")
target_link_libraries(${TARGET_APP_DEMO} PRIVATE ${LINKAGE_COMPOSITION_APP_DEMO_BASE})

if(TARGET Cheryl::UI::TGUI)
    target_sources(${TARGET_APP_DEMO} PRIVATE src/tgui-demo.cpp)
    target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_DEMO_TGUI=1)
    target_link_libraries(${TARGET_APP_DEMO} PRIVATE ${LINKAGE_COMPOSITION_APP_DEMO_UI_TGUI})
endif()

if(TARGET Cheryl::UI::RmlUi)
    target_sources(${TARGET_APP_DEMO} PRIVATE src/rmlui-demo.cpp)
    target_compile_definitions(${TARGET_APP_DEMO} PRIVATE CHERYL_DEMO_RMLUI=1)
    target_link_libraries(${TARGET_APP_DEMO} PRIVATE ${LINKAGE_COMPOSITION_APP_DEMO_UI_RMLUI})
endif()
