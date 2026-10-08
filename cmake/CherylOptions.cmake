# CherylOptions.cmake

option(WARN
        "Enable compiler warnings [-Wall]" OFF)

option(CHERYL_BUILD_NATIVE_GLFW
        "Build the native GLFW window/input integration" ON)
option(CHERYL_SANDBOX_BUILD
        "Deprecated native GLFW null-platform selection without Gainput" OFF)
option(CHERYL_BUILD_OPENGL
        "Build the OpenGL backend" ON)
option(CHERYL_BUILD_UI_TGUI
        "Build the optional TGUI UI integration" ON)
option(CHERYL_BUILD_UI_RMLUI
        "Build the optional RmlUi UI integration" ON)
option(CHERYL_BUILD_AUDIO_MINIAUDIO
        "Build the optional miniaudio playback integration" OFF)

option(CHERYL_BUILD_DEMO
        "Build the windowed demonstration application" ON)

option(CHERYL_BUILD_TESTS
        "Build Cheryl's tests and acceptance tools" ON)
option(CHERYL_BUILD_ALL_TESTS
        "Include owner and cross-project aggregate test runners in the default build" OFF)
option(CHERYL_BUILD_ACCEPTANCE_TESTS
        "Include broader acceptance runners in the default build" OFF)
option(CHERYL_BUILD_CONSUMER_TESTS
        "Build the consumer and public-header checks" OFF)
