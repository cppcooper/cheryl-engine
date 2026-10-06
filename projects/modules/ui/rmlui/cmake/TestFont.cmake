function(cheryl_rmlui_test_font output)
    if(NOT CHERYL_RMLUI_TEST_FONT)
        get_target_property(core_source RmlUi::Core SOURCE_DIR)
        foreach(candidate
            "${CHERYL_RMLUI_SOURCE}/Samples/assets/LatoLatin-Regular.ttf"
            "${core_source}/../../Samples/assets/LatoLatin-Regular.ttf"
            "${CHERYL_REPOSITORY_ROOT}/extern/rmlui/Samples/assets/LatoLatin-Regular.ttf")
            if(EXISTS "${candidate}")
                set(CHERYL_RMLUI_TEST_FONT "${candidate}")
                break()
            endif()
        endforeach()
    endif()
    if(NOT EXISTS "${CHERYL_RMLUI_TEST_FONT}")
        message(FATAL_ERROR "RmlUi checks require a real font fixture; set CHERYL_RMLUI_TEST_FONT to a readable TTF/OTF file")
    endif()
    set(${output} "${CHERYL_RMLUI_TEST_FONT}" PARENT_SCOPE)
endfunction()
