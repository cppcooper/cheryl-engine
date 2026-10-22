include_guard(GLOBAL)

function(cheryl_runtime_executables directory result)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    set(executables)
    foreach(target IN LISTS targets)
        get_target_property(kind "${target}" TYPE)
        if(kind STREQUAL "EXECUTABLE")
            list(APPEND executables "${target}")
        endif()
    endforeach()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        cheryl_runtime_executables("${child}" child_executables)
        list(APPEND executables ${child_executables})
    endforeach()
    set(${result} "${executables}" PARENT_SCOPE)
endfunction()

# Called after composition regardless of SDK installation. Owned shared targets
# and project executables use this runtime directory on Windows. Package DLLs
# must be available there even when an individual library is built directly.
function(cheryl_stage_shared_runtimes)
    if(NOT WIN32)
        return()
    endif()
    get_property(scheduled GLOBAL PROPERTY CHERYL_RUNTIME_FINALIZER_SCHEDULED)
    if(NOT scheduled)
        set_property(GLOBAL PROPERTY CHERYL_RUNTIME_FINALIZER_SCHEDULED TRUE)
        cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL cheryl_finalize_shared_runtimes)
    endif()
endfunction()

function(cheryl_finalize_shared_runtimes)
    if(NOT WIN32)
        return()
    endif()
    get_property(dependencies GLOBAL PROPERTY CHERYL_SHARED_DEPENDENCIES)
    get_property(components GLOBAL PROPERTY CHERYL_AVAILABLE_COMPONENTS)
    set(libraries)
    foreach(entry IN LISTS dependencies)
        string(REPLACE "|" ";" fields "${entry}")
        list(GET fields 0 dependency)
        get_target_property(system "${dependency}" CHERYL_SHARED_DEPENDENCY_SYSTEM)
        get_target_property(imported "${dependency}" IMPORTED)
        get_target_property(kind "${dependency}" TYPE)
        if(imported AND NOT system AND NOT kind STREQUAL "INTERFACE_LIBRARY")
            list(APPEND libraries "$<TARGET_FILE:${dependency}>")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES libraries)
    cheryl_runtime_executables("${CMAKE_SOURCE_DIR}" executables)
    set(directories)
    foreach(target IN LISTS components executables)
        get_target_property(kind "${target}" TYPE)
        if(NOT kind STREQUAL "INTERFACE_LIBRARY")
            list(APPEND directories "$<TARGET_FILE_DIR:${target}>")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES directories)
    set(script "${CMAKE_BINARY_DIR}/runtime-staging/$<CONFIG>/packages.cmake")
    string(CONCAT contents
        "include(\"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CherylRuntimeDependencies.cmake\")\n"
        "set(libraries \"${libraries}\")\n"
        "cheryl_runtime_dependencies(libraries \${libraries})\n"
        "set(directories \"${directories}\")\n"
        "foreach(directory IN LISTS directories)\n"
        "  file(MAKE_DIRECTORY \"\${directory}\")\n"
        "  file(LOCK \"\${directory}/.cheryl-runtime-stage.lock\" GUARD PROCESS TIMEOUT 120)\n"
        "  foreach(library IN LISTS libraries)\n"
        "    get_filename_component(name \"\${library}\" NAME)\n"
        "    set(destination \"\${directory}/\${name}\")\n"
        "    if(NOT library STREQUAL destination)\n"
        "      file(COPY_FILE \"\${library}\" \"\${destination}\" ONLY_IF_DIFFERENT)\n"
        "    endif()\n"
        "  endforeach()\n"
        "  file(LOCK \"\${directory}/.cheryl-runtime-stage.lock\" RELEASE)\n"
        "endforeach()\n")
    if(libraries)
        file(GENERATE OUTPUT "${script}" CONTENT "${contents}")
        add_custom_target(cheryl-runtime-stage
            COMMAND "${CMAKE_COMMAND}" -P "${script}" VERBATIM)
        foreach(target IN LISTS components executables)
            add_dependencies("${target}" cheryl-runtime-stage)
        endforeach()
    endif()
    foreach(target IN LISTS executables)
        # Standalone compositions may put their bootstrapped Engine in a sibling
        # directory. Inspect its DLL imports too: supplied shared targets can hide
        # private native dependencies from CMake's target graph.
        set(script "${CMAKE_BINARY_DIR}/runtime-staging/$<CONFIG>/${target}.cmake")
        string(CONCAT contents
            "include(\"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CherylRuntimeDependencies.cmake\")\n"
            "set(libraries \"$<TARGET_RUNTIME_DLLS:${target}>\")\n"
            "if(libraries)\n"
            "  cheryl_runtime_dependencies(libraries \${libraries})\n"
            "endif()\n"
            "set(directory \"$<TARGET_FILE_DIR:${target}>\")\n"
            "file(MAKE_DIRECTORY \"\${directory}\")\n"
            "file(LOCK \"\${directory}/.cheryl-runtime-stage.lock\" GUARD PROCESS TIMEOUT 120)\n"
            "foreach(library IN LISTS libraries)\n"
            "  get_filename_component(name \"\${library}\" NAME)\n"
            "  set(destination \"\${directory}/\${name}\")\n"
            "  if(NOT library STREQUAL destination)\n"
            "    file(COPY_FILE \"\${library}\" \"\${destination}\" ONLY_IF_DIFFERENT)\n"
            "  endif()\n"
            "endforeach()\n")
        file(GENERATE OUTPUT "${script}" CONTENT "${contents}")
        string(MAKE_C_IDENTIFIER "cheryl_runtime_${target}" staging_target)
        add_custom_target("${staging_target}"
            COMMAND "${CMAKE_COMMAND}" -P "${script}"
            DEPENDS "$<TARGET_RUNTIME_DLLS:${target}>" VERBATIM)
        add_dependencies("${target}" "${staging_target}")
    endforeach()
endfunction()
