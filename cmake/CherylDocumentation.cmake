include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CherylOptions.cmake")

function(cheryl_add_documentation_targets repository_root)
    if(TARGET docs)
        return()
    endif()
    if(CHERYL_DOCS_PYTHON)
        set(Python3_EXECUTABLE "${CHERYL_DOCS_PYTHON}")
    endif()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    cheryl_configure_documentation_options("${Python3_EXECUTABLE}")

    set(tool_arguments)
    if(CHERYL_DOCS_CACHE_DIR)
        list(APPEND tool_arguments --cache-dir "${CHERYL_DOCS_CACHE_DIR}")
    endif()
    if(CHERYL_DOCS_FORCE_PINNED)
        list(APPEND tool_arguments --force-pinned)
    endif()
    add_custom_target(docs-tools
        COMMAND "${CHERYL_DOCS_PYTHON}" "${repository_root}/docs/generation/bootstrap.py" setup ${tool_arguments}
        WORKING_DIRECTORY "${repository_root}"
        COMMENT "Preparing shared documentation tools"
        USES_TERMINAL VERBATIM)
    add_custom_target(docs
        COMMAND "${CHERYL_DOCS_PYTHON}" "${repository_root}/docs/generation/bootstrap.py" exec ${tool_arguments}
            -- python "${repository_root}/docs/generation/build.py"
        WORKING_DIRECTORY "${repository_root}"
        COMMENT "Updating Cheryl documentation"
        USES_TERMINAL VERBATIM)
    add_custom_target(docs-clean
        COMMAND "${CHERYL_DOCS_PYTHON}" "${repository_root}/docs/generation/bootstrap.py" exec ${tool_arguments}
            -- python "${repository_root}/docs/generation/build.py" --clean
        WORKING_DIRECTORY "${repository_root}"
        COMMENT "Regenerating Cheryl documentation from scratch"
        USES_TERMINAL VERBATIM)
endfunction()
