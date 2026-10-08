# Reload declarations into the caller's scope, including standalone entry points.

# Engine
set(OUTPUT_LIB_ENGINE cheryl-engine)

# Engine support
set(OUTPUT_LIB_ENGINE_S_STARTUP cheryl-engine-startup)

# Modules
set(OUTPUT_LIB_MODULE_NATIVE_GLFW cheryl-module-native-glfw)
set(OUTPUT_LIB_MODULE_OPENGL cheryl-module-opengl)
set(OUTPUT_LIB_MODULE_UI_TGUI cheryl-module-ui-tgui)
set(OUTPUT_LIB_MODULE_UI_RMLUI cheryl-module-ui-rmlui)
set(OUTPUT_LIB_MODULE_AUDIO_MINIAUDIO cheryl-module-audio-miniaudio)

# Module support
set(OUTPUT_LIB_MODULE_OPENGL_S_GL46 gl46)
set(OUTPUT_LIB_MODULE_OPENGL_S_STARTUP cheryl-module-opengl-startup)
set(OUTPUT_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK cheryl-miniaudio-sdk)

# Applications
set(OUTPUT_APP_DEMO demo)

# Aggregate tests
set(OUTPUT_TEST_ALL_ENGINE tests-all-engine)
set(OUTPUT_TEST_ALL_MODULE_NATIVE_GLFW tests-all-native-glfw)
set(OUTPUT_TEST_ALL_MODULE_OPENGL tests-all-opengl)
set(OUTPUT_TEST_ALL_MODULE_UI_TGUI tests-all-ui-tgui)
set(OUTPUT_TEST_ALL_MODULE_UI_RMLUI tests-all-ui-rmlui)
set(OUTPUT_TEST_ALL_MODULE_AUDIO_MINIAUDIO tests-all-audio-miniaudio)
set(OUTPUT_TEST_ALL_TESTS tests-all)

# Unit tests
set(OUTPUT_TEST_ENGINE tests-engine)
set(OUTPUT_TEST_ENGINE_LOGGING tests-engine-logging)
set(OUTPUT_TEST_MODULE_NATIVE_GLFW tests-native-glfw)
set(OUTPUT_TEST_NATIVE_JOYSTICK tests-native-joystick)
set(OUTPUT_TEST_MODULE_OPENGL tests-opengl)
set(OUTPUT_TEST_MODULE_UI_TGUI tests-ui-tgui)
set(OUTPUT_TEST_MODULE_UI_RMLUI tests-ui-rmlui)
set(OUTPUT_TEST_MODULE_AUDIO_MINIAUDIO tests-audio-miniaudio)

# Acceptance tests
set(OUTPUT_TEST_ACCEPTANCE_ENGINE tests-acceptance-engine)
set(OUTPUT_TEST_ACCEPTANCE_ENGINE_LOGGING tests-acceptance-engine-logging)
set(OUTPUT_TEST_ACCEPTANCE_ENGINE_SIGNAL tests-acceptance-engine-signal)
set(OUTPUT_TEST_ACCEPTANCE_OPENGL tests-acceptance-opengl)

# Cross-module tests
set(OUTPUT_TEST_UI_COEXIST tests-ui-coexist)

# Dependency checks
set(OUTPUT_TEST_DEPENDENCY_BACKWARD_CPP tests-dependency-backward-cpp)

# Consumer tests
set(OUTPUT_TEST_CONSUMER_ENGINE cheryl-consumer)
set(OUTPUT_TEST_CONSUMER_MODULE_NATIVE_GLFW cheryl-native-glfw-consumer)
set(OUTPUT_TEST_CONSUMER_MODULE_OPENGL cheryl-opengl-consumer)
set(OUTPUT_TEST_CONSUMER_MODULE_UI_TGUI cheryl-ui-tgui-consumer)
set(OUTPUT_TEST_CONSUMER_MODULE_UI_RMLUI cheryl-ui-rmlui-consumer)
set(OUTPUT_TEST_CONSUMER_MODULE_AUDIO_MINIAUDIO cheryl-audio-miniaudio-consumer)
