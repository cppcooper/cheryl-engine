cmake_minimum_required(VERSION 3.28)
project(cheryl_map_storage_consumer LANGUAGES CXX)

# Keep unit/process tooling dependencies off in either producer mode.
# The executable and probes consume only the selected production link interface.
option(MAP_STORAGE_BUILD_TESTS "Build the pinned gtest and Python process checks" OFF)
if(MAP_STORAGE_BUILD_TESTS)
    message(FATAL_ERROR "This independent CPU consumer requires MAP_STORAGE_BUILD_TESTS=OFF")
endif()
get_filename_component(map_storage_repository "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
option(MAP_STORAGE_USE_SHARED_ENGINE "Verify the normal shared Engine instead of the standalone CPU producer" OFF)
if(MAP_STORAGE_USE_SHARED_ENGINE)
    if(NOT CHERYL_BUILD_SHARED_LIBS)
        message(FATAL_ERROR "Shared Engine verification requires CHERYL_BUILD_SHARED_LIBS=ON")
    endif()
    if(NOT TARGET Cheryl::Engine)
        include("${map_storage_repository}/cmake/CherylModules.cmake")
        block(SCOPE_FOR VARIABLES)
            # Standalone use bootstraps the normal Engine. When registered by
            # Engine's test owner, reuse that exact producer and its selections.
            set(CHERYL_BUILD_TESTS OFF)
            cheryl_prepare_module()
        endblock()
    endif()
    get_target_property(map_storage_engine_kind Cheryl::Engine TYPE)
    get_target_property(map_storage_engine_source Cheryl::Engine SOURCE_DIR)
    get_target_property(map_storage_engine_imported Cheryl::Engine IMPORTED)
    if(NOT map_storage_engine_kind STREQUAL "SHARED_LIBRARY" OR map_storage_engine_imported OR
            NOT map_storage_engine_source STREQUAL "${map_storage_repository}/projects/engine")
        message(FATAL_ERROR "Shared verification requires this clone's normal shared Cheryl::Engine producer")
    endif()
    if(TARGET Cheryl::MapStorageCPU)
        message(FATAL_ERROR "Shared verification must not configure the alternate CPU producer")
    endif()
    set(map_storage_producer Cheryl::Engine)
    set(map_storage_visibility_check "#if defined(CE_ENGINE_STATIC) || defined(CE_BUILDING_ENGINE)\n#error A shared consumer must inherit Engine imports without static/producer patches\n#endif\n")
else()
    add_subdirectory("${map_storage_repository}/projects/tests/map-storage" boundary EXCLUDE_FROM_ALL)
    set(map_storage_producer Cheryl::MapStorageCPU)
    set(map_storage_visibility_check "#ifndef CE_ENGINE_STATIC\n#error The linked CPU target must declare static Engine visibility\n#endif\n#ifdef CE_BUILDING_ENGINE\n#error A consumer must not export Engine symbols\n#endif\n")
endif()

file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/map-storage-producer.txt" CONTENT
    "target=${map_storage_producer}\ntype=$<TARGET_PROPERTY:${map_storage_producer},TYPE>\nsource=$<TARGET_PROPERTY:${map_storage_producer},SOURCE_DIR>\nbinary=$<TARGET_FILE:${map_storage_producer}>\ninterface_definitions=$<TARGET_PROPERTY:${map_storage_producer},INTERFACE_COMPILE_DEFINITIONS>\ninterface_includes=$<TARGET_PROPERTY:${map_storage_producer},INTERFACE_INCLUDE_DIRECTORIES>\n")

add_executable(map_storage_consumer main.cpp)
target_link_libraries(map_storage_consumer PRIVATE ${map_storage_producer})
set_target_properties(map_storage_consumer PROPERTIES CXX_EXTENSIONS OFF)

# No prefix/umbrella header or consumer-supplied visibility definition. Each
# public header is first and repeated, then instantiated using its own contract.
set(map_storage_probe_sources)
foreach(map_storage_header IN ITEMS snapshot codec file-storage cell-patch)
    set(map_storage_probe "${CMAKE_CURRENT_BINARY_DIR}/${map_storage_header}-first.cpp")
    file(WRITE "${map_storage_probe}" "#include <cheryl/core/resources/map-storage/${map_storage_header}.h>\n#include <cheryl/core/resources/map-storage/${map_storage_header}.h>\n${map_storage_visibility_check}")
    if(map_storage_header STREQUAL "snapshot")
        file(APPEND "${map_storage_probe}" "static_assert(CE::Maps::Snapshot{} == CE::Maps::Snapshot{});\n")
    else()
        if(map_storage_header STREQUAL "codec")
            file(APPEND "${map_storage_probe}" "void codec_validate_first(const CE::Maps::Snapshot& value) { CE::Maps::validate(value); }\n")
        elseif(map_storage_header STREQUAL "cell-patch")
            file(APPEND "${map_storage_probe}" "#include <type_traits>\nstatic_assert(!std::is_constructible_v<CE::Maps::CellPatches::Witness, std::string>);\nstatic_assert(std::is_copy_constructible_v<CE::Maps::CellPatches::Witness>);\nstatic_assert(std::is_nothrow_move_constructible_v<CE::Maps::CellPatches::Witness>);\nvoid cell_patch_first(const CE::Maps::Snapshot& value) {\n    namespace P = CE::Maps::CellPatches;\n    auto witness = P::capture(value);\n    auto copied = witness;\n    auto result = P::apply(value, {copied, {}});\n    auto prepared = P::diff(value, value);\n    auto checked = P::apply(value, prepared);\n    static_cast<void>(checked);\n    static_cast<void>(result);\n}\n")
        endif()
        file(APPEND "${map_storage_probe}" "#include <string_view>\n#define PROBE_STRING_(value) #value\n#define PROBE_STRING(value) PROBE_STRING_(value)\n")
        if(MAP_STORAGE_USE_SHARED_ENGINE)
            file(APPEND "${map_storage_probe}" "#if defined(_WIN32) || defined(__GNUC__) || defined(__clang__)\nstatic_assert(!std::string_view(PROBE_STRING(CE_API)).empty());\n#endif\n")
        else()
            file(APPEND "${map_storage_probe}" "static_assert(std::string_view(PROBE_STRING(CE_API)).empty());\n")
        endif()
    endif()
    list(APPEND map_storage_probe_sources "${map_storage_probe}")
endforeach()
add_library(map_storage_header_probes OBJECT ${map_storage_probe_sources})
target_link_libraries(map_storage_header_probes PRIVATE ${map_storage_producer})
set_target_properties(map_storage_header_probes PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(map_storage_consumer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(map_storage_header_probes PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Only this small consumer's process checks remain enabled; no test dependency.
enable_testing()
foreach(map_storage_check IN ITEMS memory round_trip preservation validation patches diff diff_round_trip diff_preservation)
    add_test(NAME "map_storage.consumer.${map_storage_check}"
        COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:map_storage_consumer>"
            "-DMODE=${map_storage_check}" "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/consumer-checks"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check.cmake")
    set_tests_properties("map_storage.consumer.${map_storage_check}" PROPERTIES LABELS "map_storage")
endforeach()
