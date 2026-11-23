if(NOT TARGET Cheryl::Engine OR NOT TARGET Cheryl::UI::RmlUi)
    message(FATAL_ERROR "Cheryl::Console requires Engine and RmlUi; enable CHERYL_BUILD_UI_RMLUI for the complete console")
endif()

add_library(${TARGET_LIB_MODULE_CONSOLE} ${CHERYL_LIBRARY_TYPE}
    src/registry.cpp src/parser.cpp src/session.cpp
    src/scenario.cpp src/scenario-parser.cpp src/setting.cpp
    src/model.cpp src/view.cpp src/input.cpp
    src/runtime/runner.cpp src/runtime/report.cpp src/runtime/io.cpp
    src/runtime/command.cpp src/runtime/diagnostics.cpp)
add_library(Cheryl::Console ALIAS ${TARGET_LIB_MODULE_CONSOLE})
cheryl_register_public_component(${TARGET_LIB_MODULE_CONSOLE} FEATURE CHERYL_HAS_CONSOLE)
target_compile_features(${TARGET_LIB_MODULE_CONSOLE} PUBLIC cxx_std_23)
target_include_directories(${TARGET_LIB_MODULE_CONSOLE} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(${TARGET_LIB_MODULE_CONSOLE} PUBLIC Cheryl::Engine Cheryl::UI::RmlUi)
set_target_properties(${TARGET_LIB_MODULE_CONSOLE} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_CONSOLE}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_CONSOLE}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
cheryl_configure_library(${TARGET_LIB_MODULE_CONSOLE} CONSOLE)
