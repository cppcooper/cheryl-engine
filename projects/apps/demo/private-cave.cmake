include_guard(GLOBAL)
# Private application producer, reused by demo/unit/world callers. Engine owns
# storage/tilemap; this archive contains ONLY the original cave utility sources.
set(world_pg "${CHERYL_REPOSITORY_ROOT}/projects/tests/procedural-generation")
add_library(world-cave-private STATIC "${world_pg}/maze.cpp" "${world_pg}/random.cpp" "${world_pg}/cave.cpp")
target_compile_features(world-cave-private PRIVATE cxx_std_23)
target_include_directories(world-cave-private PUBLIC "${world_pg}")
set_target_properties(world-cave-private PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(world-cave-private PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()
