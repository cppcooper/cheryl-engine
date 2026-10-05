cheryl_add_test_suite(engine-tests
    SOURCES
        src/clipping.cpp
        src/runtime-contract.cpp
        ../all-tests/src/assets/draw-contract.cpp
        ../all-tests/src/assets/manifest.cpp
        ../all-tests/src/assets/pipeline.cpp
        ../all-tests/src/assets/render-frame.cpp
        ../all-tests/src/assets/submission.cpp
        ../all-tests/src/assets/vertex.cpp
        ../all-tests/src/core/camera.cpp
        ../all-tests/src/core/display-contract.cpp
        ../all-tests/src/core/input-capture.cpp
        ../all-tests/src/core/input-contract.cpp
        ../all-tests/src/core/input-routing.cpp
        ../all-tests/src/core/input.cpp
        ../all-tests/src/core/polling-backlog.cpp
        ../all-tests/src/core/simulation-scheduler.cpp
        ../all-tests/src/core/tick-context.cpp
        ../all-tests/src/math/math.cpp
        ../all-tests/src/math/numbers-link.cpp
        ../all-tests/src/math/numbers.cpp
        ../all-tests/src/templates/ovar.cpp
        ../all-tests/src/templates/state.cpp
    LIBRARIES Cheryl::Engine
    PRIVATE_INCLUDES "${CHERYL_ENGINE_PRIVATE_SOURCE_DIR}")
