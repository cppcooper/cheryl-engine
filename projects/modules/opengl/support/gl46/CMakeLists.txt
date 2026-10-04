# Use the pinned GLAD specification instead of downloading a newer one during compilation.
glad_add_library(gl46 STATIC EXCLUDE_FROM_ALL REPRODUCIBLE
    LOCATION "${CHERYL_BUILD_ROOT}/gladsources/gl46"
    API gl:core=4.6 EXTENSIONS GL_EXT_texture_filter_anisotropic GL_KHR_debug)
set_target_properties(gl46 PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}")
