cmake_minimum_required(VERSION 3.28)
project(cheryl_docs LANGUAGES NONE)

get_filename_component(CHERYL_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${CHERYL_REPOSITORY_ROOT}/cmake/CherylDocumentation.cmake")
cheryl_add_documentation_targets("${CHERYL_REPOSITORY_ROOT}")
