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
set(TARGET_LIB_MODULE_AUDIO_MINIAUDIO module_audio_miniaudio)
set(TARGET_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK module_audio_miniaudio_sdk)

# OpenGL support
set(TARGET_LIB_MODULE_OPENGL_S_GL46 gl46)

# Applications
set(TARGET_APP_DEMO demo)

# Test support
set(TARGET_LIB_TEST_SUPPORT cheryl_test_support)
set(TARGET_LIB_TEST_MAIN cheryl_test_main)

# Aggregate tests
set(TARGET_TEST_ALL_TESTS all-tests)
set(TARGET_TEST_ALL_ENGINE all-engine)
set(TARGET_TEST_ALL_MODULE_NATIVE_GLFW all-native-glfw)
set(TARGET_TEST_ALL_MODULE_OPENGL all-opengl)
set(TARGET_TEST_ALL_MODULE_UI_TGUI all-ui-tgui)
set(TARGET_TEST_ALL_MODULE_UI_RMLUI all-ui-rmlui)
set(TARGET_TEST_ALL_MODULE_AUDIO_MINIAUDIO all-audio-miniaudio)

# Unit tests
set(TARGET_TEST_ENGINE tests-engine)
set(TARGET_TEST_ENGINE_LOGGING tests-logging)
set(TARGET_TEST_MODULE_NATIVE_GLFW tests-native-glfw)
set(TARGET_TEST_NATIVE_JOYSTICK tests-native-joystick)
set(TARGET_TEST_MODULE_OPENGL tests-opengl)
set(TARGET_TEST_MODULE_UI_TGUI tests-ui-tgui)
set(TARGET_TEST_MODULE_UI_RMLUI tests-ui-rmlui)
set(TARGET_TEST_MODULE_AUDIO_MINIAUDIO tests-audio-miniaudio)

# Acceptance tests
set(TARGET_TEST_ACCEPTANCE_ENGINE acceptance-engine)
set(TARGET_TEST_ACCEPTANCE_OPENGL acceptance-opengl)
set(TARGET_TEST_ACCEPTANCE_ENGINE_LOGGING acceptance-logging)
set(TARGET_TEST_ACCEPTANCE_ENGINE_SIGNAL acceptance-signal)

# Cross-module tests
set(TARGET_TEST_UI_COEXIST tests-ui-coexist)

# Dependency checks
set(TARGET_TEST_DEPENDENCY_BACKWARD_CPP backward-cpp)

# Consumer tests
set(TARGET_TEST_CONSUMER_ENGINE consumer-cengine)
set(TARGET_TEST_CONSUMER_MODULE_NATIVE_GLFW consumer-module-native-glfw)
set(TARGET_TEST_CONSUMER_MODULE_OPENGL consumer-module-opengl)
set(TARGET_TEST_CONSUMER_MODULE_UI_TGUI consumer-module-ui-tgui)
set(TARGET_TEST_CONSUMER_MODULE_UI_RMLUI consumer-module-ui-rmlui)
set(TARGET_TEST_CONSUMER_MODULE_AUDIO_MINIAUDIO consumer-module-audio-miniaudio)

# Header probes
set(TARGET_TEST_CONSUMER_ENGINE_HEADERS consumer-headers-cengine)
set(TARGET_TEST_CONSUMER_MODULE_NATIVE_GLFW_HEADERS consumer-module-headers-native-glfw)
set(TARGET_TEST_CONSUMER_MODULE_OPENGL_HEADERS consumer-module-headers--opengl)
set(TARGET_TEST_CONSUMER_MODULE_UI_TGUI_HEADERS consumer-module-headers--ui-tgui)
set(TARGET_TEST_CONSUMER_MODULE_UI_RMLUI_HEADERS consumer-module-headers--ui-rmlui)
set(TARGET_TEST_CONSUMER_MODULE_AUDIO_MINIAUDIO_HEADERS consumer-module-headers-audio-miniaudio)
