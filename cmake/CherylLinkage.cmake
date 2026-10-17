# Reload declarations into the caller's scope, including standalone entry points.

include("${CMAKE_CURRENT_LIST_DIR}/CherylTargets.cmake")


# Engine
#########
set(LINKAGE_PUBLIC_LIB_ENGINE
        Threads::Threads
        ${TARGET_LIB_ENGINE_S_LOGGING_CONFIG}
        ctti
        glm::glm-header-only
)

set(LINKAGE_INTERFACE_LIB_ENGINE
        "$<TARGET_OBJECTS:${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS}>"
)

set(LINKAGE_PRIVATE_LIB_ENGINE
        spdlog::spdlog
        Backward::Interface
        Freetype::Freetype
        harfbuzz::harfbuzz
        Cheryl::ICU
)

set(LINKAGE_PUBLIC_LIB_ENGINE_S_SIGNAL_HANDLERS
        Backward::Interface
)

set(LINKAGE_PUBLIC_LIB_ENGINE_S_STARTUP
        Cheryl::Engine
        CLI11::CLI11
)

set(LINKAGE_PUBLIC_LIB_ENGINE_S_WORKER_TOPOLOGY_HWLOC
        Cheryl::Engine)
set(LINKAGE_PUBLIC_LIB_ENGINE_S_WORKER_TOPOLOGY_JSON
        Cheryl::Engine)
set(LINKAGE_PUBLIC_LIB_ENGINE_S_WORKER_TOPOLOGY_EXAMPLE
        Cheryl::WorkerTopology::Hwloc
        Cheryl::WorkerTopology::Json)

# Native GLFW module
#####################
set(LINKAGE_PUBLIC_LIB_MODULE_NATIVE_GLFW
        Cheryl::Engine
)

set(LINKAGE_PRIVATE_LIB_MODULE_NATIVE_GLFW
        glfw
)

set(LINKAGE_PUBLIC_LIB_MODULE_NATIVE_GLFW_INPUT_GAINPUT_CANDIDATES
        gainput::gainput
        gainputstatic
        gainput
)

set(LINKAGE_PRIVATE_LIB_MODULE_NATIVE_GLFW_INPUT_LINUX
        X11::X11
)

# OpenGL module
################
set(LINKAGE_PUBLIC_LIB_MODULE_OPENGL
        Cheryl::Engine
        Cheryl::NativeGLFW
)

set(LINKAGE_PRIVATE_LIB_MODULE_OPENGL
        ${TARGET_LIB_MODULE_OPENGL_S_GL46}
        OpenGL::GL
)

set(LINKAGE_PUBLIC_LIB_MODULE_OPENGL_S_STARTUP
        Cheryl::Startup
        Cheryl::OpenGL
)

# TGUI module
##############
set(LINKAGE_PUBLIC_LIB_MODULE_UI_TGUI
        Cheryl::Engine
        TGUI::TGUI
)

# RmlUi module
###############
set(LINKAGE_PUBLIC_LIB_MODULE_UI_RMLUI
        Cheryl::Engine
        RmlUi::Core
)

# Audio module
###############
set(LINKAGE_PUBLIC_LIB_MODULE_AUDIO_MINIAUDIO
        Cheryl::Engine)

set(LINKAGE_PRIVATE_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK
        Threads::Threads)

# Demo composition
###################
set(LINKAGE_COMPOSITION_DEBUG_TERMINAL
        "$<TARGET_NAME_IF_EXISTS:Cheryl::DebugTerminal>")

set(LINKAGE_COMPOSITION_APP_DEMO_BASE
        Cheryl::Engine
        Cheryl::NativeGLFW
        Cheryl::OpenGL
        Cheryl::OpenGL::Startup
        ${LINKAGE_COMPOSITION_DEBUG_TERMINAL}
)

set(LINKAGE_COMPOSITION_APP_DEMO_AUDIO
        Cheryl::Audio::Miniaudio
)

set(LINKAGE_COMPOSITION_APP_DEMO_UI_TGUI
        Cheryl::UI::TGUI
)

set(LINKAGE_COMPOSITION_APP_DEMO_UI_RMLUI
        Cheryl::UI::RmlUi
)

# Test support
###############
set(LINKAGE_INTERFACE_LIB_TEST_SUPPORT
        Cheryl::Engine
        gtest)

set(LINKAGE_INTERFACE_LIB_TEST_MAIN
        ${TARGET_LIB_TEST_SUPPORT}
        ${LINKAGE_COMPOSITION_DEBUG_TERMINAL})

# Test infrastructure
######################
set(LINKAGE_PUBLIC_TEST_CASES
        ${TARGET_LIB_TEST_SUPPORT})

set(LINKAGE_PRIVATE_TEST_AGGREGATE
        ${TARGET_LIB_TEST_MAIN})

set(LINKAGE_PRIVATE_TEST_RUNNER
        ${TARGET_LIB_TEST_MAIN})

# Unit tests
#############
set(LINKAGE_PUBLIC_TEST_ENGINE
        Cheryl::Engine)

set(LINKAGE_PUBLIC_TEST_ENGINE_LOGGING
        Cheryl::Engine)

set(LINKAGE_PUBLIC_TEST_WORKER_TOPOLOGY_JSON
        Cheryl::WorkerTopology::Json)

set(LINKAGE_PUBLIC_TEST_WORKER_TOPOLOGY_COMPOSITION
        ${TARGET_LIB_ENGINE_S_WORKER_TOPOLOGY_EXAMPLE})

set(LINKAGE_PUBLIC_TEST_MODULE_NATIVE_GLFW
        Cheryl::NativeGLFW)

set(LINKAGE_PUBLIC_TEST_MODULE_OPENGL
        Cheryl::OpenGL)

set(LINKAGE_PUBLIC_TEST_MODULE_UI_TGUI
        Cheryl::UI::TGUI)

set(LINKAGE_PUBLIC_TEST_MODULE_UI_RMLUI
        Cheryl::UI::RmlUi)
set(LINKAGE_PUBLIC_TEST_MODULE_AUDIO_MINIAUDIO
        Cheryl::Audio::Miniaudio)

# Acceptance tests
###################
set(LINKAGE_PUBLIC_TEST_ACCEPTANCE_ENGINE
        Cheryl::Engine)

set(LINKAGE_PUBLIC_TEST_ACCEPTANCE_WORKER_TOPOLOGY_HWLOC
        Cheryl::WorkerTopology::Hwloc)

set(LINKAGE_PRIVATE_TEST_ACCEPTANCE_ENGINE_LOGGING
        Cheryl::Engine
        ${LINKAGE_COMPOSITION_DEBUG_TERMINAL})

set(LINKAGE_PRIVATE_TEST_ACCEPTANCE_ENGINE_SIGNAL
        Cheryl::Engine
        ${LINKAGE_COMPOSITION_DEBUG_TERMINAL})

set(LINKAGE_PUBLIC_TEST_ACCEPTANCE_OPENGL
        Cheryl::OpenGL)

set(LINKAGE_PUBLIC_TEST_ACCEPTANCE_OPENGL_LINUX_X11
        X11::X11)

# Cross-module tests
#####################
set(LINKAGE_PUBLIC_TEST_UI_COEXIST
        Cheryl::UI::TGUI
        Cheryl::UI::RmlUi)

# Dependency checks
####################
set(LINKAGE_PUBLIC_TEST_DEPENDENCY_BACKWARD_CPP
        Backward::Backward)

# Consumer tests
#################
set(LINKAGE_PRIVATE_TEST_CONSUMER_ENGINE
        Cheryl::Engine)

set(LINKAGE_PRIVATE_TEST_CONSUMER_MODULE_NATIVE_GLFW
        Cheryl::NativeGLFW)

set(LINKAGE_PRIVATE_TEST_CONSUMER_MODULE_OPENGL
        Cheryl::OpenGL)

set(LINKAGE_PRIVATE_TEST_CONSUMER_MODULE_UI_TGUI
        Cheryl::UI::TGUI)

set(LINKAGE_PRIVATE_TEST_CONSUMER_MODULE_UI_RMLUI
        Cheryl::UI::RmlUi)
set(LINKAGE_PRIVATE_TEST_CONSUMER_MODULE_AUDIO_MINIAUDIO
        Cheryl::Audio::Miniaudio)
