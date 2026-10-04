# Each translation unit includes its header first, with no umbrella/prefix header.
set(consumer_headers
    cheryl/core/engine/engine-context.h
    cheryl/core/engine/worker-pool.h
    cheryl/core/controls/input-interface.h
    cheryl/core/display/display-system-interface.h
    cheryl/core/rendering/render-frame.h
    cheryl/core/game-framework/game-runtime.h
    cheryl/core/logging/log-names.h
    cheryl/core/logging/logger.h
    cheryl/core/resources/asset-management/asset-loader.h
    cheryl/assets/resources/resource-provider.h
    cheryl/backends/opengl/glslprogram.h)
if(NOT CHERYL_SANDBOX_BUILD)
    list(APPEND consumer_headers cheryl/core/controls/input-system.h)
endif()
set(header_sources)
foreach(header IN LISTS consumer_headers)
    string(MAKE_C_IDENTIFIER "${header}" identifier)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/${identifier}.cpp")
    file(WRITE "${source}" "#include <${header}>\n")
    list(APPEND header_sources "${source}")
endforeach()
add_library(cheryl-consumer-headers OBJECT ${header_sources})
target_link_libraries(cheryl-consumer-headers PRIVATE Cheryl::Engine)
