# Each translation unit includes its header first, with no umbrella/prefix header.
set(consumer_headers
    cheryl/core.h
    cheryl/core/controls.h
    cheryl/core/display.h
    cheryl/core/engine/engine-context.h
    cheryl/core/engine/worker-pool.h
    cheryl/core/subsystems/event-channel.h
    cheryl/core/subsystems/event-bus.h
    cheryl/core/subsystems/event-system.h
    cheryl/core/controls/input-interface.h
    cheryl/core/controls/button-codes.h
    cheryl/core/controls/input-record.h
    cheryl/core/display/display-system-interface.h
    cheryl/core/rendering/render-frame.h
    cheryl/core/rendering/clip-region.h
    cheryl/core/game-framework/game-runtime.h
    cheryl/core/logging/log-names.h
    cheryl/core/logging/logger.h
    cheryl/core/resources/asset-management/asset-loader.h
    cheryl/assets/definitions/animation.h
    cheryl/assets/resources/resource-provider.h)
cheryl_add_header_probes(${TARGET_TEST_CONSUMER_ENGINE_HEADERS} ${LINKAGE_PRIVATE_TEST_CONSUMER_ENGINE} NEUTRAL HEADERS ${consumer_headers})
