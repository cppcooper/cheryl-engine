# Embed a repository-owned, licensed fallback rather than a machine-specific path.
set(font_fallback_file "${CHERYL_REPOSITORY_ROOT}/assets/fonts/DejaVuSans.ttf")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${font_fallback_file}")
file(READ "${font_fallback_file}" font_fallback_hex HEX)
string(REGEX REPLACE "(..)" "0x\\1," font_fallback_values "${font_fallback_hex}")
string(REGEX REPLACE "(................................................................................)" "\\1\n"
    font_fallback_values "${font_fallback_values}")
set(font_fallback_source "${CMAKE_CURRENT_BINARY_DIR}/generated/font-fallback.cpp")
file(GENERATE OUTPUT "${font_fallback_source}" CONTENT
"#include \"font-fallback-internal.h\"
namespace CE::Text::Detail {
    namespace {
        constexpr unsigned char fallback[] = {
${font_fallback_values}
        };
    }
    std::span<const unsigned char> builtin_font_bytes() noexcept { return fallback; }
}
")
set_source_files_properties("${font_fallback_source}" PROPERTIES GENERATED TRUE)
target_sources(${TARGET_LIB_ENGINE} PRIVATE "${font_fallback_source}")
target_include_directories(${TARGET_LIB_ENGINE} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/text")
