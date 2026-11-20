add_library(${TARGET_LIB_MODULE_STORY} STATIC
    src/model/model.cpp src/model/validation.cpp)
add_library(Cheryl::Story ALIAS ${TARGET_LIB_MODULE_STORY})
set_target_properties(${TARGET_LIB_MODULE_STORY} PROPERTIES OUTPUT_NAME "${OUTPUT_LIB_MODULE_STORY}")
target_include_directories(${TARGET_LIB_MODULE_STORY} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(${TARGET_LIB_MODULE_STORY} PUBLIC cxx_std_23)
