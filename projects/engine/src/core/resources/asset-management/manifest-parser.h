#pragma once

#include <assets/definitions/manifest.h>
#include <assets/definitions/shader-assets.h>
#include <nlohmann/json.hpp>
#include <istream>

namespace CE::Assets::ManifestDetail {
    [[nodiscard]] AssetManifest parse_asset(const nlohmann::json& root, const std::filesystem::path& source);
    [[nodiscard]] ShaderAssetManifest parse_shader(const nlohmann::json& root, const std::filesystem::path& source);
    [[nodiscard]] nlohmann::json read_document(std::istream& input, const std::filesystem::path& source);
}
