cmake_minimum_required(VERSION 3.28)

include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylModules.cmake")
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cheryl_audio_miniaudio_module VERSION ${VERSION_PROJECT_MODULE_AUDIO_MINIAUDIO} LANGUAGES C CXX)
endif()
if(TARGET Cheryl::Audio::Miniaudio)
    return()
endif()
option(CHERYL_BUILD_TESTS "Build module-local checks" OFF)
cheryl_prepare_module()

set(CHERYL_MINIAUDIO_SOURCE "" CACHE PATH "Miniaudio 0.11.25 source directory, overriding the pinned submodule")
if(TARGET miniaudio::miniaudio)
    set(cheryl_miniaudio_target miniaudio::miniaudio)
elseif(TARGET miniaudio)
    set(cheryl_miniaudio_target miniaudio)
else()
    set(cheryl_miniaudio_source "${CHERYL_MINIAUDIO_SOURCE}")
    if(NOT cheryl_miniaudio_source)
        set(cheryl_miniaudio_source "${CHERYL_REPOSITORY_ROOT}/extern/miniaudio")
    endif()
    if(NOT EXISTS "${cheryl_miniaudio_source}/miniaudio.c" OR NOT EXISTS "${cheryl_miniaudio_source}/miniaudio.h")
        message(FATAL_ERROR "Initialize extern/miniaudio, supply CHERYL_MINIAUDIO_SOURCE, or provide a miniaudio target")
    endif()
    # Own only the SDK implementation needed here, without upstream examples,
    # extra nodes, optional codec integrations, install rules or cache rewrites.
    add_library(${TARGET_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK} STATIC "${cheryl_miniaudio_source}/miniaudio.c")
    set_target_properties(${TARGET_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK} PROPERTIES
        OUTPUT_NAME "${OUTPUT_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK}"
        ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
    target_include_directories(${TARGET_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK} SYSTEM PUBLIC "${cheryl_miniaudio_source}")
    target_link_libraries(${TARGET_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK} PRIVATE
        ${LINKAGE_PRIVATE_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK} ${CMAKE_DL_LIBS})
    if(UNIX)
        target_link_libraries(${TARGET_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK} PRIVATE m)
    endif()
    set(cheryl_miniaudio_target ${TARGET_LIB_MODULE_AUDIO_MINIAUDIO_S_SDK})
endif()

file(GLOB_RECURSE cheryl_audio_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${TARGET_LIB_MODULE_AUDIO_MINIAUDIO} STATIC ${cheryl_audio_sources})
add_library(Cheryl::Audio::Miniaudio ALIAS ${TARGET_LIB_MODULE_AUDIO_MINIAUDIO})
set_target_properties(${TARGET_LIB_MODULE_AUDIO_MINIAUDIO} PROPERTIES
    VERSION "${VERSION_PROJECT_MODULE_AUDIO_MINIAUDIO}"
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_AUDIO_MINIAUDIO}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
target_include_directories(${TARGET_LIB_MODULE_AUDIO_MINIAUDIO} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cheryl>")
target_link_libraries(${TARGET_LIB_MODULE_AUDIO_MINIAUDIO}
    PUBLIC ${LINKAGE_PUBLIC_LIB_MODULE_AUDIO_MINIAUDIO}
    PRIVATE ${cheryl_miniaudio_target})

if(CHERYL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(CHERYL_BUILD_CONSUMER_TESTS)
    add_subdirectory(tests/consumer)
endif()
