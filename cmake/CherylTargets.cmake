# Reload declarations into the caller's scope, including standalone entry points.

# Engine
set(TARGET_LIB_ENGINE cengine)

# Engine support
set(TARGET_LIB_ENGINE_S_LOGGING_CONFIG cengine_logging_config)
set(TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS cengine_signal_handlers)

# Modules
set(TARGET_LIB_MODULE_NATIVE_GLFW module_native_glfw)
set(TARGET_LIB_MODULE_OPENGL module_opengl)
set(TARGET_LIB_MODULE_UI_TGUI module_ui_tgui)
set(TARGET_LIB_MODULE_UI_RMLUI module_ui_rmlui)

# OpenGL support
set(TARGET_LIB_MODULE_OPENGL_S_GL46 gl46)

# Applications
set(TARGET_APP_DEMO demo)

# Test support
set(TARGET_LIB_TEST_SUPPORT cheryl_test_support)
set(TARGET_LIB_TEST_MAIN cheryl_test_main)

# Aggregate tests
set(TARGET_TEST_ALL_ENGINE engine-all)
set(TARGET_TEST_ALL_MODULE_NATIVE_GLFW native-glfw-all)
set(TARGET_TEST_ALL_MODULE_OPENGL opengl-all)
set(TARGET_TEST_ALL_MODULE_UI_TGUI ui-tgui-all)
set(TARGET_TEST_ALL_MODULE_UI_RMLUI ui-rmlui-all)
set(TARGET_TEST_ALL_TESTS all-tests)

# Unit tests
set(TARGET_TEST_ENGINE engine-tests)
set(TARGET_TEST_ENGINE_LOGGING logging-tests)
set(TARGET_TEST_MODULE_NATIVE_GLFW native-glfw-tests)
set(TARGET_TEST_MODULE_OPENGL opengl-tests)
set(TARGET_TEST_MODULE_UI_TGUI ui-tgui-tests)
set(TARGET_TEST_MODULE_UI_RMLUI ui-rmlui-tests)

# Acceptance tests
set(TARGET_TEST_ACCEPTANCE_ENGINE engine-acceptance)
set(TARGET_TEST_ACCEPTANCE_ENGINE_LOGGING cheryl-logging-acceptance)
set(TARGET_TEST_ACCEPTANCE_ENGINE_SIGNAL cheryl-signal-acceptance)
set(TARGET_TEST_ACCEPTANCE_OPENGL opengl-acceptance)

# Cross-module tests
set(TARGET_TEST_UI_COEXIST ui-coexist-tests)

# Dependency checks
set(TARGET_TEST_DEPENDENCY_BACKWARD_CPP backward-cpp)

# Consumer tests and header probes
set(TARGET_TEST_CONSUMER_ENGINE cheryl-consumer)
set(TARGET_TEST_CONSUMER_ENGINE_HEADERS cheryl-consumer-headers)
set(TARGET_TEST_CONSUMER_MODULE_NATIVE_GLFW cheryl-native-glfw-consumer)
set(TARGET_TEST_CONSUMER_MODULE_NATIVE_GLFW_HEADERS cheryl-native-glfw-consumer-headers)
set(TARGET_TEST_CONSUMER_MODULE_OPENGL cheryl-opengl-consumer)
set(TARGET_TEST_CONSUMER_MODULE_OPENGL_HEADERS cheryl-opengl-consumer-headers)
set(TARGET_TEST_CONSUMER_MODULE_UI_TGUI cheryl-ui-tgui-consumer)
set(TARGET_TEST_CONSUMER_MODULE_UI_TGUI_HEADERS cheryl-ui-tgui-consumer-headers)
set(TARGET_TEST_CONSUMER_MODULE_UI_RMLUI cheryl-ui-rmlui-consumer)
set(TARGET_TEST_CONSUMER_MODULE_UI_RMLUI_HEADERS cheryl-ui-rmlui-consumer-headers)
