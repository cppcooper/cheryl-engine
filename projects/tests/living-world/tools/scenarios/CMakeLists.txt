add_library(cheryl-living-scenarios STATIC runner.cpp)
add_library(Cheryl::LivingWorld::Scenarios ALIAS cheryl-living-scenarios)
target_compile_features(cheryl-living-scenarios PUBLIC cxx_std_23)
target_compile_definitions(cheryl-living-scenarios PRIVATE LIVING_SCENARIO_S1="${magic_source}/fixtures/S1.verse")
target_link_libraries(cheryl-living-scenarios PUBLIC Cheryl::LivingWorld::Console Cheryl::Console)

add_executable(living-scenarios main.cpp)
target_link_libraries(living-scenarios PRIVATE Cheryl::LivingWorld::Scenarios)
