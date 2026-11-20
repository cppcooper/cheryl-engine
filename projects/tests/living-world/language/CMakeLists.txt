cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(CherylLanguagePractice LANGUAGES CXX)
    include(CTest)
endif()

add_library(cheryl_living_world_language STATIC fragment.cpp projection.cpp drafts.cpp practice.cpp)
target_compile_features(cheryl_living_world_language PUBLIC cxx_std_23)
target_include_directories(cheryl_living_world_language PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(cheryl_language_practice main.cpp)
target_link_libraries(cheryl_language_practice PRIVATE cheryl_living_world_language)

option(CHERYL_LANGUAGE_BUILD_TESTS "Define focused language source tests" ON)
if(CHERYL_LANGUAGE_BUILD_TESTS)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../tests/language" "${CMAKE_CURRENT_BINARY_DIR}/language-tests")
endif()
