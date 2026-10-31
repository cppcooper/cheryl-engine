# Source-tree-only CPU service closure. Never link alongside Cheryl::Engine:
# both targets own the same Engine logging/exception/diagnostic implementation.
if(TARGET Cheryl::Engine OR TARGET Cheryl::MemoryCPU)
    message(FATAL_ERROR "MemoryCPU requires one exclusive Engine service implementation")
endif()
get_filename_component(cheryl_memory_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET ctti)
    add_subdirectory("${cheryl_memory_root}/extern/ctti" extern/ctti EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET Backward::Interface)
    set(BACKWARD_SHARED OFF)
    add_subdirectory("${cheryl_memory_root}/extern/backward-cpp" extern/backward-cpp EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET spdlog::spdlog)
    block(SCOPE_FOR VARIABLES)
        set(CMAKE_CXX_STANDARD 23)
        set(SPDLOG_USE_STD_FORMAT ON)
        set(BUILD_SHARED_LIBS OFF)
        set(SPDLOG_BUILD_SHARED OFF)
        add_subdirectory("${cheryl_memory_root}/extern/spdlog" extern/spdlog EXCLUDE_FROM_ALL)
    endblock()
endif()
find_package(Threads REQUIRED)
set(cheryl_memory_engine "${cheryl_memory_root}/projects/engine")
add_library(cheryl-memory-cpu STATIC
    "${cheryl_memory_engine}/src/internal/diagnostics.cpp"
    "${cheryl_memory_engine}/src/internal/exceptions.cpp"
    "${cheryl_memory_engine}/src/core/logging/backend-guard.cpp"
    "${cheryl_memory_engine}/src/core/logging/log.cpp"
    "${cheryl_memory_engine}/src/core/logging/process-lifetime.cpp")
add_library(Cheryl::MemoryCPU ALIAS cheryl-memory-cpu)
target_compile_features(cheryl-memory-cpu PUBLIC cxx_std_23)
set_target_properties(cheryl-memory-cpu PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(cheryl-memory-cpu PUBLIC CE_ENGINE_STATIC)
target_include_directories(cheryl-memory-cpu PUBLIC
    "${cheryl_memory_engine}/include" "${cheryl_memory_engine}/include/cheryl")
target_link_libraries(cheryl-memory-cpu PUBLIC ctti Threads::Threads PRIVATE spdlog::spdlog Backward::Interface)
# compile-policy.h selects the same existing profile from NDEBUG in every TU.
