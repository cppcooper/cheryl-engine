# Use the pinned GLAD specification instead of downloading a newer one during compilation.
glad_add_library(${TARGET_LIB_MODULE_OPENGL_S_GL46} ${CHERYL_LIBRARY_TYPE} EXCLUDE_FROM_ALL REPRODUCIBLE
    LOCATION "${CHERYL_BUILD_ROOT}/gladsources/gl46"
    API gl:core=4.6 EXTENSIONS GL_ARB_texture_filter_anisotropic GL_EXT_texture_filter_anisotropic GL_KHR_debug)
set_target_properties(${TARGET_LIB_MODULE_OPENGL_S_GL46} PROPERTIES
    OUTPUT_NAME "${OUTPUT_LIB_MODULE_OPENGL_S_GL46}"
    ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
cheryl_shared_dependency(${TARGET_LIB_MODULE_OPENGL_S_GL46} OWNED)
