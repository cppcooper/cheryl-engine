#pragma once

#include <assets/definitions/manifest.h>
#include <assets/definitions/shader-assets.h>
#include <nlohmann/json.hpp>
#include <istream>
#include <string_view>

namespace CE::Assets::ManifestDetail {
    [[nodiscard]] std::filesystem::path graphics_directory(const std::filesystem::path& source);
    [[nodiscard]] std::filesystem::path graphics_path(
        const std::filesystem::path& source, std::string_view reference, std::string_view location
    );
    [[nodiscard]] AssetManifest parse_asset(const nlohmann::json& root, const std::filesystem::path& source);
    [[nodiscard]] ShaderAssetManifest parse_shader(const nlohmann::json& root, const std::filesystem::path& source);
    [[nodiscard]] nlohmann::json read_document(std::istream& input, const std::filesystem::path& source);
}
