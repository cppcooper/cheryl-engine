# Leak checks need separate processes so deliberate leaks never enter unit runners.
foreach(probe IN ITEMS native-lsan-default native-lsan-override)
    add_executable(${probe} probe.cpp)
    target_link_libraries(${probe} PRIVATE Cheryl::NativeGLFW ${CMAKE_DL_LIBS})
    target_compile_options(${probe} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${probe} PRIVATE -fsanitize=address)
    set_target_properties(${probe} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CHERYL_BUILD_ROOT}"
        CXX_VISIBILITY_PRESET hidden)
endforeach()
target_sources(native-lsan-override PRIVATE override.cpp)
add_test(NAME native-glfw-lsan
    COMMAND "${CMAKE_COMMAND}"
        "-DDEFAULT_PROBE=$<TARGET_FILE:native-lsan-default>"
        "-DOVERRIDE_PROBE=$<TARGET_FILE:native-lsan-override>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check.cmake")
set_tests_properties(native-glfw-lsan PROPERTIES
    SKIP_REGULAR_EXPRESSION "LSAN_PROBE_UNAVAILABLE"
    TIMEOUT 45)
