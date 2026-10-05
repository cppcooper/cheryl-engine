# Each translation unit includes its header first, with no umbrella/prefix header.
set(consumer_headers
    cheryl/core.h
    cheryl/core/controls.h
    cheryl/core/display.h
    cheryl/core/engine/engine-context.h
    cheryl/core/engine/worker-pool.h
    cheryl/core/controls/input-interface.h
    cheryl/core/display/display-system-interface.h
    cheryl/core/rendering/render-frame.h
    cheryl/core/rendering/clip-region.h
    cheryl/core/game-framework/game-runtime.h
    cheryl/core/logging/log-names.h
    cheryl/core/logging/logger.h
    cheryl/core/resources/asset-management/asset-loader.h
    cheryl/assets/resources/resource-provider.h)
cheryl_add_header_probes(cheryl-consumer-headers Cheryl::Engine NEUTRAL HEADERS ${consumer_headers})
