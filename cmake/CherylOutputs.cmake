include_guard(GLOBAL)

# Engine
set(OUTPUT_LIB_ENGINE cheryl-engine)

# Modules
set(OUTPUT_LIB_MODULE_NATIVE_GLFW cheryl-module-native-glfw)
set(OUTPUT_LIB_MODULE_OPENGL cheryl-module-opengl)
set(OUTPUT_LIB_MODULE_UI_TGUI cheryl-module-ui-tgui)
set(OUTPUT_LIB_MODULE_UI_RMLUI cheryl-module-ui-rmlui)

# Applications
set(OUTPUT_APP_DEMO demo)

# Aggregate tests
set(OUTPUT_TEST_ALL_ENGINE tests-all-engine)
set(OUTPUT_TEST_ALL_MODULE_NATIVE_GLFW tests-all-native-glfw)
set(OUTPUT_TEST_ALL_MODULE_OPENGL tests-all-opengl)
set(OUTPUT_TEST_ALL_MODULE_UI_TGUI tests-all-ui-tgui)
set(OUTPUT_TEST_ALL_MODULE_UI_RMLUI tests-all-ui-rmlui)
set(OUTPUT_TEST_ALL_TESTS tests-all)

# Unit tests
set(OUTPUT_TEST_ENGINE tests-engine)
set(OUTPUT_TEST_MODULE_NATIVE_GLFW tests-native-glfw)
set(OUTPUT_TEST_MODULE_OPENGL tests-opengl)
set(OUTPUT_TEST_MODULE_UI_TGUI tests-ui-tgui)
set(OUTPUT_TEST_MODULE_UI_RMLUI tests-ui-rmlui)

# Acceptance tests
set(OUTPUT_TEST_ACCEPTANCE_ENGINE tests-acceptance-engine)
set(OUTPUT_TEST_ACCEPTANCE_OPENGL tests-acceptance-opengl)

# Cross-module tests
set(OUTPUT_TEST_UI_COEXIST tests-ui-coexist)

# Dependency checks
set(OUTPUT_TEST_DEPENDENCY_BACKWARD_CPP tests-dependency-backward-cpp)