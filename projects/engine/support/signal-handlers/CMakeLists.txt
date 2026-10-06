# Forward this object through the engine target so static consumers reliably get
# the original Debug crash bootstrap. Exception traces use Backward::Interface.
add_library(${TARGET_LIB_ENGINE_SIGNAL_HANDLERS} OBJECT src/main.cpp)
add_library(Cheryl::SignalHandlers ALIAS ${TARGET_LIB_ENGINE_SIGNAL_HANDLERS})
target_compile_features(${TARGET_LIB_ENGINE_SIGNAL_HANDLERS} PUBLIC cxx_std_23)
target_link_libraries(${TARGET_LIB_ENGINE_SIGNAL_HANDLERS} PUBLIC ${LINKAGE_PUBLIC_LIB_ENGINE_SIGNAL_HANDLERS})
