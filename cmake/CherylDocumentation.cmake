include_guard(GLOBAL)

function(cheryl_add_documentation_targets repository_root)
    if(TARGET docs)
        return()
    endif()
    if(CHERYL_DOCS_PYTHON)
        set(Python3_EXECUTABLE "${CHERYL_DOCS_PYTHON}")
    endif()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    set(CHERYL_DOCS_PYTHON "${Python3_EXECUTABLE}" CACHE FILEPATH "Python interpreter for documentation tool selection")
    set(CHERYL_DOCS_CACHE_DIR "" CACHE PATH "Shared documentation tool cache; empty uses the user cache")
    option(CHERYL_DOCS_FORCE_PINNED "Use locked documentation tools instead of compatible installed tools" OFF)

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
        COMMENT "Generating Cheryl documentation"
        USES_TERMINAL VERBATIM)
endfunction()
