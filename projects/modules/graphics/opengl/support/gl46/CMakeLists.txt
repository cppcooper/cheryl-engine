# Use the pinned GLAD specification instead of downloading a newer one during compilation.
glad_add_library(${TARGET_LIB_MODULE_OPENGL_GL46} STATIC EXCLUDE_FROM_ALL REPRODUCIBLE
    LOCATION "${CHERYL_BUILD_ROOT}/gladsources/gl46"
    API gl:core=4.6 EXTENSIONS GL_EXT_texture_filter_anisotropic GL_KHR_debug)
set_target_properties(${TARGET_LIB_MODULE_OPENGL_GL46} PROPERTIES
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_OPENGL_GL46}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
