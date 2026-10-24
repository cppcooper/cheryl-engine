# Source-tree CPU proof target. The normal Engine discovers these same .cpp files
# through its existing source glob; this target does not configure the full Engine.
include_guard(GLOBAL)
get_filename_component(map_storage_engine "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(map_storage_root "${map_storage_engine}/../.." ABSOLUTE)
if(NOT EXISTS "${map_storage_root}/extern/json/single_include/nlohmann/json.hpp")
    message(FATAL_ERROR "Missing pinned extern/json/single_include/nlohmann/json.hpp")
endif()

add_library(map_storage_codec STATIC
    "${map_storage_engine}/src/core/resources/map-storage/codec.cpp"
    "${map_storage_engine}/src/core/resources/map-storage/cell-patch.cpp"
    "${map_storage_engine}/src/core/resources/map-storage/file-storage.cpp")
add_library(Cheryl::MapStorageCPU ALIAS map_storage_codec)
target_compile_features(map_storage_codec PUBLIC cxx_std_23)
target_compile_definitions(map_storage_codec PUBLIC CE_ENGINE_STATIC)
target_include_directories(map_storage_codec PUBLIC "${map_storage_engine}/include")
target_include_directories(map_storage_codec SYSTEM PRIVATE "${map_storage_root}/extern/json/single_include")
set_target_properties(map_storage_codec PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(map_storage_codec PRIVATE -Wall -Wextra -Wpedantic)
endif()
