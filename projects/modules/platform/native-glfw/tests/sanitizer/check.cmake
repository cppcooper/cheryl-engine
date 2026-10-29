function(check_probe executable mode expected_exit)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "ASAN_OPTIONS=detect_leaks=1"
            "LSAN_OPTIONS=exitcode=23:use_stacks=0:use_registers=0"
            "${executable}" "${mode}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE diagnostic
        TIMEOUT 10)
    if(result STREQUAL "77")
        message("LSAN_PROBE_UNAVAILABLE: Fontconfig is not installed")
        set(probe_unavailable TRUE PARENT_SCOPE)
        return()
    endif()
    if(NOT result STREQUAL "${expected_exit}" OR NOT diagnostic MATCHES "external allocation created")
        message(FATAL_ERROR "${executable} ${mode}: expected ${expected_exit}, got ${result}\n${output}${diagnostic}")
    endif()
    if(expected_exit EQUAL 0 AND diagnostic MATCHES "detected memory leaks")
        message(FATAL_ERROR "External allocation was not filtered:\n${diagnostic}")
    elseif(mode STREQUAL "application")
        if(NOT diagnostic MATCHES "113 byte\\(s\\) leaked in 1 allocation\\(s\\)")
            message(FATAL_ERROR "Expected only the application leak:\n${diagnostic}")
        endif()
    elseif(expected_exit EQUAL 23 AND NOT diagnostic MATCHES "detected memory leaks")
        message(FATAL_ERROR "Application override did not restore dependency reports:\n${diagnostic}")
    endif()
    message(STATUS "${mode}: expected exit ${expected_exit} confirmed")
endfunction()

check_probe("${DEFAULT_PROBE}" external 0)
if(probe_unavailable)
    return()
endif()
check_probe("${DEFAULT_PROBE}" application 23)
check_probe("${OVERRIDE_PROBE}" external 23)
