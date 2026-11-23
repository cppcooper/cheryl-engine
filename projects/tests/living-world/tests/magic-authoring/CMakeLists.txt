if(NOT TARGET Cheryl::LivingWorld::MagicAuthoring)
    message(FATAL_ERROR "Add the authoring leaf before its consumers")
endif()
cheryl_add_test_suite(tests-living-magic-authoring
    SOURCES tests.cpp
    LIBRARIES Cheryl::LivingWorld::MagicAuthoring
    DEFINITIONS LIVING_WORLD_S1="${magic_source}/fixtures/S1.verse")
add_executable(living-magic-authoring-consumer consumer.cpp)
add_executable(tests-living-magic-authoring-allocation allocation.cpp)
foreach(authoring_consumer IN ITEMS living-magic-authoring-consumer tests-living-magic-authoring-allocation)
    target_compile_features(${authoring_consumer} PRIVATE cxx_std_23)
    target_compile_definitions(${authoring_consumer} PRIVATE LIVING_WORLD_S1="${magic_source}/fixtures/S1.verse")
    target_link_libraries(${authoring_consumer} PRIVATE Cheryl::LivingWorld::MagicAuthoring)
endforeach()
add_test(NAME living_magic_authoring COMMAND tests-living-magic-authoring "--gtest_filter=MagicAuthoring.*")
add_test(NAME living_magic_authoring_consumer COMMAND living-magic-authoring-consumer)
add_test(NAME living_magic_authoring_allocation COMMAND tests-living-magic-authoring-allocation)
set_tests_properties(living_magic_authoring living_magic_authoring_consumer living_magic_authoring_allocation
    PROPERTIES TIMEOUT 30 LABELS "living-world;magic;authoring")
