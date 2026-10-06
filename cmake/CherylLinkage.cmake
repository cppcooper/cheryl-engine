# Reload declarations into the caller's scope, including standalone entry points.

include("${CMAKE_CURRENT_LIST_DIR}/CherylTargets.cmake")


# Engine
#########
set(LINKAGE_PUBLIC_LIB_ENGINE
        Threads::Threads
        ${TARGET_LIB_ENGINE_LOGGING_CONFIG}
        ctti
        spdlog::spdlog
        glm::glm-header-only
        Backward::Interface
)

set(LINKAGE_INTERFACE_LIB_ENGINE
        "$<TARGET_OBJECTS:${TARGET_LIB_ENGINE_SIGNAL_HANDLERS}>"
)

set(LINKAGE_PUBLIC_LIB_ENGINE_SIGNAL_HANDLERS
        Backward::Interface
)

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
        ${TARGET_LIB_MODULE_OPENGL_GL46}
)

set(LINKAGE_PRIVATE_LIB_MODULE_OPENGL
        glfw
        OpenGL::GL
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

# Demo composition
###################
set(LINKAGE_COMPOSITION_APP_DEMO_BASE
        Cheryl::Engine
        Cheryl::NativeGLFW
        Cheryl::OpenGL
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
        ${TARGET_LIB_TEST_SUPPORT})

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

set(LINKAGE_PUBLIC_TEST_MODULE_NATIVE_GLFW
        Cheryl::NativeGLFW
        glfw)

set(LINKAGE_PUBLIC_TEST_MODULE_OPENGL
        Cheryl::OpenGL)

set(LINKAGE_PUBLIC_TEST_MODULE_UI_TGUI
        Cheryl::UI::TGUI)

set(LINKAGE_PUBLIC_TEST_MODULE_UI_RMLUI
        Cheryl::UI::RmlUi)

# Acceptance tests
###################
set(LINKAGE_PUBLIC_TEST_ACCEPTANCE_ENGINE
        Cheryl::Engine)

set(LINKAGE_PRIVATE_TEST_ACCEPTANCE_ENGINE_LOGGING
        Cheryl::Engine)

set(LINKAGE_PRIVATE_TEST_ACCEPTANCE_ENGINE_SIGNAL
        Cheryl::Engine)

set(LINKAGE_PUBLIC_TEST_ACCEPTANCE_OPENGL
        Cheryl::OpenGL
        glfw)

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
