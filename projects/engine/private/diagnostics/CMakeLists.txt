# Static consumers receive this object; a shared Engine embeds it once.
# Exception traces use Backward::Interface.
file(GLOB_RECURSE cheryl_signal_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS} OBJECT ${cheryl_signal_sources})
add_library(Cheryl::SignalHandlers ALIAS ${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS})
if(CHERYL_BUILD_SHARED_LIBS)
    set_target_properties(${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS} PROPERTIES
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
    target_compile_definitions(${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS} PRIVATE CE_BUILDING_ENGINE)
else()
    target_compile_definitions(${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS} PRIVATE CE_ENGINE_STATIC)
endif()
target_include_directories(${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS} PRIVATE
    "${CHERYL_REPOSITORY_ROOT}/projects/engine/include")
target_compile_features(${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS} PUBLIC cxx_std_23)
target_link_libraries(${TARGET_LIB_ENGINE_S_SIGNAL_HANDLERS} PUBLIC ${LINKAGE_PUBLIC_LIB_ENGINE_S_SIGNAL_HANDLERS})
