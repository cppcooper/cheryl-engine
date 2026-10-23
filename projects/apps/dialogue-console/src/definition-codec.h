#pragma once

#include "gate.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GateConsole {
    inline constexpr std::size_t maximum_definition_bytes = 64 * 1024;
    inline constexpr std::size_t maximum_definition_nodes = 256;
    inline constexpr std::size_t maximum_definition_choices = 2048;
    inline constexpr std::size_t maximum_definition_id_bytes = 128;
    inline constexpr std::size_t maximum_definition_text_bytes = 4096;
    inline constexpr std::size_t maximum_definition_total_text_bytes = 32 * 1024;
    inline constexpr std::size_t maximum_definition_depth = 5;

    // Structural/read failures carry one owned location. Compilation failures
    // retain EVERY original engine diagnostic, with authored indices and values.
    struct DefinitionError {
        std::string origin;
        std::string field;
        std::string message;
        std::optional<std::size_t> node;
        std::optional<std::size_t> choice;
        std::vector<D::Diagnostic> diagnostics;
    };

    std::expected<D::Draft, DefinitionError> decode_definition(std::string_view bytes, std::string origin);
    std::expected<D::DefinitionHandle, DefinitionError> compile_definition(std::string_view bytes, std::string origin);
    std::expected<D::DefinitionHandle, DefinitionError> load_definition(const std::string& path);
    std::string format_definition_error(const DefinitionError& error);

    namespace Testing {
        // Lower only this one budget to exercise its guard within the fixed byte
        // cap: 2048 even empty complete choice objects cannot fit in 64 KiB.
        std::expected<D::Draft, DefinitionError>
        decode_definition_choice_budget(std::string_view bytes, std::string origin, std::size_t budget);
    }
}
