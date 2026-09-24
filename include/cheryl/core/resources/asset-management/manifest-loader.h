#pragma once

#include <assets/manifest.h>

#include <filesystem>
#include <istream>

namespace CE::Assets {
    /** Parse one schema 1.0 document into backend-independent definitions. Document-local
     * references are checked here; duplicate IDs across documents and image bounds are checked
     * by Loader after it has collected the full asset set.
     */
    struct ManifestLoader {
        [[nodiscard]] static AssetManifest load(const std::filesystem::path& file);
        [[nodiscard]] static AssetManifest parse(std::istream& input, const std::filesystem::path& source);
    };
}
