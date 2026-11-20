set(magic_source "${CHERYL_REPOSITORY_ROOT}/projects/magic")
add_library(cheryl-living-magic-compiler STATIC "${magic_source}/src/compiler.cpp")
add_library(Cheryl::LivingWorld::MagicCompiler ALIAS cheryl-living-magic-compiler)
target_include_directories(cheryl-living-magic-compiler PUBLIC "${magic_source}/include")
target_compile_features(cheryl-living-magic-compiler PUBLIC cxx_std_23)

add_library(cheryl-living-magic-contact STATIC "${magic_source}/src/runtime.cpp")
add_library(Cheryl::LivingWorld::MagicContact ALIAS cheryl-living-magic-contact)
target_link_libraries(cheryl-living-magic-contact PUBLIC Cheryl::LivingWorld::MagicCompiler)
target_compile_features(cheryl-living-magic-contact PUBLIC cxx_std_23)

add_library(cheryl-living-world STATIC
    host/host.cpp acquisition/delivery.cpp magic/contact/view.cpp story/scene/scene.cpp
    recovery/paired.cpp)
add_library(Cheryl::LivingWorld::Host ALIAS cheryl-living-world)
target_include_directories(cheryl-living-world PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(cheryl-living-world PUBLIC cxx_std_23)
target_link_libraries(cheryl-living-world PUBLIC Cheryl::Engine Cheryl::LivingWorld::MagicContact)

add_executable(cheryl-living-world-consumer consumer/main.cpp)
target_compile_features(cheryl-living-world-consumer PRIVATE cxx_std_23)
target_compile_definitions(cheryl-living-world-consumer PRIVATE LIVING_WORLD_S1="${magic_source}/fixtures/S1.verse")
target_link_libraries(cheryl-living-world-consumer PRIVATE Cheryl::LivingWorld::Host)

cheryl_add_test_aggregate(tests-living-world OWNER)
set(CHERYL_TEST_AGGREGATE tests-living-world)
cheryl_add_test_suite(tests-living-world-joins
    SOURCES tests/host/host.cpp tests/acquisition/observation.cpp
        tests/magic-contact/contact.cpp tests/story-scene/scene.cpp tests/integration/join.cpp
        tests/integration/engine-map.cpp
    LIBRARIES Cheryl::LivingWorld::Host
    DEFINITIONS LIVING_WORLD_S1="${magic_source}/fixtures/S1.verse")

add_executable(tests-living-world-allocation tests/integration/allocation.cpp)
target_compile_features(tests-living-world-allocation PRIVATE cxx_std_23)
target_compile_definitions(tests-living-world-allocation PRIVATE LIVING_WORLD_S1="${magic_source}/fixtures/S1.verse")
target_link_libraries(tests-living-world-allocation PRIVATE Cheryl::LivingWorld::Host)
add_test(NAME living_world_allocation COMMAND tests-living-world-allocation)

add_subdirectory(world/sites)
add_subdirectory(tests/world-sites)
add_subdirectory(language)

add_test(NAME living_world_consumer COMMAND cheryl-living-world-consumer)
add_test(NAME living_world_joins COMMAND tests-living-world-joins
    "--gtest_filter=HostFixture.*:living_join.*:living_contact.*:living_scene.*:living_observation.*:living_integration.*")
set_tests_properties(living_world_allocation living_world_consumer living_world_joins
    PROPERTIES TIMEOUT 30 LABELS "living-world;joins")

add_dependencies(tests-living-world tests-living-world-joins tests-living-world-allocation
    cheryl-living-world-consumer tests-world-sites living-world-sites-consumer
    cheryl_language_practice)
if(TARGET cheryl_language_tests)
    add_dependencies(tests-living-world cheryl_language_tests)
endif()
