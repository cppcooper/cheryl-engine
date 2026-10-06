include_guard(GLOBAL)

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
        ${TARGET_LIB_ENGINE}
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
        ${TARGET_LIB_ENGINE}
        ${TARGET_LIB_MODULE_NATIVE_GLFW}
        gl46
)

set(LINKAGE_PRIVATE_LIB_MODULE_OPENGL
        glfw
        OpenGL::GL
)

# TGUI module
##############
set(LINKAGE_PUBLIC_LIB_MODULE_UI_TGUI
        ${TARGET_LIB_ENGINE}
        TGUI::TGUI
)

# RmlUi module
###############
set(LINKAGE_PUBLIC_LIB_MODULE_UI_RMLUI
        ${TARGET_LIB_ENGINE}
        RmlUi::Core
)

# Demo composition
###################
set(LINKAGE_COMPOSITION_APP_DEMO_BASE
        ${TARGET_LIB_ENGINE}
        ${TARGET_LIB_MODULE_NATIVE_GLFW}
        ${TARGET_LIB_MODULE_OPENGL}
)

set(LINKAGE_COMPOSITION_APP_DEMO_UI_TGUI
        ${TARGET_LIB_MODULE_UI_TGUI}
)

set(LINKAGE_COMPOSITION_APP_DEMO_UI_RMLUI
        ${TARGET_LIB_MODULE_UI_RMLUI}
)