#pragma once

#include <assets/manifest.h>

#include <filesystem>
#include <istream>

namespace CE::Assets {
    struct ManifestLoader {
        [[nodiscard]] static AssetManifest load(const std::filesystem::path& file);
        [[nodiscard]] static AssetManifest parse(std::istream& input,
                                                 const std::filesystem::path& source);
    };
}
