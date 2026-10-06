include_guard(GLOBAL)

# Engine
set(TARGET_LIB_ENGINE cengine)
set(TARGET_LIB_ENGINE_LOGGING_CONFIG cengine_logging_config)
set(TARGET_LIB_ENGINE_SIGNAL_HANDLERS cengine_signal_handlers)

# Modules
set(TARGET_LIB_MODULE_NATIVE_GLFW module_native_glfw)
set(TARGET_LIB_MODULE_OPENGL module_opengl)
set(TARGET_LIB_MODULE_UI_TGUI module_ui_tgui)
set(TARGET_LIB_MODULE_UI_RMLUI module_ui_rmlui)

# Applications
set(TARGET_APP_DEMO demo)

# Aggregate tests
set(TARGET_TEST_ALL_ENGINE engine-all)
set(TARGET_TEST_ALL_MODULE_NATIVE_GLFW native-glfw-all)
set(TARGET_TEST_ALL_MODULE_OPENGL opengl-all)
set(TARGET_TEST_ALL_MODULE_UI_TGUI ui-tgui-all)
set(TARGET_TEST_ALL_MODULE_UI_RMLUI ui-rmlui-all)
set(TARGET_TEST_ALL_TESTS all-tests)

# Unit tests
set(TARGET_TEST_ENGINE engine-tests)
set(TARGET_TEST_MODULE_NATIVE_GLFW native-glfw-tests)
set(TARGET_TEST_MODULE_OPENGL opengl-tests)
set(TARGET_TEST_MODULE_UI_TGUI ui-tgui-tests)
set(TARGET_TEST_MODULE_UI_RMLUI ui-rmlui-tests)

# Acceptance tests
set(TARGET_TEST_ACCEPTANCE_ENGINE engine-acceptance)
set(TARGET_TEST_ACCEPTANCE_OPENGL opengl-acceptance)

# Cross-module tests
set(TARGET_TEST_UI_COEXIST ui-coexist-tests)

# Dependency checks
set(TARGET_TEST_DEPENDENCY_BACKWARD_CPP backward-cpp)