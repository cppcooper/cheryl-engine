cmake_minimum_required(VERSION 3.23)
project(CherylProcGenPrivate LANGUAGES CXX)

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "Configure this private prototype as a standalone project")
endif()

option(PROCGEN_UBSAN "Run focused integer/topology checks with undefined-behavior diagnostics" OFF)

add_library(procgen-private STATIC maze.cpp random.cpp traversal.cpp metrics.cpp configuration.cpp cave.cpp clearance.cpp tunnels.cpp dungeon.cpp elevation.cpp drainage.cpp spill.cpp channels.cpp climate.cpp moisture.cpp)
add_library(procgen-checks STATIC checks.cpp cave-checks.cpp clearance-checks.cpp tunnel-checks.cpp dungeon-checks.cpp elevation-checks.cpp drainage-checks.cpp spill-checks.cpp channel-checks.cpp climate-checks.cpp moisture-checks.cpp)
target_link_libraries(procgen-checks PRIVATE procgen-private)
add_executable(procgen-tests tests.cpp traversal-tests.cpp configuration-tests.cpp)
target_link_libraries(procgen-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-consumer consumer.cpp)
target_link_libraries(procgen-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-cave-tests cave-tests.cpp)
target_link_libraries(procgen-cave-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-cave-consumer cave-consumer.cpp)
target_link_libraries(procgen-cave-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-clearance-tests clearance-tests.cpp)
target_link_libraries(procgen-clearance-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-clearance-consumer clearance-consumer.cpp)
target_link_libraries(procgen-clearance-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-tunnel-tests tunnel-tests.cpp)
target_link_libraries(procgen-tunnel-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-tunnel-consumer tunnel-consumer.cpp)
target_link_libraries(procgen-tunnel-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-dungeon-tests dungeon-tests.cpp)
target_link_libraries(procgen-dungeon-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-dungeon-consumer dungeon-consumer.cpp)
target_link_libraries(procgen-dungeon-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-elevation-tests elevation-tests.cpp)
target_link_libraries(procgen-elevation-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-elevation-consumer elevation-consumer.cpp)
target_link_libraries(procgen-elevation-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-drainage-tests drainage-tests.cpp)
target_link_libraries(procgen-drainage-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-drainage-consumer drainage-consumer.cpp)
target_link_libraries(procgen-drainage-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-spill-tests spill-tests.cpp)
target_link_libraries(procgen-spill-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-spill-consumer spill-consumer.cpp)
target_link_libraries(procgen-spill-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-channel-tests channel-tests.cpp)
target_link_libraries(procgen-channel-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-channel-consumer channel-consumer.cpp)
target_link_libraries(procgen-channel-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-climate-tests climate-tests.cpp)
target_link_libraries(procgen-climate-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-climate-consumer climate-consumer.cpp)
target_link_libraries(procgen-climate-consumer PRIVATE procgen-private procgen-checks)

add_executable(procgen-moisture-tests moisture-tests.cpp)
target_link_libraries(procgen-moisture-tests PRIVATE procgen-private procgen-checks)
add_executable(procgen-moisture-consumer moisture-consumer.cpp)
target_link_libraries(procgen-moisture-consumer PRIVATE procgen-private procgen-checks)

foreach(target IN ITEMS procgen-private procgen-checks procgen-tests procgen-consumer procgen-cave-tests procgen-cave-consumer
                        procgen-clearance-tests procgen-clearance-consumer procgen-tunnel-tests procgen-tunnel-consumer procgen-dungeon-tests procgen-dungeon-consumer
                        procgen-elevation-tests procgen-elevation-consumer procgen-drainage-tests procgen-drainage-consumer
                        procgen-spill-tests procgen-spill-consumer procgen-channel-tests procgen-channel-consumer
                        procgen-climate-tests procgen-climate-consumer
                        procgen-moisture-tests procgen-moisture-consumer)
    target_compile_features(${target} PRIVATE cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
    endif()
    if(PROCGEN_UBSAN)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(FATAL_ERROR "PROCGEN_UBSAN requires GCC or Clang")
        endif()
        target_compile_options(${target} PRIVATE -fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=undefined -fno-sanitize-recover=all)
    endif()
endforeach()

enable_testing()
foreach(case IN ITEMS rng domain topology four_d replay ownership invalid overflow budgets exhaustion)
    add_test(NAME procgen.${case} COMMAND procgen-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
foreach(case IN ITEMS route_unit route_weighted route_cycles route_limits route_invalid route_overflow route_ownership
                      metrics_path metrics_branch metrics_cycle metrics_invalid metrics_limits)
    add_test(NAME procgen.${case} COMMAND procgen-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
foreach(case IN ITEMS config_profile config_four_d config_tiny config_rounding config_fixed config_invalid config_replay
                      config_ownership config_limits config_exhaustion)
    add_test(NAME procgen.${case} COMMAND procgen-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.consumer COMMAND procgen-consumer)
set_tests_properties(procgen.consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS cave_initial cave_smoothing cave_components cave_empty cave_replay cave_ownership cave_invalid cave_limits cave_exhaustion)
    add_test(NAME procgen.${case} COMMAND procgen-cave-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.cave_consumer COMMAND procgen-cave-consumer)
set_tests_properties(procgen.cave_consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS clearance_literal clearance_outside clearance_exhaustive clearance_regions clearance_generated
                      clearance_invalid clearance_limits clearance_ownership)
    add_test(NAME procgen.${case} COMMAND procgen-clearance-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.clearance_consumer COMMAND procgen-clearance-consumer)
set_tests_properties(procgen.clearance_consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS tunnel_literal tunnel_multiple tunnel_exhaustive tunnel_generated tunnel_empty tunnel_invalid tunnel_limits tunnel_ownership)
    add_test(NAME procgen.${case} COMMAND procgen-tunnel-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.tunnel_consumer COMMAND procgen-tunnel-consumer)
set_tests_properties(procgen.tunnel_consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS dungeon_literal dungeon_layout dungeon_partial dungeon_controls dungeon_replay dungeon_ownership
                      dungeon_invalid dungeon_limits dungeon_exhaustion)
    add_test(NAME procgen.${case} COMMAND procgen-dungeon-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.dungeon_consumer COMMAND procgen-dungeon-consumer)
set_tests_properties(procgen.dungeon_consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS elevation_literal elevation_fields elevation_sea elevation_flat elevation_replay elevation_invalid elevation_limits elevation_ownership)
    add_test(NAME procgen.${case} COMMAND procgen-elevation-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.elevation_consumer COMMAND procgen-elevation-consumer)
set_tests_properties(procgen.elevation_consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS drainage_literal drainage_terminals drainage_ties drainage_extremes drainage_exhaustive drainage_generated
                      drainage_invalid drainage_limits drainage_ownership)
    add_test(NAME procgen.${case} COMMAND procgen-drainage-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.drainage_consumer COMMAND procgen-drainage-consumer)
set_tests_properties(procgen.drainage_consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS spill_literal spill_saddles spill_plateau spill_extremes spill_exhaustive spill_generated spill_invalid spill_limits spill_ownership)
    add_test(NAME procgen.${case} COMMAND procgen-spill-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.spill_consumer COMMAND procgen-spill-consumer)
set_tests_properties(procgen.spill_consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS channel_literal channel_lake channel_thresholds channel_tiny channel_generated channel_malformed channel_invalid channel_limits channel_ownership)
    add_test(NAME procgen.${case} COMMAND procgen-channel-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.channel_consumer COMMAND procgen-channel-consumer)
set_tests_properties(procgen.channel_consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS climate_sun climate_seasons climate_calendar climate_frame climate_ridge climate_surface climate_generated
                      climate_invalid climate_projection climate_limits climate_ownership)
    add_test(NAME procgen.${case} COMMAND procgen-climate-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.climate_consumer COMMAND procgen-climate-consumer)
set_tests_properties(procgen.climate_consumer PROPERTIES TIMEOUT 30)

foreach(case IN ITEMS moisture_transport moisture_faces moisture_phases moisture_sources moisture_ridge moisture_generated
                      moisture_grouped moisture_boundaries moisture_capacity moisture_invalid moisture_limits moisture_ownership)
    add_test(NAME procgen.${case} COMMAND procgen-moisture-tests ${case})
    set_tests_properties(procgen.${case} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME procgen.moisture_consumer COMMAND procgen-moisture-consumer)
set_tests_properties(procgen.moisture_consumer PROPERTIES TIMEOUT 30)
