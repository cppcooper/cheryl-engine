# Each translation unit includes its header first, with no umbrella/prefix header.
set(consumer_headers
    cheryl/engine.h
    cheryl/cheryl.h
    cheryl/modules.h
    cheryl/support.h
    cheryl/core.h
    cheryl/core/controls.h
    cheryl/core/display.h
    cheryl/core/engine/engine-context.h
    cheryl/core/engine/worker-pool.h
    cheryl/core/engine/worker-topology.h
    cheryl/core/engine/worker-configuration.h
    cheryl/core/subsystems/event-channel.h
    cheryl/core/subsystems/event-bus.h
    cheryl/core/subsystems/event-system.h
    cheryl/core/controls/input-interface.h
    cheryl/core/controls/gamepad-availability.h
    cheryl/core/controls/action-context.h
    cheryl/core/controls/button-codes.h
    cheryl/core/controls/input-record.h
    cheryl/core/display/display-system-interface.h
    cheryl/core/rendering/render-frame.h
    cheryl/core/rendering/graphics-domain.h
    cheryl/core/rendering/clip-region.h
    cheryl/core/game-framework/game-runtime.h
    cheryl/core/logging/log-names.h
    cheryl/core/logging/logger.h
    cheryl/core/resources/asset-management/asset-loader.h
    cheryl/assets/definitions/animation.h
    cheryl/assets/selection/tile-selection.h
    cheryl/assets/types/2d/tileset.h
    cheryl/assets/types/2d/stbfont.h
    cheryl/text/utf8.h
    cheryl/text/font-collection.h
    cheryl/text/font.h
    cheryl/text/font-service.h
    cheryl/text/font-rasterizer.h
    cheryl/text/layout.h
    cheryl/text/text-updater.h
    cheryl/audio.h
    cheryl/audio/clip.h
    cheryl/audio/system.h
    cheryl/assets/types/2d/unicode-text.h
    cheryl/assets/resources/resource-provider.h)
cheryl_add_header_probes(${TARGET_TEST_CONSUMER_ENGINE_HEADERS} ${LINKAGE_PRIVATE_TEST_CONSUMER_ENGINE} NEUTRAL HEADERS ${consumer_headers})
target_sources(${TARGET_TEST_CONSUMER_ENGINE_HEADERS} PRIVATE aggregate.cpp)

if(TARGET Cheryl::Cheryl)
    add_library(${TARGET_TEST_CONSUMER_CHERYL_HEADERS} OBJECT aggregate.cpp)
    target_link_libraries(${TARGET_TEST_CONSUMER_CHERYL_HEADERS} PRIVATE Cheryl::Cheryl)
    set(aggregate_owners
        "Cheryl::NativeGLFW|NATIVE_GLFW"
        "Cheryl::DebugTerminal::Linux|DEBUG_TERMINAL_LINUX"
        "Cheryl::OpenGL|OPENGL"
        "Cheryl::UI::TGUI|UI_TGUI"
        "Cheryl::UI::RmlUi|UI_RMLUI"
        "Cheryl::Audio::Miniaudio|AUDIO_MINIAUDIO"
        "Cheryl::Startup|SUPPORT_STARTUP"
        "Cheryl::DebugTerminal::Startup|SUPPORT_DEBUG_TERMINAL"
        "Cheryl::WorkerTopology::Hwloc|SUPPORT_WORKER_TOPOLOGY_HWLOC"
        "Cheryl::WorkerTopology::Json|SUPPORT_WORKER_TOPOLOGY_JSON"
        "Cheryl::OpenGL::Startup|SUPPORT_OPENGL_STARTUP")
    foreach(owner IN LISTS aggregate_owners)
        string(REPLACE "|" ";" contract "${owner}")
        list(GET contract 0 owner_target)
        list(GET contract 1 feature)
        if(TARGET ${owner_target})
            target_compile_definitions(${TARGET_TEST_CONSUMER_CHERYL_HEADERS} PRIVATE "CHERYL_CONSUMER_${feature}=1")
        endif()
    endforeach()
    if(TARGET Cheryl::DebugTerminal::Linux)
        cheryl_add_header_probes(${TARGET_TEST_CONSUMER_CHERYL_HEADERS}-debug-terminal Cheryl::DebugTerminal::Linux
            HEADERS cheryl/modules/debug-terminal-linux.h)
        add_dependencies(${TARGET_TEST_CONSUMER_CHERYL_HEADERS} ${TARGET_TEST_CONSUMER_CHERYL_HEADERS}-debug-terminal)
    endif()
endif()
