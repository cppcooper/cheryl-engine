# Forward this object through the engine target so static consumers reliably get
# the original Debug crash bootstrap. Exception traces use Backward::Interface.
add_library(cheryl_signal_handlers OBJECT src/main.cpp)
add_library(Cheryl::SignalHandlers ALIAS cheryl_signal_handlers)
target_compile_features(cheryl_signal_handlers PUBLIC cxx_std_23)
target_link_libraries(cheryl_signal_handlers PUBLIC Backward::Interface)
